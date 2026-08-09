#!/bin/sh

# Native M4 package-publication smoke gate. This creates a disposable FreeBSD
# pkg repository from one already-installed package, signs the catalogue with
# the documented external-signer contract, and optionally asks pkg to accept
# it through an isolated client database.

set -eu

CLIENT=0
FIXTURE_PACKAGE=${NORTHSTAR_PKG_FIXTURE_PACKAGE:-}
TMP_DIR=

usage() {
    cat <<USAGE
Usage: $0 [--client]

Create and validate a temporary signed FreeBSD pkg repository. The default
mode validates publication structure only and runs as the current user.
--client additionally runs an isolated pkg update against the repository;
because pkg rejects user-owned file:// repositories, --client must be run as
root (for example: sudo -n make pkg-repository-smoke NORTHSTAR_PKG_CLIENT=1).
No system pkg database, repository configuration, package installation, or
signing key is retained.
USAGE
}

die() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

cleanup() {
    if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
        rm -rf "$TMP_DIR"
    fi
}
trap cleanup EXIT HUP INT TERM

while [ "$#" -gt 0 ]; do
    case "$1" in
        --client)
            CLIENT=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            printf 'ERROR: unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

[ "$(uname -s)" = FreeBSD ] || die 'pkg repository smoke requires FreeBSD'

for command_name in awk chmod find grep mktemp openssl pkg sha256 tar; do
    command -v "$command_name" >/dev/null 2>&1 || {
        die "required command is unavailable: $command_name"
    }
done

if [ "$CLIENT" -eq 1 ] && [ "$(id -u)" -ne 0 ]; then
    die '--client must run as root so pkg can validate a root-owned file:// repository'
fi

if [ -z "$FIXTURE_PACKAGE" ]; then
    FIXTURE_PACKAGE=$(pkg query -a '%n' | awk 'NF { print; exit }') || true
fi
[ -n "$FIXTURE_PACKAGE" ] || die 'no installed package is available as a repository fixture'
pkg info -e "$FIXTURE_PACKAGE" >/dev/null 2>&1 || {
    die "fixture package is not installed: $FIXTURE_PACKAGE"
}

TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-pkg-repository.XXXXXX")
mkdir -p \
    "$TMP_DIR/packages" \
    "$TMP_DIR/repo" \
    "$TMP_DIR/repos" \
    "$TMP_DIR/fingerprints/trusted"

printf 'Using installed fixture package: %s\n' "$FIXTURE_PACKAGE"
pkg create -q -o "$TMP_DIR/packages" "$FIXTURE_PACKAGE"

package_path=$(find "$TMP_DIR/packages" -type f -name '*.pkg' -print -quit)
[ -n "$package_path" ] || die 'pkg create did not produce a package archive'

openssl genrsa -out "$TMP_DIR/repo.key" 2048 >/dev/null 2>&1
chmod 0400 "$TMP_DIR/repo.key"
openssl rsa -in "$TMP_DIR/repo.key" -pubout -out "$TMP_DIR/repo.pub" >/dev/null 2>&1

# FreeBSD pkg repo passes the SHA256 catalogue digest on stdin. Keep the
# private key in this disposable directory to model an external signing
# command without committing or installing a project key.
cat > "$TMP_DIR/sign.sh" <<SIGNER
#!/bin/sh
read -t 2 sum
[ -n "\$sum" ] || exit 1
echo SIGNATURE
echo -n "\$sum" | /usr/bin/openssl dgst -sign "$TMP_DIR/repo.key" -sha256 -binary
echo
echo CERT
cat "$TMP_DIR/repo.pub"
echo END
SIGNER
chmod 0700 "$TMP_DIR/sign.sh"

pkg repo -q -o "$TMP_DIR/repo" "$TMP_DIR/packages" \
    signing_command: "$TMP_DIR/sign.sh"

[ -s "$TMP_DIR/repo/meta.conf" ] || die 'pkg repo did not produce meta.conf'
[ -s "$TMP_DIR/repo/data.pkg" ] || die 'pkg repo did not produce data.pkg'
grep -Eq 'version[[:space:]]*[:=][[:space:]]*2' "$TMP_DIR/repo/meta.conf" || {
    die 'repository metadata is not FreeBSD pkg repository format v2'
}

catalogue_entries=$(tar -tf "$TMP_DIR/repo/data.pkg")
for entry in data data.sig data.pub; do
    printf '%s\n' "$catalogue_entries" | grep -Fx "$entry" >/dev/null 2>&1 || {
        die "data.pkg is missing signed catalogue member: $entry"
    }
done

fingerprint=$(sha256 -q "$TMP_DIR/repo.pub")
printf '%s\n' \
    'function: sha256' \
    "fingerprint: \"$fingerprint\"" \
    > "$TMP_DIR/fingerprints/trusted/northstar"

printf 'PASS: pkg repo produced v2 meta.conf, data.pkg, data.sig, and data.pub\n'
printf 'PASS: external RSA signer fingerprint is %s\n' "$fingerprint"

if [ "$CLIENT" -eq 0 ]; then
    printf '%s\n' 'INFO: client verification skipped; rerun with --client as root to exercise pkg update'
    exit 0
fi

mkdir -p "$TMP_DIR/db" "$TMP_DIR/cache"
printf '%s\n' \
    'northstar: {' \
    "    url: \"file://$TMP_DIR/repo\"," \
    '    mirror_type: "none",' \
    '    signature_type: "FINGERPRINTS",' \
    "    fingerprints: \"$TMP_DIR/fingerprints\"," \
    '    enabled: yes' \
    '}' \
    > "$TMP_DIR/repos/northstar.conf"

client_log=$TMP_DIR/pkg-client.log
if ! PKG_DBDIR="$TMP_DIR/db" \
    PKG_CACHEDIR="$TMP_DIR/cache" \
    pkg -o REPOS_DIR="$TMP_DIR/repos" \
        -o ASSUME_ALWAYS_YES=yes \
        update -f > "$client_log" 2>&1; then
    cat "$client_log" >&2
    die 'pkg client rejected the signed repository'
fi
cat "$client_log"
printf '%s\n' 'PASS: isolated pkg client accepted the FINGERPRINTS-signed repository'
