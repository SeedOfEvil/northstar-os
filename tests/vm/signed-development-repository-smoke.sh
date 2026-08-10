#!/bin/sh

# Publish Northstar's real CPack artifact through the development-channel
# contract, refresh it with an isolated pkg client, and prove tampering fails.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
PACKAGE_PATH=${NORTHSTAR_PACKAGE_PATH:-}
SOURCE_REVISION=${NORTHSTAR_SOURCE_REVISION:-}
PORTS_BRANCH=${NORTHSTAR_PORTS_BRANCH:-2026Q3}
PORTS_COMMIT=${NORTHSTAR_PORTS_COMMIT:-}
TMP_DIR=

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

[ "$(uname -s)" = FreeBSD ] || die 'signed development repository smoke requires FreeBSD'
[ "$(id -u)" -eq 0 ] || die 'isolated file repository client validation must run as root'

if [ -z "$PACKAGE_PATH" ]; then
    PACKAGE_PATH=$(find "$ROOT/build" -maxdepth 1 -type f -name 'northstar-*.pkg' -print | sort | tail -1)
fi
[ -f "$PACKAGE_PATH" ] || die 'Northstar CPack package is missing; run make package first'
pkg query -F "$PACKAGE_PATH" '%n' | grep -Fx northstar >/dev/null 2>&1 \
    || die 'package artifact is not the Northstar package'

if [ -z "$SOURCE_REVISION" ] && command -v git >/dev/null 2>&1 && [ -d "$ROOT/.git" ]; then
    SOURCE_REVISION=$(git -C "$ROOT" rev-parse HEAD)
fi
printf '%s\n' "$SOURCE_REVISION" | grep -Eq '^[0-9A-Fa-f]{7,64}$' \
    || die 'NORTHSTAR_SOURCE_REVISION must identify the immutable source commit'

qt_version=$(pkg query -e '%n = qt6-base' '%v')
wayfire_version=$(pkg query -e '%n = wayfire' '%v')
[ -n "$qt_version" ] && [ -n "$wayfire_version" ] || die 'dependency versions are unavailable'
if [ -z "$PORTS_COMMIT" ]; then
    command -v git >/dev/null 2>&1 || die 'git is required to resolve the Ports branch for the smoke gate'
    PORTS_COMMIT=$(git ls-remote https://git.FreeBSD.org/ports.git "refs/heads/$PORTS_BRANCH" \
        | awk 'NR == 1 { print $1 }')
fi
printf '%s\n' "$PORTS_COMMIT" | grep -Eq '^[0-9A-Fa-f]{40}$' \
    || die 'NORTHSTAR_PORTS_COMMIT must be an exact FreeBSD Ports commit'

TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-signed-development.XXXXXX")
mkdir -p "$TMP_DIR/packages" "$TMP_DIR/client-db" "$TMP_DIR/client-cache" \
    "$TMP_DIR/tampered-db" "$TMP_DIR/tampered-cache" "$TMP_DIR/repos"
cp -p "$PACKAGE_PATH" "$TMP_DIR/packages/"

openssl genrsa -out "$TMP_DIR/development.key" 2048 >/dev/null 2>&1
chmod 0400 "$TMP_DIR/development.key"
openssl rsa -in "$TMP_DIR/development.key" -pubout -out "$TMP_DIR/development.pub" >/dev/null 2>&1

cat > "$TMP_DIR/pkg-signer" <<SIGNER
#!/bin/sh
read -t 2 sum
[ -n "\$sum" ] || exit 1
echo SIGNATURE
echo -n "\$sum" | /usr/bin/openssl dgst -sign "$TMP_DIR/development.key" -sha256 -binary
echo
echo CERT
cat "$TMP_DIR/development.pub"
echo END
SIGNER
chmod 0700 "$TMP_DIR/pkg-signer"

cat > "$TMP_DIR/publication-signer" <<SIGNER
#!/bin/sh
set -eu
[ "\$#" -eq 2 ] || exit 2
/usr/bin/openssl dgst -sha256 -sign "$TMP_DIR/development.key" -out "\$2" "\$1"
SIGNER
chmod 0700 "$TMP_DIR/publication-signer"

freebsd_release=$(freebsd-version -u)
cat > "$TMP_DIR/upstream.lock" <<LOCK
FREEBSD_RELEASE=$freebsd_release
FREEBSD_ARCH=$(uname -m)
PORTS_BRANCH=$PORTS_BRANCH
PORTS_COMMIT=$PORTS_COMMIT
QT_PACKAGE_VERSION=$qt_version
WAYFIRE_PACKAGE_VERSION=$wayfire_version
PROJECT_COMMIT=$SOURCE_REVISION
LOCK

generated_at=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
"$ROOT/tools/publish-development-repository.sh" \
    --packages "$TMP_DIR/packages" \
    --output "$TMP_DIR/repository" \
    --pkg-signer "$TMP_DIR/pkg-signer" \
    --publication-signer "$TMP_DIR/publication-signer" \
    --public-key "$TMP_DIR/development.pub" \
    --source-lock "$TMP_DIR/upstream.lock" \
    --revision 73 \
    --source-revision "$SOURCE_REVISION" \
    --generated-at "$generated_at" \
    --repository-url pkg+https://packages.example.test/northstar/development \
    --fingerprints-path "$TMP_DIR/repository/fingerprints"

for artifact in meta.conf data.pkg repository-metadata.json signature.json \
    publication-record.conf repository-policy.conf northstar-development.conf; do
    [ -s "$TMP_DIR/repository/$artifact" ] || die "publication artifact is missing: $artifact"
done
find "$TMP_DIR/repository" -type f \( -name '*.key' -o -name '*private*' \) | grep . >/dev/null 2>&1 \
    && die 'private key material escaped into the publication'

printf '%s\n' \
    'northstar-development: {' \
    "    url: \"file://$TMP_DIR/repository\"," \
    '    mirror_type: "none",' \
    '    signature_type: "FINGERPRINTS",' \
    "    fingerprints: \"$TMP_DIR/repository/fingerprints\"," \
    '    enabled: yes' \
    '}' > "$TMP_DIR/repos/northstar.conf"

PKG_DBDIR="$TMP_DIR/client-db" PKG_CACHEDIR="$TMP_DIR/client-cache" \
    pkg -o REPOS_DIR="$TMP_DIR/repos" -o ASSUME_ALWAYS_YES=yes update -f \
    > "$TMP_DIR/client.log" 2>&1 \
    || { cat "$TMP_DIR/client.log" >&2; die 'isolated pkg client rejected the signed development repository'; }

cp -Rp "$TMP_DIR/repository" "$TMP_DIR/tampered-repository"
catalogue_number=0
for catalogue_name in data packagesite filesite; do
    catalogue_path="$TMP_DIR/tampered-repository/$catalogue_name.pkg"
    [ -f "$catalogue_path" ] || continue
    catalogue_number=$((catalogue_number + 1))
    catalogue_dir="$TMP_DIR/tampered-catalogue-$catalogue_number"
    mkdir "$catalogue_dir"
    tar -xf "$catalogue_path" -C "$catalogue_dir"
    signature_path=$(find "$catalogue_dir" -type f -name '*.sig' -print -quit)
    [ -n "$signature_path" ] || die "signed catalogue has no signature member: $catalogue_name.pkg"
    printf X | dd of="$signature_path" bs=1 seek=0 conv=notrunc >/dev/null 2>&1
    member_list=$(find "$catalogue_dir" -maxdepth 1 -type f -print | sort)
    (
        cd "$catalogue_dir"
        # shellcheck disable=SC2086
        tar -caf "$catalogue_path" $(printf '%s\n' "$member_list" | sed 's|.*/||')
    )
done
[ "$catalogue_number" -gt 0 ] || die 'no signed catalogue archive was available to alter'
printf '%s\n' \
    'northstar-development: {' \
    "    url: \"file://$TMP_DIR/tampered-repository\"," \
    '    mirror_type: "none",' \
    '    signature_type: "FINGERPRINTS",' \
    "    fingerprints: \"$TMP_DIR/tampered-repository/fingerprints\"," \
    '    enabled: yes' \
    '}' > "$TMP_DIR/repos/northstar.conf"

if PKG_DBDIR="$TMP_DIR/tampered-db" PKG_CACHEDIR="$TMP_DIR/tampered-cache" \
    pkg -o REPOS_DIR="$TMP_DIR/repos" -o ASSUME_ALWAYS_YES=yes update -f \
    > "$TMP_DIR/tampered.log" 2>&1; then
    die 'isolated pkg client accepted altered signed metadata'
fi

printf '%s\n' 'PASS: isolated pkg client trusted and refreshed the signed development channel'
printf '%s\n' 'PASS: altered signed catalogue metadata was rejected'
printf '%s\n' 'PASS: publication contains package provenance and no private signing key'
