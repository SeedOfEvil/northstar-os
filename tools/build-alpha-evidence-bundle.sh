#!/bin/sh

# Build or verify one bounded M6 evidence bundle. Collection remains read-only;
# only the explicitly selected output directory is created.

set -eu

PROG=${0##*/}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
LANE=
OUTPUT=
MATRIX_OBSERVATIONS=
PLATFORM_OBSERVATIONS=
VERIFY=
REQUIRE_PASS=0

usage() {
    cat <<EOF
Usage:
  $PROG --lane vm|intel|amd --output DIRECTORY [options]
  $PROG --verify DIRECTORY [--require-pass]

Options:
  --matrix-observations FILE    Fixed PR91 operator observations.
  --platform-observations FILE  Fixed PR92 operator observations.
  --require-pass                Exit unsuccessfully unless the bundle passes.

Collection is read-only apart from the new atomic output directory. The bundle
contains three bounded records, a schema-1 summary, and their SHA-256 manifest.
EOF
}

fail() { printf 'ERROR: %s\n' "$1" >&2; exit "${2:-2}"; }

while [ "$#" -gt 0 ]; do
    case "$1" in
        --lane) [ "$#" -ge 2 ] || fail '--lane requires a value'; LANE=$2; shift 2 ;;
        --output) [ "$#" -ge 2 ] || fail '--output requires a value'; OUTPUT=$2; shift 2 ;;
        --matrix-observations) [ "$#" -ge 2 ] || fail '--matrix-observations requires a value'; MATRIX_OBSERVATIONS=$2; shift 2 ;;
        --platform-observations) [ "$#" -ge 2 ] || fail '--platform-observations requires a value'; PLATFORM_OBSERVATIONS=$2; shift 2 ;;
        --verify) [ "$#" -ge 2 ] || fail '--verify requires a value'; VERIFY=$2; shift 2 ;;
        --require-pass) REQUIRE_PASS=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) fail "unknown option: $1" ;;
    esac
done

field() {
    key=$1
    file=$2
    awk -F= -v wanted="$key" '$1 == wanted { count++; value=substr($0, index($0, "=") + 1) } END { if (count == 1) print value }' "$file"
}

hash_file() {
    file=$1
    if command -v sha256 >/dev/null 2>&1; then
        sha256 -q "$file"
    elif command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$file" | awk '{ print $1 }'
    else
        fail 'SHA-256 tool is unavailable'
    fi
}

file_mode() {
    path=$1
    stat -f '%Lp' "$path" 2>/dev/null || stat -c '%a' "$path" 2>/dev/null || fail 'file mode inspection is unavailable'
}

validate_bundle() {
    bundle=$1
    [ -d "$bundle" ] || fail 'bundle directory is unavailable'
    [ ! -L "$bundle" ] || fail 'bundle directory must not be a symbolic link'
    [ "$(file_mode "$bundle")" = 700 ] || fail 'bundle directory mode must be 0700'
    for name in alpha-readiness.conf platform-evidence.conf alpha-matrix.conf bundle.conf SHA256; do
        [ -f "$bundle/$name" ] || fail "bundle file is unavailable: $name"
        [ ! -L "$bundle/$name" ] || fail "bundle file must not be a symbolic link: $name"
        [ "$(file_mode "$bundle/$name")" = 600 ] || fail "bundle file mode must be 0600: $name"
    done
    file_count=$(find "$bundle" -maxdepth 1 -type f -print 2>/dev/null | awk 'END { print NR + 0 }')
    [ "$file_count" -eq 5 ] || fail 'bundle contains unknown or missing files'

    checksum_lines=$(awk 'NF { count++ } END { print count + 0 }' "$bundle/SHA256")
    [ "$checksum_lines" -eq 4 ] || fail 'SHA256 manifest must contain exactly four records'
    for name in alpha-readiness.conf platform-evidence.conf alpha-matrix.conf bundle.conf; do
        expected=$(awk -v wanted="$name" '$2 == wanted { count++; value=$1 } END { if (count == 1) print value }' "$bundle/SHA256")
        case "$expected" in
            ''|*[!0-9a-f]*) fail "invalid or duplicate SHA256 record: $name" ;;
        esac
        [ "${#expected}" -eq 64 ] || fail "invalid SHA256 length: $name"
        actual=$(hash_file "$bundle/$name")
        [ "$actual" = "$expected" ] || fail "bundle digest mismatch: $name"
    done

    summary=$bundle/bundle.conf
    required='schema_version captured_at_utc lane record_count readiness_status readiness_claim platform_status matrix_status bundle_status readiness_sha256 platform_sha256 matrix_sha256'
    for key in $required; do
        [ -n "$(field "$key" "$summary")" ] || fail "bundle summary field is missing or duplicated: $key"
    done
    summary_lines=$(awk 'NF { count++ } END { print count + 0 }' "$summary")
    [ "$summary_lines" -eq 12 ] || fail 'bundle summary contains unknown or missing records'
    [ "$(field schema_version "$summary")" = 1 ] || fail 'bundle schema must be exactly 1'
    [ "$(field record_count "$summary")" = 3 ] || fail 'bundle record count must be exactly 3'
    bundle_lane=$(field lane "$summary")
    case "$bundle_lane" in vm|intel|amd) ;; *) fail 'bundle lane is invalid' ;; esac
    bundle_status=$(field bundle_status "$summary")
    case "$bundle_status" in supplemental|blocked|pending|partial|fail|pass) ;; *) fail 'bundle status is invalid' ;; esac

    readiness=$bundle/alpha-readiness.conf
    platform=$bundle/platform-evidence.conf
    matrix=$bundle/alpha-matrix.conf
    [ "$(field readiness_sha256 "$summary")" = "$(hash_file "$readiness")" ] || fail 'summary readiness digest differs'
    [ "$(field platform_sha256 "$summary")" = "$(hash_file "$platform")" ] || fail 'summary platform digest differs'
    [ "$(field matrix_sha256 "$summary")" = "$(hash_file "$matrix")" ] || fail 'summary matrix digest differs'
    [ "$(field readiness_status "$summary")" = "$(field alpha_status "$readiness")" ] || fail 'summary readiness status differs'
    [ "$(field readiness_claim "$summary")" = "$(field matrix_claim "$readiness")" ] || fail 'summary readiness claim differs'
    [ "$(field platform_status "$summary")" = "$(field platform_status "$platform")" ] || fail 'summary platform status differs'
    [ "$(field matrix_status "$summary")" = "$(field matrix_status "$matrix")" ] || fail 'summary matrix status differs'
    [ "$(field expected_lane "$matrix")" = "$bundle_lane" ] || fail 'matrix lane differs from bundle lane'
    [ "$(field hardware_status "$matrix")" = "$(field alpha_status "$readiness")" ] || fail 'matrix readiness status differs'
    [ "$(field hardware_claim "$matrix")" = "$(field matrix_claim "$readiness")" ] || fail 'matrix readiness claim differs'
    [ "$(field hardware_graphics_lane "$matrix")" = "$(field graphics_lane "$readiness")" ] || fail 'matrix graphics lane differs'
    [ "$(field platform_status "$matrix")" = "$(field platform_status "$platform")" ] || fail 'matrix platform status differs'

    if [ "$bundle_status" = pass ]; then
        case "$bundle_lane" in intel|amd) ;; *) fail 'passing bundle is not a physical lane' ;; esac
        [ "$(field alpha_status "$readiness")" = ready ] || fail 'passing bundle readiness is not ready'
        [ "$(field matrix_claim "$readiness")" = "$bundle_lane" ] || fail 'passing bundle readiness claim differs'
        [ "$(field platform_class "$platform")" = physical ] || fail 'passing bundle platform is not physical'
        [ "$(field platform_status "$platform")" = pass ] || fail 'passing bundle platform did not pass'
        [ "$(field matrix_status "$matrix")" = pass ] || fail 'passing bundle matrix did not pass'
    fi

    printf 'BUNDLE_VERIFIED=yes\nLANE=%s\nBUNDLE_STATUS=%s\nBUNDLE=%s\n' \
        "$bundle_lane" "$bundle_status" "$bundle"
    if [ "$REQUIRE_PASS" -eq 1 ] && [ "$bundle_status" != pass ]; then return 1; fi
}

if [ -n "$VERIFY" ]; then
    [ -z "$LANE$OUTPUT$MATRIX_OBSERVATIONS$PLATFORM_OBSERVATIONS" ] || fail '--verify cannot be combined with collection options'
    validate_bundle "$VERIFY"
    exit $?
fi

case "$LANE" in vm|intel|amd) ;; *) fail '--lane must be vm, intel, or amd' ;; esac
[ -n "$OUTPUT" ] || fail '--output is required'
[ ! -e "$OUTPUT" ] || fail 'output directory already exists'
[ ! -L "$OUTPUT" ] || fail 'output directory must not be a symbolic link'
[ -z "$MATRIX_OBSERVATIONS" ] || { [ -f "$MATRIX_OBSERVATIONS" ] && [ ! -L "$MATRIX_OBSERVATIONS" ]; } || fail 'matrix observations are unavailable or symbolic'
[ -z "$PLATFORM_OBSERVATIONS" ] || { [ -f "$PLATFORM_OBSERVATIONS" ] && [ ! -L "$PLATFORM_OBSERVATIONS" ]; } || fail 'platform observations are unavailable or symbolic'

parent=$(dirname "$OUTPUT")
mkdir -p "$parent"
workspace=$OUTPUT.tmp.$$
[ ! -e "$workspace" ] || fail 'temporary output already exists'
umask 077
mkdir "$workspace"
chmod 0700 "$workspace"
cleanup() { rm -rf "$workspace"; }
trap cleanup EXIT HUP INT TERM

readiness=$workspace/alpha-readiness.conf
platform=$workspace/platform-evidence.conf
matrix=$workspace/alpha-matrix.conf
sh "$SCRIPT_DIR/collect-alpha-readiness.sh" --output "$readiness" >/dev/null

set -- --output "$platform"
[ -z "$PLATFORM_OBSERVATIONS" ] || set -- "$@" --observations "$PLATFORM_OBSERVATIONS"
sh "$SCRIPT_DIR/collect-platform-evidence.sh" "$@" >/dev/null

set -- --lane "$LANE" --output "$matrix" --platform-evidence "$platform"
[ -z "$MATRIX_OBSERVATIONS" ] || set -- "$@" --observations "$MATRIX_OBSERVATIONS"
sh "$SCRIPT_DIR/run-alpha-matrix.sh" "$@" >/dev/null

readiness_status=$(field alpha_status "$readiness")
readiness_claim=$(field matrix_claim "$readiness")
platform_status=$(field platform_status "$platform")
matrix_status=$(field matrix_status "$matrix")
[ "$(field expected_lane "$matrix")" = "$LANE" ] || fail 'matrix lane differs during collection'
[ "$(field hardware_status "$matrix")" = "$readiness_status" ] || fail 'readiness status changed during collection'
[ "$(field hardware_claim "$matrix")" = "$readiness_claim" ] || fail 'readiness claim changed during collection'
[ "$(field platform_status "$matrix")" = "$platform_status" ] || fail 'platform status changed during collection'

bundle_status=blocked
if [ "$platform_status" = fail ] || [ "$matrix_status" = fail ]; then bundle_status=fail
elif [ "$platform_status" = pending ] || [ "$matrix_status" = pending ]; then bundle_status=pending
elif [ "$platform_status" = partial ] || [ "$matrix_status" = partial ]; then bundle_status=partial
elif [ "$LANE" = vm ]; then
    case "$platform_status:$matrix_status:$readiness_status" in
        inventory-only:inventory-only:supplemental|supplemental:supplemental:supplemental) bundle_status=supplemental ;;
    esac
else
    if [ "$readiness_status" = ready ] && [ "$readiness_claim" = "$LANE" ] && \
        [ "$platform_status" = pass ] && [ "$matrix_status" = pass ]; then
        bundle_status=pass
    fi
fi

readiness_sha=$(hash_file "$readiness")
platform_sha=$(hash_file "$platform")
matrix_sha=$(hash_file "$matrix")
summary=$workspace/bundle.conf
{
    printf 'schema_version=1\n'
    printf 'captured_at_utc=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    printf 'lane=%s\n' "$LANE"
    printf 'record_count=3\n'
    printf 'readiness_status=%s\n' "$readiness_status"
    printf 'readiness_claim=%s\n' "$readiness_claim"
    printf 'platform_status=%s\n' "$platform_status"
    printf 'matrix_status=%s\n' "$matrix_status"
    printf 'bundle_status=%s\n' "$bundle_status"
    printf 'readiness_sha256=%s\n' "$readiness_sha"
    printf 'platform_sha256=%s\n' "$platform_sha"
    printf 'matrix_sha256=%s\n' "$matrix_sha"
} > "$summary"

{
    printf '%s  alpha-readiness.conf\n' "$readiness_sha"
    printf '%s  platform-evidence.conf\n' "$platform_sha"
    printf '%s  alpha-matrix.conf\n' "$matrix_sha"
    printf '%s  bundle.conf\n' "$(hash_file "$summary")"
} > "$workspace/SHA256"
chmod 0600 "$readiness" "$platform" "$matrix" "$summary" "$workspace/SHA256"
requested_require_pass=$REQUIRE_PASS
REQUIRE_PASS=0
validate_bundle "$workspace" >/dev/null
REQUIRE_PASS=$requested_require_pass
mv "$workspace" "$OUTPUT"
trap - EXIT HUP INT TERM

printf 'BUNDLE_VERIFIED=yes\nLANE=%s\nBUNDLE_STATUS=%s\nBUNDLE=%s\n' \
    "$LANE" "$bundle_status" "$OUTPUT"
if [ "$REQUIRE_PASS" -eq 1 ] && [ "$bundle_status" != pass ]; then exit 1; fi
