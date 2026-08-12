#!/bin/sh

set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
VERIFY=$ROOT/apps/installer/northstar-installer-source-verify
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-installer-source.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM
SOURCE=$TMP_DIR/source
BIN=$TMP_DIR/bin
PRIVATE_KEY=$TMP_DIR/source-signing-private.pem
PUBLIC_KEY=$TMP_DIR/source-signing.pem
mkdir -p "$SOURCE" "$BIN"
fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }

OPENSSL=$(command -v openssl || true)
[ -n "$OPENSSL" ] || { printf '%s\n' 'SKIP: openssl is required for installer-source verification'; exit 0; }

cat > "$BIN/sha256" <<'EOF'
#!/bin/sh
[ "${1:-}" = -q ] && shift
if command -v sha256sum >/dev/null 2>&1; then
    if [ "$#" -eq 1 ]; then sha256sum "$1"; else sha256sum; fi | awk '{ print $1 }'
else
    exec /sbin/sha256 -q "$@"
fi
EOF
chmod +x "$BIN/sha256"

"$OPENSSL" genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 \
    -out "$PRIVATE_KEY" >/dev/null 2>&1
"$OPENSSL" pkey -in "$PRIVATE_KEY" -pubout -out "$PUBLIC_KEY" >/dev/null 2>&1
printf '%s\n' 'Northstar signed installer payload fixture' > "$SOURCE/northstar-runtime.txz"

write_manifest() {
    payload_size=$(wc -c < "$SOURCE/northstar-runtime.txz" | tr -d ' ')
    payload_sha=$("$BIN/sha256" -q "$SOURCE/northstar-runtime.txz")
    cat > "$SOURCE/source-manifest.conf" <<EOF
schema_version=1
product=Northstar
freebsd_release=15.1-RELEASE
architecture=amd64
project_commit=0123456789abcdef0123456789abcdef01234567
payload_name=northstar-runtime.txz
payload_size=$payload_size
payload_sha256=$payload_sha
runtime_manifest_sha256=abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789
EOF
    "$OPENSSL" dgst -sha256 -sign "$PRIVATE_KEY" \
        -out "$SOURCE/source-manifest.conf.sig" "$SOURCE/source-manifest.conf"
}

manifest_digest() { "$BIN/sha256" -q "$SOURCE/source-manifest.conf"; }
run_verify() {
    env NORTHSTAR_INSTALLER_SOURCE_TEST_MODE=1 \
        NORTHSTAR_INSTALLER_SOURCE_ROOT="$SOURCE" \
        NORTHSTAR_INSTALLER_SOURCE_TRUST_KEY="$PUBLIC_KEY" \
        NORTHSTAR_INSTALLER_SOURCE_OPENSSL="$OPENSSL" \
        NORTHSTAR_INSTALLER_SOURCE_SHA256="$BIN/sha256" \
        NORTHSTAR_INSTALLER_SOURCE_STAT=/usr/bin/stat \
        sh "$VERIFY" "$@"
}

write_manifest
EXPECTED=$(manifest_digest)
run_verify --capabilities | grep -Fx 'mutation=none' >/dev/null \
    || fail 'capabilities do not preserve the read-only boundary'
run_verify --verify "$EXPECTED" > "$TMP_DIR/verified.out"
grep -Fx 'SOURCE_VERIFICATION=PASS' "$TMP_DIR/verified.out" >/dev/null \
    || fail 'valid signed source was rejected'
grep -Fx 'PAYLOAD_NAME=northstar-runtime.txz' "$TMP_DIR/verified.out" >/dev/null \
    || fail 'verified payload identity was not reported'
grep -Fx 'SOURCE_MUTATION=none' "$TMP_DIR/verified.out" >/dev/null \
    || fail 'source verifier did not report its no-mutation contract'

if run_verify --verify aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa \
    >/dev/null 2>&1; then
    fail 'a source manifest outside the reviewed plan was accepted'
fi

printf '%s\n' 'tampered' >> "$SOURCE/northstar-runtime.txz"
if run_verify --verify "$EXPECTED" >/dev/null 2>&1; then
    fail 'tampered installer payload was accepted'
fi

printf '%s\n' 'Northstar signed installer payload fixture' > "$SOURCE/northstar-runtime.txz"
write_manifest
EXPECTED=$(manifest_digest)
printf '%s\n' 'not-a-signature' > "$SOURCE/source-manifest.conf.sig"
if run_verify --verify "$EXPECTED" >/dev/null 2>&1; then
    fail 'tampered source signature was accepted'
fi

write_manifest
sed 's#payload_name=northstar-runtime.txz#payload_name=../escape.txz#' \
    "$SOURCE/source-manifest.conf" > "$TMP_DIR/unsafe.manifest"
mv "$TMP_DIR/unsafe.manifest" "$SOURCE/source-manifest.conf"
"$OPENSSL" dgst -sha256 -sign "$PRIVATE_KEY" \
    -out "$SOURCE/source-manifest.conf.sig" "$SOURCE/source-manifest.conf"
EXPECTED=$(manifest_digest)
if run_verify --verify "$EXPECTED" >/dev/null 2>&1; then
    fail 'signed manifest with an unsafe payload path was accepted'
fi

[ ! -e "$SOURCE/source-signing-private.pem" ] \
    || fail 'private signing key entered the installer source root'
printf '%s\n' 'PASS: signed installer source verifies provenance and rejects digest, signature, payload, and path tampering'
