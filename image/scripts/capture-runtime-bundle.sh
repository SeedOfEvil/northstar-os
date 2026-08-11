#!/bin/sh

# Capture an exact, offline package closure from an accepted FreeBSD host.
# The resulting directory is immutable input for the privileged image builder.

set -eu

ROOTS=
NORTHSTAR_PACKAGE=
OUTPUT=
STAGING=

usage() {
    cat <<'USAGE'
Usage: capture-runtime-bundle.sh --roots FILE --northstar-package FILE \
  --output NEW_DIRECTORY

All root packages must already be installed. Dependencies are traversed from
the local pkg database, exact installed packages are recreated with pkg create,
and the reviewed Northstar package is copied from its immutable publication.
No package is downloaded or installed.
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
        --roots) ROOTS=${2-}; shift 2 ;;
        --northstar-package) NORTHSTAR_PACKAGE=${2-}; shift 2 ;;
        --output) OUTPUT=${2-}; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

[ "$(uname -s)" = FreeBSD ] || die 'runtime capture requires FreeBSD'
for command_name in awk basename cp dirname find grep mkdir mktemp mv pkg rm sed sha256 sort stat tr wc; do
    command -v "$command_name" >/dev/null 2>&1 \
        || die "required command is unavailable: $command_name"
done
[ -f "$ROOTS" ] && [ ! -L "$ROOTS" ] || die 'roots must be a regular non-symlink file'
[ -f "$NORTHSTAR_PACKAGE" ] && [ ! -L "$NORTHSTAR_PACKAGE" ] \
    || die 'Northstar package must be a regular non-symlink file'
[ -n "$OUTPUT" ] || die 'output is required'
[ ! -e "$OUTPUT" ] && [ ! -L "$OUTPUT" ] || die 'output already exists'

case "$(LC_ALL=C tr -d '\11\12\15\40-\176' < "$ROOTS" | wc -c | tr -d ' ')" in
    0) : ;;
    *) die 'roots contain non-ASCII or control characters' ;;
esac

awk '
    /^[[:space:]]*($|#)/ { next }
    !/^[A-Za-z0-9][A-Za-z0-9+_.-]*$/ { exit 1 }
    seen[$0]++ { exit 1 }
' "$ROOTS" || die 'roots contain an unsafe or duplicate package name'

output_parent=$(dirname "$OUTPUT")
mkdir -p "$output_parent"
output_parent=$(CDPATH= cd -- "$output_parent" && pwd)
OUTPUT=$output_parent/$(basename "$OUTPUT")
STAGING=$(mktemp -d "$output_parent/.northstar-runtime.XXXXXX")
mkdir -p "$STAGING/packages"

pending=$STAGING/pending
seen=$STAGING/seen
: > "$pending"
: > "$seen"
awk '!/^[[:space:]]*($|#)/ { print }' "$ROOTS" | sort > "$pending"

while [ -s "$pending" ]; do
    package_name=$(sed -n '1p' "$pending")
    sed '1d' "$pending" > "$pending.next"
    mv "$pending.next" "$pending"
    grep -Fx "$package_name" "$seen" >/dev/null 2>&1 && continue
    printf '%s\n' "$package_name" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9+_.-]*$' \
        || die "unsafe dependency name: $package_name"
    pkg info -e "$package_name" || die "required runtime package is not installed: $package_name"
    printf '%s\n' "$package_name" >> "$seen"
    pkg query -e "%n = '$package_name'" '%dn' 2>/dev/null \
        | grep -E '^[A-Za-z0-9][A-Za-z0-9+_.-]*$' >> "$pending" || true
    sort -u "$pending" -o "$pending"
done
sort -u "$seen" -o "$seen"

northstar_name=$(pkg query -F "$NORTHSTAR_PACKAGE" '%n') \
    || die 'cannot read Northstar package metadata'
[ "$northstar_name" = northstar ] || die 'reviewed package is not named northstar'

while IFS= read -r package_name; do
    if [ "$package_name" = northstar ]; then
        cp -p "$NORTHSTAR_PACKAGE" "$STAGING/packages/"
    else
        pkg create -q -o "$STAGING/packages" "$package_name" \
            || die "failed to recreate installed package: $package_name"
    fi
done < "$seen"

records_unsorted=$STAGING/runtime-package-records.unsorted
records=$STAGING/runtime-package-records
: > "$records_unsorted"
find "$STAGING/packages" -type f -name '*.pkg' -print | while IFS= read -r package_path; do
    [ ! -L "$package_path" ] || exit 20
    metadata=$(pkg query -F "$package_path" '%n|%v|%o') || exit 21
    [ "$(printf '%s\n' "$metadata" | wc -l | tr -d ' ')" = 1 ] || exit 22
    name=$(printf '%s' "$metadata" | awk -F'|' '{ print $1 }')
    version=$(printf '%s' "$metadata" | awk -F'|' '{ print $2 }')
    origin=$(printf '%s' "$metadata" | awk -F'|' '{ print $3 }')
    printf '%s\n' "$name" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9+_.-]*$' || exit 23
    printf '%s\n' "$version" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9+_.~,:-]*$' || exit 24
    printf '%s\n' "$origin" | grep -Eq '^[A-Za-z0-9_.+~-]+/[A-Za-z0-9_.+~-]+$' || exit 25
    filename=$(basename "$package_path")
    digest=$(sha256 -q "$package_path")
    size=$(stat -f '%z' "$package_path")
    printf '%s|%s|%s|%s|%s|%s\n' \
        "$filename" "$digest" "$size" "$name" "$version" "$origin"
done > "$records_unsorted" || die 'runtime package metadata is missing or unsafe'

sort -t '|' -k4,4 "$records_unsorted" > "$records"
rm -f "$records_unsorted" "$pending" "$seen"
[ -s "$records" ] || die 'runtime bundle is empty'
awk -F'|' 'seen[$4]++ { exit 1 }' "$records" \
    || die 'runtime bundle contains duplicate package names'

while IFS= read -r required_name; do
    awk -F'|' -v name="$required_name" '$4 == name { found=1 } END { exit !found }' "$records" \
        || die "runtime bundle omitted required root: $required_name"
done <<EOF
$(awk '!/^[[:space:]]*($|#)/ { print }' "$ROOTS")
EOF

package_count=$(wc -l < "$records" | tr -d ' ')
records_sha256=$(sha256 -q "$records")
printf '%s\n' \
    'schema_version=1' \
    'freebsd_abi=FreeBSD:15:amd64' \
    "package_count=$package_count" \
    "runtime_package_records_sha256=$records_sha256" \
    > "$STAGING/runtime-bundle.conf"
cp "$ROOTS" "$STAGING/runtime-roots.txt"
chmod 0444 "$STAGING/runtime-bundle.conf" "$records" "$STAGING/runtime-roots.txt"
mv "$STAGING" "$OUTPUT"
STAGING=
printf 'PASS: captured %s exact runtime packages at %s\n' "$package_count" "$OUTPUT"
