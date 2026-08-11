#!/bin/sh

# Validate immutable M5 image inputs and atomically record their provenance.
# This unprivileged stage never downloads artifacts or mutates a disk image.

set -eu

LOCK=
ARTIFACTS=
OUTPUT=
PROJECT_COMMIT=
CHECK_LOCK=0
STAGING=

usage() {
    cat <<'USAGE'
Usage: prepare-image-inputs.sh --lock FILE --check-lock
       prepare-image-inputs.sh --lock FILE --artifacts DIR --output DIR \
         --project-commit FULL_GIT_COMMIT

The preparation stage validates a strict input lock and, in normal mode,
verifies staged base.txz, kernel.txz, and Northstar package artifacts before
atomically writing a deterministic resolved-input record. It never downloads,
extracts, mounts, partitions, or modifies an image.
USAGE
}

die() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

cleanup() {
    if [ -n "$STAGING" ] && [ -d "$STAGING" ]; then
        rm -rf "$STAGING"
    fi
}
trap cleanup EXIT HUP INT TERM

while [ "$#" -gt 0 ]; do
    case "$1" in
        --lock) LOCK=${2-}; shift 2 ;;
        --artifacts) ARTIFACTS=${2-}; shift 2 ;;
        --output) OUTPUT=${2-}; shift 2 ;;
        --project-commit) PROJECT_COMMIT=${2-}; shift 2 ;;
        --check-lock) CHECK_LOCK=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

[ -f "$LOCK" ] && [ ! -L "$LOCK" ] || die 'lock must be a regular non-symlink file'

for command_name in awk basename cp dirname grep mkdir mktemp mv rm sort tr wc; do
    command -v "$command_name" >/dev/null 2>&1 || die "required command is unavailable: $command_name"
done
if command -v sha256 >/dev/null 2>&1; then
    SHA256_COMMAND=sha256
elif command -v sha256sum >/dev/null 2>&1; then
    SHA256_COMMAND=sha256sum
else
    die 'sha256 or sha256sum is required'
fi

case "$(LC_ALL=C tr -d '\11\12\15\40-\176' < "$LOCK" | wc -c | tr -d ' ')" in
    0) : ;;
    *) die 'lock contains non-ASCII or control characters' ;;
esac

allowed_keys='SCHEMA_VERSION TARGET_FORMAT FIRMWARE ROOT_FILESYSTEM FREEBSD_RELEASE FREEBSD_ARCH FREEBSD_ABI SOURCE_DATE_EPOCH RELEASE_BASE_URL RELEASE_MANIFEST RELEASE_MANIFEST_SHA256 BASE_ARTIFACT BASE_SHA256 BASE_SIZE KERNEL_ARTIFACT KERNEL_SHA256 KERNEL_SIZE NORTHSTAR_PACKAGE NORTHSTAR_PACKAGE_VERSION NORTHSTAR_PACKAGE_SHA256 NORTHSTAR_SOURCE_REVISION NORTHSTAR_REPOSITORY_REVISION NORTHSTAR_CATALOGUE_SHA256 NORTHSTAR_METADATA_SHA256 NORTHSTAR_SIGNATURE_FINGERPRINT'

awk -F= '
    /^[[:space:]]*($|#)/ { next }
    $1 !~ /^[A-Z][A-Z0-9_]*$/ || NF < 2 { exit 1 }
' "$LOCK" || die 'lock contains malformed entries'

for present_key in $(awk -F= '/^[A-Z][A-Z0-9_]*=/ { print $1 }' "$LOCK"); do
    case " $allowed_keys " in
        *" $present_key "*) : ;;
        *) die "lock contains unsupported key: $present_key" ;;
    esac
done

lock_count() {
    awk -F= -v key="$1" '$1 == key { count++ } END { print count + 0 }' "$LOCK"
}

lock_value() {
    awk -F= -v key="$1" '$1 == key { sub(/^[^=]*=/, ""); print; exit }' "$LOCK"
}

for required_key in $allowed_keys; do
    [ "$(lock_count "$required_key")" -eq 1 ] \
        || die "lock key must appear exactly once: $required_key"
    [ -n "$(lock_value "$required_key")" ] || die "lock key is empty: $required_key"
done

if grep -Eiq '(^|=)(UNSET|LATEST|RESOLVED_BY_BUILDER|GENERATED_AT_BUILD_TIME)($|[[:space:]])' "$LOCK"; then
    die 'lock contains unresolved or moving input values'
fi

[ "$(lock_value SCHEMA_VERSION)" = 1 ] || die 'unsupported image lock schema'
[ "$(lock_value TARGET_FORMAT)" = qcow2 ] || die 'first M5 target must be qcow2'
[ "$(lock_value FIRMWARE)" = UEFI ] || die 'image firmware must be UEFI'
[ "$(lock_value ROOT_FILESYSTEM)" = ZFS ] || die 'image root filesystem must be ZFS'
[ "$(lock_value FREEBSD_RELEASE)" = 15.1-RELEASE ] || die 'image release must be FreeBSD 15.1-RELEASE'
[ "$(lock_value FREEBSD_ARCH)" = amd64 ] || die 'image architecture must be amd64'
[ "$(lock_value FREEBSD_ABI)" = 'FreeBSD:15:amd64' ] || die 'image ABI is unsupported'

case "$(lock_value RELEASE_BASE_URL)" in
    https://download.freebsd.org/releases/amd64/amd64/15.1-RELEASE) : ;;
    *) die 'release URL must be the pinned official FreeBSD 15.1 amd64 directory' ;;
esac

safe_artifact_name() {
    value=$1
    printf '%s\n' "$value" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9._+-]*$' \
        || die "unsafe artifact name: $value"
}

safe_artifact_name "$(lock_value BASE_ARTIFACT)"
safe_artifact_name "$(lock_value KERNEL_ARTIFACT)"
safe_artifact_name "$(lock_value NORTHSTAR_PACKAGE)"
[ "$(lock_value RELEASE_MANIFEST)" = MANIFEST ] || die 'release manifest must be MANIFEST'
[ "$(lock_value BASE_ARTIFACT)" = base.txz ] || die 'base artifact must be base.txz'
[ "$(lock_value KERNEL_ARTIFACT)" = kernel.txz ] || die 'kernel artifact must be kernel.txz'

for digest_key in RELEASE_MANIFEST_SHA256 BASE_SHA256 KERNEL_SHA256 NORTHSTAR_PACKAGE_SHA256 \
    NORTHSTAR_CATALOGUE_SHA256 \
    NORTHSTAR_METADATA_SHA256 NORTHSTAR_SIGNATURE_FINGERPRINT; do
    printf '%s\n' "$(lock_value "$digest_key")" | grep -Eq '^[0-9a-f]{64}$' \
        || die "invalid lowercase SHA-256 digest: $digest_key"
done
printf '%s\n' "$(lock_value NORTHSTAR_SOURCE_REVISION)" | grep -Eq '^[0-9a-f]{40}$' \
    || die 'invalid Northstar source revision'

for numeric_key in SOURCE_DATE_EPOCH BASE_SIZE KERNEL_SIZE NORTHSTAR_REPOSITORY_REVISION; do
    printf '%s\n' "$(lock_value "$numeric_key")" | grep -Eq '^[0-9]+$' \
        || die "invalid numeric value: $numeric_key"
done
printf '%s\n' "$(lock_value NORTHSTAR_PACKAGE_VERSION)" \
    | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || die 'invalid Northstar package version'

if [ "$CHECK_LOCK" -eq 1 ]; then
    [ -z "$ARTIFACTS$OUTPUT$PROJECT_COMMIT" ] \
        || die '--check-lock cannot be combined with staging arguments'
    printf 'PASS: validated pinned Northstar M5 QCOW2 input lock\n'
    exit 0
fi

[ -n "$ARTIFACTS" ] && [ -n "$OUTPUT" ] && [ -n "$PROJECT_COMMIT" ] \
    || die 'normal mode requires --artifacts, --output, and --project-commit'
printf '%s\n' "$PROJECT_COMMIT" | grep -Eq '^[0-9a-f]{40}$' \
    || die 'project commit must be a full lowercase Git revision'
[ -d "$ARTIFACTS" ] && [ ! -L "$ARTIFACTS" ] \
    || die 'artifacts path must be a real directory'
[ ! -e "$OUTPUT" ] && [ ! -L "$OUTPUT" ] || die 'output already exists'

ARTIFACTS=$(CDPATH= cd -- "$ARTIFACTS" && pwd)
output_parent=$(dirname "$OUTPUT")
mkdir -p "$output_parent"
output_parent=$(CDPATH= cd -- "$output_parent" && pwd)
OUTPUT=$output_parent/$(basename "$OUTPUT")

file_sha256() {
    if [ "$SHA256_COMMAND" = sha256 ]; then
        sha256 -q "$1"
    else
        sha256sum "$1" | awk '{ print $1 }'
    fi
}

file_size() {
    wc -c < "$1" | tr -d ' '
}

verify_artifact() {
    name=$1
    expected_sha=$2
    expected_size=${3-}
    path=$ARTIFACTS/$name
    [ -f "$path" ] && [ ! -L "$path" ] || die "artifact is missing or unsafe: $name"
    actual_sha=$(file_sha256 "$path")
    [ "$actual_sha" = "$expected_sha" ] || die "artifact digest mismatch: $name"
    actual_size=$(file_size "$path")
    if [ -n "$expected_size" ] && [ "$actual_size" != "$expected_size" ]; then
        die "artifact size mismatch: $name"
    fi
    printf '%s|%s|%s\n' "$name" "$actual_sha" "$actual_size"
}

STAGING=$(mktemp -d "$output_parent/.northstar-image-inputs.XXXXXX")
records=$STAGING/artifact-records
unsorted_records=$STAGING/artifact-records.unsorted
verify_artifact "$(lock_value BASE_ARTIFACT)" "$(lock_value BASE_SHA256)" \
    "$(lock_value BASE_SIZE)" > "$unsorted_records"
verify_artifact "$(lock_value KERNEL_ARTIFACT)" "$(lock_value KERNEL_SHA256)" \
    "$(lock_value KERNEL_SIZE)" >> "$unsorted_records"
verify_artifact "$(lock_value NORTHSTAR_PACKAGE)" \
    "$(lock_value NORTHSTAR_PACKAGE_SHA256)" >> "$unsorted_records"
sort "$unsorted_records" > "$records"
rm -f "$unsorted_records"

cp "$LOCK" "$STAGING/input.lock"
lock_sha256=$(file_sha256 "$STAGING/input.lock")
records_sha256=$(file_sha256 "$records")

{
    printf 'schema_version=1\n'
    printf 'target_format=qcow2\n'
    printf 'project_commit=%s\n' "$PROJECT_COMMIT"
    printf 'source_date_epoch=%s\n' "$(lock_value SOURCE_DATE_EPOCH)"
    printf 'input_lock_sha256=%s\n' "$lock_sha256"
    printf 'artifact_records_sha256=%s\n' "$records_sha256"
    printf 'freebsd_release=%s\n' "$(lock_value FREEBSD_RELEASE)"
    printf 'freebsd_arch=%s\n' "$(lock_value FREEBSD_ARCH)"
    printf 'freebsd_release_manifest_sha256=%s\n' "$(lock_value RELEASE_MANIFEST_SHA256)"
    printf 'northstar_package_version=%s\n' "$(lock_value NORTHSTAR_PACKAGE_VERSION)"
    printf 'northstar_source_revision=%s\n' "$(lock_value NORTHSTAR_SOURCE_REVISION)"
    printf 'northstar_repository_revision=%s\n' "$(lock_value NORTHSTAR_REPOSITORY_REVISION)"
    printf 'northstar_catalogue_sha256=%s\n' "$(lock_value NORTHSTAR_CATALOGUE_SHA256)"
    printf 'northstar_metadata_sha256=%s\n' "$(lock_value NORTHSTAR_METADATA_SHA256)"
    printf 'northstar_signature_fingerprint=%s\n' "$(lock_value NORTHSTAR_SIGNATURE_FINGERPRINT)"
} > "$STAGING/resolved-image-inputs.conf"

chmod 0444 "$STAGING/input.lock" "$records" "$STAGING/resolved-image-inputs.conf"
mv "$STAGING" "$OUTPUT"
STAGING=
printf 'PASS: prepared verified Northstar image inputs at %s\n' "$OUTPUT"
