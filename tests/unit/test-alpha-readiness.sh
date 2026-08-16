#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-alpha-readiness.XXXXXX")
OUTPUT=$TMP_DIR/readiness.conf
STDOUT=$TMP_DIR/stdout
STDERR=$TMP_DIR/stderr

cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT HUP INT TERM
fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
contains() { grep -Fx "$1" "$2" >/dev/null 2>&1 || fail "missing $1"; }

run_probe() {
    NORTHSTAR_ALPHA_TEST_MODE=1 \
    NORTHSTAR_ALPHA_TEST_RELEASE=${TEST_RELEASE:-15.1-RELEASE-p2} \
    NORTHSTAR_ALPHA_TEST_ARCH=${TEST_ARCH:-amd64} \
    NORTHSTAR_ALPHA_TEST_BOOT=${TEST_BOOT:-UEFI} \
    NORTHSTAR_ALPHA_TEST_ROOTFS=${TEST_ROOTFS:-zfs} \
    NORTHSTAR_ALPHA_TEST_PLATFORM=${TEST_PLATFORM:-virtual-machine} \
    NORTHSTAR_ALPHA_TEST_CPU_VENDOR=${TEST_CPU_VENDOR:-intel} \
    NORTHSTAR_ALPHA_TEST_DRM_CARDS=${TEST_DRM_CARDS:-0} \
    NORTHSTAR_ALPHA_TEST_DRM_RENDER=${TEST_DRM_RENDER:-0} \
    NORTHSTAR_ALPHA_TEST_DRM_DRIVER=${TEST_DRM_DRIVER:-none} \
    NORTHSTAR_ALPHA_TEST_WIRED=${TEST_WIRED:-1} \
    NORTHSTAR_ALPHA_TEST_WIFI=${TEST_WIFI:-0} \
    NORTHSTAR_ALPHA_TEST_AUDIO=${TEST_AUDIO:-1} \
    NORTHSTAR_ALPHA_TEST_INPUT=${TEST_INPUT:-2} \
    NORTHSTAR_ALPHA_TEST_AGENT=${TEST_AGENT:-yes} \
        sh "$ROOT/tools/collect-alpha-readiness.sh" --output "$OUTPUT" "$@" \
        > "$STDOUT" 2> "$STDERR"
}

run_probe
contains 'schema_version=1' "$OUTPUT"
contains 'graphics_lane=vm-supplemental' "$OUTPUT"
contains 'matrix_claim=vm' "$OUTPUT"
contains 'alpha_status=supplemental' "$OUTPUT"
contains 'blockers=direct_drm_kms' "$OUTPUT"
[ "$(stat -f '%Lp' "$OUTPUT" 2>/dev/null || stat -c '%a' "$OUTPUT")" = 600 ] \
    || fail 'output mode is not 0600'

TEST_PLATFORM=physical TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 TEST_DRM_DRIVER=intel run_probe --require-ready
contains 'graphics_lane=intel-drm' "$OUTPUT"
contains 'matrix_claim=intel' "$OUTPUT"
contains 'alpha_status=ready' "$OUTPUT"
contains 'blockers=none' "$OUTPUT"

TEST_PLATFORM=physical TEST_CPU_VENDOR=amd TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 \
TEST_DRM_DRIVER=amd TEST_AUDIO=0 run_probe
contains 'graphics_lane=amd-drm' "$OUTPUT"
contains 'matrix_claim=none' "$OUTPUT"
contains 'alpha_status=blocked' "$OUTPUT"
contains 'blockers=audio' "$OUTPUT"

TEST_PLATFORM=physical TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 \
TEST_DRM_DRIVER=other run_probe
contains 'graphics_lane=other-drm' "$OUTPUT"
contains 'alpha_status=blocked' "$OUTPUT"
contains 'blockers=supported_graphics' "$OUTPUT"

TEST_RELEASE=14.3-RELEASE TEST_PLATFORM=virtual-machine run_probe
contains 'matrix_claim=none' "$OUTPUT"
contains 'alpha_status=blocked' "$OUTPUT"
contains 'blockers=freebsd_release,direct_drm_kms' "$OUTPUT"

if TEST_PLATFORM=physical TEST_DRM_CARDS=0 TEST_DRM_RENDER=0 TEST_DRM_DRIVER=none \
    run_probe --require-ready; then
    fail '--require-ready accepted an incomplete lane'
fi

if grep -E 'secret|MAC|serial|command' "$OUTPUT" >/dev/null 2>&1; then
    fail 'readiness output contains a forbidden private-data marker'
fi

printf '%s\n' 'PASS: M6 alpha readiness classifies VM, Intel, AMD, and blocked lanes'
