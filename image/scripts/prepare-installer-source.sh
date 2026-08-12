#!/bin/sh

# Build the immutable signed source directory consumed by Northstar installer
# media. The private key remains external; only its derived public key and the
# detached signature enter the output.

set -eu

PAYLOAD=
RUNTIME_MANIFEST=
PROJECT_ROOT=
PROJECT_COMMIT=
SIGNING_KEY=
OUTPUT=
OPENSSL=${NORTHSTAR_INSTALLER_MEDIA_OPENSSL:-openssl}
STAGING=
SUCCESS=0

usage() {
    cat <<'USAGE'
Usage: prepare-installer-source.sh --payload FILE --runtime-manifest FILE \
  --project-root CLEAN_CHECKOUT --project-commit FULL_COMMIT \
  --signing-key EXTERNAL_PRIVATE_KEY --output NEW_DIRECTORY

The payload must be a northstar-rootfs-v1 txz containing the exact runtime
manifest. The private key must resolve outside the project checkout.
USAGE
}

die() { printf 'ERROR: %s\n' "$1" >&2; exit 1; }
cleanup() {
    if [ "$SUCCESS" -ne 1 ] && [ -n "$STAGING" ] && [ -d "$STAGING" ]; then
        rm -rf "$STAGING"
    fi
}
trap cleanup EXIT HUP INT TERM

while [ "$#" -gt 0 ]; do
    case "$1" in
        --payload) PAYLOAD=${2-}; shift 2 ;;
        --runtime-manifest) RUNTIME_MANIFEST=${2-}; shift 2 ;;
        --project-root) PROJECT_ROOT=${2-}; shift 2 ;;
        --project-commit) PROJECT_COMMIT=${2-}; shift 2 ;;
        --signing-key) SIGNING_KEY=${2-}; shift 2 ;;
        --output) OUTPUT=${2-}; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

for command_name in awk basename cp cut dirname git grep mkdir mktemp mv rm tar tr wc "$OPENSSL"; do
    command -v "$command_name" >/dev/null 2>&1 \
        || die "required command is unavailable: $command_name"
done
if command -v sha256 >/dev/null 2>&1; then SHA256_COMMAND=sha256
elif command -v sha256sum >/dev/null 2>&1; then SHA256_COMMAND=sha256sum
else die 'sha256 or sha256sum is required'; fi

file_sha256() {
    if [ "$SHA256_COMMAND" = sha256 ]; then sha256 -q "$1"; else sha256sum "$1" | awk '{ print $1 }'; fi
}
file_size() { wc -c < "$1" | tr -d ' '; }
canonical_file() {
    directory=$(CDPATH= cd -- "$(dirname "$1")" && pwd -P)
    printf '%s/%s\n' "$directory" "$(basename "$1")"
}

[ -f "$PAYLOAD" ] && [ ! -L "$PAYLOAD" ] && [ -r "$PAYLOAD" ] \
    || die 'payload must be a readable regular file'
[ -f "$RUNTIME_MANIFEST" ] && [ ! -L "$RUNTIME_MANIFEST" ] && [ -r "$RUNTIME_MANIFEST" ] \
    || die 'runtime manifest must be a readable regular file'
[ -d "$PROJECT_ROOT/.git" ] && [ ! -L "$PROJECT_ROOT" ] || die 'project root must be a real Git checkout'
[ -f "$SIGNING_KEY" ] && [ ! -L "$SIGNING_KEY" ] && [ -r "$SIGNING_KEY" ] \
    || die 'signing key must be a readable regular file'
[ -n "$OUTPUT" ] && [ ! -e "$OUTPUT" ] && [ ! -L "$OUTPUT" ] || die 'output must not exist'
printf '%s\n' "$PROJECT_COMMIT" | grep -Eq '^[0-9a-f]{40}$' || die 'project commit must be a full lowercase Git revision'

PROJECT_ROOT=$(CDPATH= cd -- "$PROJECT_ROOT" && pwd -P)
signing_key_path=$(canonical_file "$SIGNING_KEY")
case "$signing_key_path" in "$PROJECT_ROOT"|"$PROJECT_ROOT"/*)
    die 'private signing key must remain outside the project checkout' ;;
esac
[ "$(git -C "$PROJECT_ROOT" rev-parse HEAD)" = "$PROJECT_COMMIT" ] \
    || die 'project checkout HEAD does not match the requested commit'
[ -z "$(git -C "$PROJECT_ROOT" status --porcelain)" ] || die 'project checkout must be clean'
"$OPENSSL" pkey -in "$SIGNING_KEY" -noout >/dev/null 2>&1 || die 'signing key is not a supported private key'

payload_name=$(basename "$PAYLOAD")
printf '%s\n' "$payload_name" | grep -Eq '^northstar-rootfs-v1-[0-9a-f]{12}\.txz$' \
    || die 'payload name must be northstar-rootfs-v1-<12 commit hex>.txz'
[ "$payload_name" = "northstar-rootfs-v1-$(printf '%s' "$PROJECT_COMMIT" | cut -c1-12).txz" ] \
    || die 'payload name does not match the project commit'
payload_size=$(file_size "$PAYLOAD")
printf '%s\n' "$payload_size" | grep -Eq '^[0-9]{1,12}$' || die 'payload size is outside the installer bound'
[ "$payload_size" -gt 0 ] && [ "$payload_size" -le 137438953472 ] || die 'payload size is outside the installer bound'
payload_sha256=$(file_sha256 "$PAYLOAD" | tr '[:upper:]' '[:lower:]')
runtime_sha256=$(file_sha256 "$RUNTIME_MANIFEST" | tr '[:upper:]' '[:lower:]')

archive_runtime=$(
    tar -xOf "$PAYLOAD" ./var/db/northstar/runtime-manifest.conf 2>/dev/null \
        || tar -xOf "$PAYLOAD" var/db/northstar/runtime-manifest.conf 2>/dev/null
) || die 'payload omits var/db/northstar/runtime-manifest.conf'
archive_runtime_file=$(mktemp "${TMPDIR:-/tmp}/northstar-runtime-manifest.XXXXXX")
trap 'rm -f "$archive_runtime_file"; cleanup' EXIT HUP INT TERM
printf '%s\n' "$archive_runtime" > "$archive_runtime_file"
[ "$(file_sha256 "$archive_runtime_file" | tr '[:upper:]' '[:lower:]')" = "$runtime_sha256" ] \
    || die 'payload runtime manifest does not match the reviewed manifest'

output_parent=$(dirname "$OUTPUT")
mkdir -p "$output_parent"
output_parent=$(CDPATH= cd -- "$output_parent" && pwd -P)
OUTPUT=$output_parent/$(basename "$OUTPUT")
STAGING=$(mktemp -d "$output_parent/.northstar-installer-source.XXXXXX")
cp "$PAYLOAD" "$STAGING/$payload_name"
cp "$RUNTIME_MANIFEST" "$STAGING/runtime-manifest.conf"
"$OPENSSL" pkey -in "$SIGNING_KEY" -pubout -out "$STAGING/source-signing.pem" >/dev/null 2>&1
cat > "$STAGING/source-manifest.conf" <<EOF
schema_version=2
product=Northstar
freebsd_release=15.1-RELEASE
architecture=amd64
project_commit=$PROJECT_COMMIT
payload_kind=northstar-rootfs-v1
payload_name=$payload_name
payload_size=$payload_size
payload_sha256=$payload_sha256
runtime_manifest_sha256=$runtime_sha256
EOF
"$OPENSSL" dgst -sha256 -sign "$SIGNING_KEY" \
    -out "$STAGING/source-manifest.conf.sig" "$STAGING/source-manifest.conf"
"$OPENSSL" dgst -sha256 -verify "$STAGING/source-signing.pem" \
    -signature "$STAGING/source-manifest.conf.sig" "$STAGING/source-manifest.conf" >/dev/null 2>&1 \
    || die 'detached source signature did not verify'
manifest_sha256=$(file_sha256 "$STAGING/source-manifest.conf" | tr '[:upper:]' '[:lower:]')
signature_sha256=$(file_sha256 "$STAGING/source-manifest.conf.sig" | tr '[:upper:]' '[:lower:]')
trust_key_sha256=$(file_sha256 "$STAGING/source-signing.pem" | tr '[:upper:]' '[:lower:]')
cat > "$STAGING/installer-source-provenance.conf" <<EOF
schema_version=1
product=Northstar
source_manifest_sha256=$manifest_sha256
source_signature_sha256=$signature_sha256
source_trust_key_sha256=$trust_key_sha256
payload_name=$payload_name
payload_size=$payload_size
payload_sha256=$payload_sha256
runtime_manifest_sha256=$runtime_sha256
project_commit=$PROJECT_COMMIT
private_key_included=no
EOF
rm -f "$archive_runtime_file"
chmod 0444 "$STAGING"/*
mv "$STAGING" "$OUTPUT"
STAGING=
SUCCESS=1
printf 'PASS: prepared signed Northstar installer source at %s\n' "$OUTPUT"
