#!/bin/sh

# Package the accepted patched Wayfire tree used by the Proxmox/scfb lane.

set -eu

SOURCE=
OUTPUT=
SOURCE_REVISION=
PATCH_SHA256=
SOURCE_DATE_EPOCH=
STAGING=

usage() {
    cat <<'USAGE'
Usage: package-nested-wayfire.sh --source DIRECTORY --output NEW_DIRECTORY \
  --source-revision REVISION --patch-sha256 SHA256 \
  --source-date-epoch UNIX_SECONDS
USAGE
}

die() { printf 'ERROR: %s\n' "$1" >&2; exit 1; }
cleanup() { [ -z "$STAGING" ] || [ ! -d "$STAGING" ] || rm -rf "$STAGING"; }
trap cleanup EXIT HUP INT TERM

while [ "$#" -gt 0 ]; do
    case "$1" in
        --source) SOURCE=${2-}; shift 2 ;;
        --output) OUTPUT=${2-}; shift 2 ;;
        --source-revision) SOURCE_REVISION=${2-}; shift 2 ;;
        --patch-sha256) PATCH_SHA256=${2-}; shift 2 ;;
        --source-date-epoch) SOURCE_DATE_EPOCH=${2-}; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

[ "$(uname -s)" = FreeBSD ] || die 'nested Wayfire packaging requires FreeBSD'
for command_name in awk basename cp dirname find grep mkdir mktemp mv pkg readlink rm sed sha256 sort stat; do
    command -v "$command_name" >/dev/null 2>&1 \
        || die "required command is unavailable: $command_name"
done
[ -d "$SOURCE" ] && [ ! -L "$SOURCE" ] || die 'source must be a real directory'
[ -x "$SOURCE/bin/wayfire" ] || die 'source does not contain an executable Wayfire binary'
[ -n "$OUTPUT" ] && [ ! -e "$OUTPUT" ] && [ ! -L "$OUTPUT" ] || die 'output must not exist'
printf '%s\n' "$SOURCE_REVISION" | grep -Eq '^[0-9a-f]{7,40}$' \
    || die 'source revision must be a lowercase Git revision'
printf '%s\n' "$PATCH_SHA256" | grep -Eq '^[0-9a-f]{64}$' \
    || die 'patch-sha256 must be a lowercase SHA-256 digest'
printf '%s\n' "$SOURCE_DATE_EPOCH" | grep -Eq '^[0-9]{10}$' \
    || die 'source-date-epoch must be ten decimal digits'

output_parent=$(dirname "$OUTPUT")
mkdir -p "$output_parent"
output_parent=$(CDPATH= cd -- "$output_parent" && pwd)
OUTPUT=$output_parent/$(basename "$OUTPUT")
STAGING=$(mktemp -d "$output_parent/.northstar-wayfire-package.XXXXXX")
target=$STAGING/root/home/northstar/.local/wayfire-nested
mkdir -p "$(dirname "$target")" "$STAGING/output" "$STAGING/metadata"
cp -Rp "$SOURCE" "$target"

tree_records=$STAGING/tree-records
find "$target" \( -type f -o -type l \) -print | sort | while IFS= read -r path; do
    relative=${path#"$target"/}
    case "$relative" in *'|'*|*'\n'*) exit 20 ;; esac
    if [ -L "$path" ]; then
        printf 'link|%s|%s\n' "$relative" "$(readlink "$path")"
    else
        printf 'file|%s|%s|%s\n' "$relative" "$(sha256 -q "$path")" "$(stat -f '%z' "$path")"
    fi
done > "$tree_records" || die 'compatibility runtime tree contains unsafe paths'
[ -s "$tree_records" ] || die 'compatibility runtime tree is empty'
tree_sha256=$(sha256 -q "$tree_records")

cat > "$STAGING/metadata/+MANIFEST" <<EOF
name: "northstar-wayfire-nested"
version: "0.10.1.746bc7e"
origin: "x11-wm/northstar-wayfire-nested"
comment: "Northstar patched nested Wayfire runtime"
desc: "Reviewed Wayfire compatibility runtime for Northstar's Proxmox scfb lane."
maintainer: "northstar@localhost.invalid"
www: "https://github.com/SeedOfEvil/northstar-os"
prefix: "/home/northstar/.local/wayfire-nested"
arch: "freebsd:15:x86:64"
licenselogic: "single"
licenses: ["MIT"]
annotations: {
  northstar_source_revision: "$SOURCE_REVISION",
  northstar_patch_sha256: "$PATCH_SHA256",
  northstar_tree_sha256: "$tree_sha256"
}
EOF

find "$target" \( -type f -o -type l \) -print | sort \
    | sed "s|^$STAGING/root||" > "$STAGING/plist"
pkg create -q -l 1 -T 2 -t "$SOURCE_DATE_EPOCH" -r "$STAGING/root" \
    -m "$STAGING/metadata" -p "$STAGING/plist" -o "$STAGING/output" \
    || die 'pkg failed to create the compatibility compositor package'
package_path=$(find "$STAGING/output" -type f -name '*.pkg' -print)
[ "$(printf '%s\n' "$package_path" | awk 'NF { count++ } END { print count + 0 }')" -eq 1 ] \
    || die 'pkg did not create exactly one compatibility package'
package_sha256=$(sha256 -q "$package_path")
package_size=$(stat -f '%z' "$package_path")

mkdir "$STAGING/final"
cp -p "$package_path" "$STAGING/final/"
cp "$tree_records" "$STAGING/final/runtime-tree-records"
printf '%s\n' \
    'schema_version=1' \
    "source_revision=$SOURCE_REVISION" \
    "patch_sha256=$PATCH_SHA256" \
    "runtime_tree_sha256=$tree_sha256" \
    "package_sha256=$package_sha256" \
    "package_size=$package_size" \
    > "$STAGING/final/compat-package.conf"
chmod 0444 "$STAGING/final/compat-package.conf" "$STAGING/final/runtime-tree-records"
mv "$STAGING/final" "$OUTPUT"
rm -rf "$STAGING"
STAGING=
printf 'PASS: packaged accepted nested Wayfire runtime at %s\n' "$OUTPUT"
