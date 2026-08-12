#!/bin/sh

# Assemble one integrated Northstar installer release candidate from immutable
# image inputs. Production creates only file artifacts on a marked disposable
# FreeBSD builder; no host disk device is accepted by this interface.

set -eu

RESOLVED=
ARTIFACTS=
RUNTIME=
PROJECT_ROOT=
PROJECT_COMMIT=
SIGNING_KEY=
OUTPUT=
IMAGE_MARKER=/etc/northstar/disposable-image-builder.conf
MEDIA_MARKER=/etc/northstar/disposable-installer-media-builder.conf
DISK_SIZE_GB=16
SOURCE_SIZE_GB=4
PREFLIGHT=0
TEST_MODE=${NORTHSTAR_INSTALLER_RC_TEST_MODE:-0}
IMAGE_ASSEMBLER=${NORTHSTAR_INSTALLER_RC_IMAGE_ASSEMBLER:-}
SOURCE_PREPARER=${NORTHSTAR_INSTALLER_RC_SOURCE_PREPARER:-}
MEDIA_ASSEMBLER=${NORTHSTAR_INSTALLER_RC_MEDIA_ASSEMBLER:-}
STAGING=
SUCCESS=0

usage() {
    cat <<'USAGE'
Usage: assemble-installer-rc.sh --resolved-inputs DIR --artifacts DIR \
  --runtime-bundle DIR --project-root CLEAN_CHECKOUT \
  --project-commit FULL_COMMIT --signing-key EXTERNAL_PRIVATE_KEY \
  --output NEW_DIRECTORY [--image-builder-marker FILE] \
  [--media-builder-marker FILE] [--disk-size-gb 16] \
  [--source-size-gb 4] [--preflight]

Production is FreeBSD/root-only and requires two protected disposable-builder
markers. It emits only QCOW2, signed-source, and raw-image files beneath its
new output directory. It never accepts or writes a host disk device.
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
        --resolved-inputs) RESOLVED=${2-}; shift 2 ;;
        --artifacts) ARTIFACTS=${2-}; shift 2 ;;
        --runtime-bundle) RUNTIME=${2-}; shift 2 ;;
        --project-root) PROJECT_ROOT=${2-}; shift 2 ;;
        --project-commit) PROJECT_COMMIT=${2-}; shift 2 ;;
        --signing-key) SIGNING_KEY=${2-}; shift 2 ;;
        --output) OUTPUT=${2-}; shift 2 ;;
        --image-builder-marker) IMAGE_MARKER=${2-}; shift 2 ;;
        --media-builder-marker) MEDIA_MARKER=${2-}; shift 2 ;;
        --disk-size-gb) DISK_SIZE_GB=${2-}; shift 2 ;;
        --source-size-gb) SOURCE_SIZE_GB=${2-}; shift 2 ;;
        --preflight) PREFLIGHT=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
if [ "$TEST_MODE" = 1 ]; then
    [ "$(id -u)" -ne 0 ] || die 'installer RC test mode is forbidden for root'
    [ -n "$IMAGE_ASSEMBLER" ] && [ -n "$SOURCE_PREPARER" ] && [ -n "$MEDIA_ASSEMBLER" ] \
        || die 'test mode requires all three fixed-tool overrides'
else
    IMAGE_ASSEMBLER=$ROOT/image/scripts/assemble-qcow2-image.sh
    SOURCE_PREPARER=$ROOT/image/scripts/prepare-installer-source.sh
    MEDIA_ASSEMBLER=$ROOT/image/scripts/assemble-installer-usb.sh
fi

for command_name in awk basename cat chmod cut dirname git grep id mkdir mktemp mv openssl rm stat tr uname wc; do
    command -v "$command_name" >/dev/null 2>&1 || die "required command is unavailable: $command_name"
done
if command -v sha256 >/dev/null 2>&1; then SHA256_COMMAND=sha256
elif command -v sha256sum >/dev/null 2>&1; then SHA256_COMMAND=sha256sum
else die 'sha256 or sha256sum is required'; fi
file_sha256() { if [ "$SHA256_COMMAND" = sha256 ]; then sha256 -q "$1"; else sha256sum "$1" | awk '{ print $1 }'; fi; }
file_size() { if [ "$(uname -s)" = FreeBSD ]; then stat -f '%z' "$1"; else wc -c < "$1" | tr -d ' '; fi; }
canonical_file() { directory=$(CDPATH= cd -- "$(dirname "$1")" && pwd -P); printf '%s/%s\n' "$directory" "$(basename "$1")"; }

for directory in "$RESOLVED" "$ARTIFACTS" "$RUNTIME"; do
    [ -d "$directory" ] && [ ! -L "$directory" ] || die 'RC input directory is missing or unsafe'
done
[ -d "$PROJECT_ROOT/.git" ] && [ ! -L "$PROJECT_ROOT" ] || die 'project root must be a real Git checkout'
[ -f "$SIGNING_KEY" ] && [ ! -L "$SIGNING_KEY" ] && [ -r "$SIGNING_KEY" ] || die 'signing key must be a readable regular file'
[ -n "$OUTPUT" ] && [ ! -e "$OUTPUT" ] && [ ! -L "$OUTPUT" ] || die 'output must not exist'
printf '%s\n' "$PROJECT_COMMIT" | grep -Eq '^[0-9a-f]{40}$' || die 'project commit must be a full lowercase Git revision'
case "$DISK_SIZE_GB" in ''|*[!0-9]*) die 'disk size must be an integer GiB value' ;; esac
case "$SOURCE_SIZE_GB" in ''|*[!0-9]*) die 'source size must be an integer GiB value' ;; esac
[ "$DISK_SIZE_GB" -ge 12 ] && [ "$DISK_SIZE_GB" -le 64 ] || die 'disk size must be between 12 and 64 GiB'
[ "$SOURCE_SIZE_GB" -ge 2 ] && [ "$SOURCE_SIZE_GB" -le 32 ] || die 'source size must be between 2 and 32 GiB'
PROJECT_ROOT=$(CDPATH= cd -- "$PROJECT_ROOT" && pwd -P)
[ "$(git -C "$PROJECT_ROOT" rev-parse HEAD)" = "$PROJECT_COMMIT" ] || die 'project checkout HEAD does not match project commit'
[ -z "$(git -C "$PROJECT_ROOT" status --porcelain)" ] || die 'project checkout must be clean'
signing_key=$(canonical_file "$SIGNING_KEY")
case "$signing_key" in "$PROJECT_ROOT"|"$PROJECT_ROOT"/*) die 'signing key must remain outside the project checkout' ;; esac
for tool in "$IMAGE_ASSEMBLER" "$SOURCE_PREPARER" "$MEDIA_ASSEMBLER"; do
    [ -x "$tool" ] && [ ! -L "$tool" ] || die 'fixed RC tool is missing or unsafe'
done
openssl pkey -in "$signing_key" -noout >/dev/null 2>&1 || die 'signing key is not a supported private key'

if [ "$PREFLIGHT" -eq 1 ]; then
    "$IMAGE_ASSEMBLER" --resolved-inputs "$RESOLVED" --artifacts "$ARTIFACTS" \
        --runtime-bundle "$RUNTIME" --output "$OUTPUT" --project-root "$PROJECT_ROOT" \
        --project-commit "$PROJECT_COMMIT" --disk-size-gb "$DISK_SIZE_GB" --preflight
    printf 'PASS: verified integrated Northstar installer RC inputs for %s\n' "$PROJECT_COMMIT"
    exit 0
fi

if [ "$TEST_MODE" != 1 ]; then
    [ "$(uname -s)" = FreeBSD ] || die 'installer RC assembly requires FreeBSD'
    [ "$(id -u)" -eq 0 ] || die 'installer RC assembly must run as root on a disposable builder'
    for marker in "$IMAGE_MARKER" "$MEDIA_MARKER"; do
        [ -f "$marker" ] && [ ! -L "$marker" ] || die 'required disposable-builder marker is missing or unsafe'
        [ "$(stat -f '%u' "$marker")" -eq 0 ] || die 'disposable-builder marker must be root-owned'
        mode=$(stat -f '%Lp' "$marker"); case "$mode" in 400|600) ;; *) die 'disposable-builder marker must be mode 0400 or 0600' ;; esac
    done
    [ "$(stat -f '%u' "$signing_key")" -eq 0 ] || die 'RC signing key must be root-owned'
    key_mode=$(stat -f '%Lp' "$signing_key"); case "$key_mode" in 400|600) ;; *) die 'RC signing key must be mode 0400 or 0600' ;; esac
fi

output_parent=$(dirname "$OUTPUT"); mkdir -p "$output_parent"; output_parent=$(CDPATH= cd -- "$output_parent" && pwd -P)
OUTPUT=$output_parent/$(basename "$OUTPUT")
STAGING=$(mktemp -d "$output_parent/.northstar-installer-rc.XXXXXX")
image_output=$STAGING/image
source_output=$STAGING/installer-source
media_output=$STAGING/media

"$IMAGE_ASSEMBLER" --resolved-inputs "$RESOLVED" --artifacts "$ARTIFACTS" \
    --runtime-bundle "$RUNTIME" --output "$image_output" --project-root "$PROJECT_ROOT" \
    --project-commit "$PROJECT_COMMIT" --disk-size-gb "$DISK_SIZE_GB" \
    --builder-marker "$IMAGE_MARKER"
short_commit=$(printf '%s' "$PROJECT_COMMIT" | cut -c1-12)
qcow2=$image_output/northstar-15.1-amd64.qcow2
image_provenance=$image_output/image-provenance.conf
payload=$image_output/northstar-rootfs-v1-$short_commit.txz
runtime_manifest=$image_output/runtime-manifest.conf
for path in "$qcow2" "$image_provenance" "$payload" "$runtime_manifest"; do
    [ -f "$path" ] && [ ! -L "$path" ] || die 'QCOW2 stage omitted a required RC artifact'
done

"$SOURCE_PREPARER" --payload "$payload" --runtime-manifest "$runtime_manifest" \
    --project-root "$PROJECT_ROOT" --project-commit "$PROJECT_COMMIT" \
    --signing-key "$signing_key" --output "$source_output"

"$MEDIA_ASSEMBLER" --image "$qcow2" --image-provenance "$image_provenance" \
    --installer-source "$source_output" --output "$media_output" \
    --source-size-gb "$SOURCE_SIZE_GB" --builder-marker "$MEDIA_MARKER"

raw=$media_output/northstar-15.1-amd64-installer-usb.img
media_provenance=$media_output/media-provenance.conf
for path in "$raw" "$media_provenance"; do
    [ -f "$path" ] && [ ! -L "$path" ] || die 'media stage omitted a required RC artifact'
done
cat > "$STAGING/release-candidate.conf" <<EOF
schema_version=1
product=Northstar
release=15.1
architecture=amd64
project_commit=$PROJECT_COMMIT
image_sha256=$(file_sha256 "$qcow2")
image_size=$(file_size "$qcow2")
image_provenance_sha256=$(file_sha256 "$image_provenance")
payload_sha256=$(file_sha256 "$payload")
runtime_manifest_sha256=$(file_sha256 "$runtime_manifest")
installer_source_manifest_sha256=$(file_sha256 "$source_output/source-manifest.conf")
installer_source_signature_sha256=$(file_sha256 "$source_output/source-manifest.conf.sig")
installer_media_sha256=$(file_sha256 "$raw")
installer_media_size=$(file_size "$raw")
media_provenance_sha256=$(file_sha256 "$media_provenance")
host_disk_write=unsupported
EOF
chmod 0444 "$STAGING/release-candidate.conf"
mv "$STAGING" "$OUTPUT"; STAGING=; SUCCESS=1
printf 'PASS: assembled integrated Northstar installer RC at %s\n' "$OUTPUT"
