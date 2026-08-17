#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-alpha-matrix-test.XXXXXX")
OUTPUT=$TMP_DIR/matrix.conf
OBSERVATIONS=$TMP_DIR/observations.conf
TEMPLATE=$TMP_DIR/template.conf
PLATFORM=$TMP_DIR/platform.conf

cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT HUP INT TERM
fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
contains() { grep -Fx "$1" "$2" >/dev/null 2>&1 || fail "missing $1"; }

write_observations() {
    override_key=${1:-none}
    override_value=${2:-pass}
    {
        printf 'schema_version=1\n'
        for key in graphical_login direct_compositor display_output native_qt xwayland firefox files settings networking audio input shell_recovery update_rollback clean_shutdown; do
            value=pass
            [ "$key" != "$override_key" ] || value=$override_value
            printf '%s=%s\n' "$key" "$value"
        done
    } > "$OBSERVATIONS"
}

write_platform() {
    platform_status=${1:-pass}
    {
        printf 'schema_version=1\n'
        printf 'captured_at_utc=2026-08-17T00:00:00Z\n'
        printf 'platform_class=physical\n'
        printf 'wired_device_count=1\n'
        printf 'wired_active_count=1\n'
        printf 'wifi_device_count=0\n'
        printf 'wifi_active_count=0\n'
        printf 'default_route=yes\n'
        printf 'dns_configured=yes\n'
        printf 'audio_device_count=1\n'
        printf 'mixer_available=yes\n'
        printf 'mixer_readable=yes\n'
        printf 'input_device_count=2\n'
        printf 'keyboard_available=yes\n'
        printf 'pointer_available=yes\n'
        printf 'acpi_available=yes\n'
        printf 'suspend_command_available=yes\n'
        printf 'capability_status=ready\n'
        printf 'capability_blockers=none\n'
        printf 'observations=validated\n'
        printf 'manual_pass_count=6\n'
        printf 'manual_fail_count=0\n'
        printf 'manual_pending_count=0\n'
        printf 'manual_deferred_count=0\n'
        printf 'platform_status=%s\n' "$platform_status"
    } > "$PLATFORM"
}

run_matrix() {
    NORTHSTAR_MATRIX_TEST_MODE=1 \
    NORTHSTAR_ALPHA_TEST_MODE=1 \
    NORTHSTAR_ALPHA_TEST_RELEASE=${TEST_RELEASE:-15.1-RELEASE-p2} \
    NORTHSTAR_ALPHA_TEST_ARCH=amd64 \
    NORTHSTAR_ALPHA_TEST_BOOT=UEFI \
    NORTHSTAR_ALPHA_TEST_ROOTFS=zfs \
    NORTHSTAR_ALPHA_TEST_PLATFORM=${TEST_PLATFORM:-virtual-machine} \
    NORTHSTAR_ALPHA_TEST_DRM_CARDS=${TEST_DRM_CARDS:-0} \
    NORTHSTAR_ALPHA_TEST_DRM_RENDER=${TEST_DRM_RENDER:-0} \
    NORTHSTAR_ALPHA_TEST_DRM_DRIVER=${TEST_DRM_DRIVER:-none} \
    NORTHSTAR_ALPHA_TEST_WIRED=1 \
    NORTHSTAR_ALPHA_TEST_AUDIO=1 \
    NORTHSTAR_ALPHA_TEST_INPUT=2 \
    NORTHSTAR_MATRIX_TEST_SHELL=${TEST_SHELL:-yes} \
    NORTHSTAR_MATRIX_TEST_SESSION=${TEST_SESSION:-yes} \
    NORTHSTAR_MATRIX_TEST_DESKTOP_ENTRY=${TEST_DESKTOP_ENTRY:-yes} \
    NORTHSTAR_MATRIX_TEST_FIREFOX=${TEST_FIREFOX:-yes} \
    NORTHSTAR_MATRIX_TEST_TERMINAL=${TEST_TERMINAL:-yes} \
    NORTHSTAR_MATRIX_TEST_DIAGNOSTICS=${TEST_DIAGNOSTICS:-yes} \
    NORTHSTAR_MATRIX_TEST_DISPLAY_MANAGER=${TEST_DISPLAY_MANAGER:-yes} \
        sh "$ROOT/tools/run-alpha-matrix.sh" --output "$OUTPUT" "$@"
}

run_matrix --lane vm --write-template "$TEMPLATE"
contains 'expected_lane=vm' "$OUTPUT"
contains 'hardware_status=supplemental' "$OUTPUT"
contains 'matrix_status=inventory-only' "$OUTPUT"
contains 'schema_version=1' "$TEMPLATE"
contains 'clean_shutdown=pending' "$TEMPLATE"
[ "$(stat -f '%Lp' "$TEMPLATE" 2>/dev/null || stat -c '%a' "$TEMPLATE")" = 600 ] || fail 'template mode is not 0600'

write_observations
write_platform
run_matrix --lane vm --observations "$OBSERVATIONS"
contains 'matrix_status=supplemental' "$OUTPUT"
contains 'manual_pass_count=14' "$OUTPUT"

TEST_PLATFORM=physical TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 TEST_DRM_DRIVER=intel \
    run_matrix --lane intel --observations "$OBSERVATIONS" --platform-evidence "$PLATFORM" --require-pass
contains 'hardware_claim=intel' "$OUTPUT"
contains 'preflight_status=pass' "$OUTPUT"
contains 'platform_evidence=validated' "$OUTPUT"
contains 'platform_status=pass' "$OUTPUT"
contains 'matrix_status=pass' "$OUTPUT"

write_observations firefox fail
if TEST_PLATFORM=physical TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 TEST_DRM_DRIVER=intel \
    run_matrix --lane intel --observations "$OBSERVATIONS" --platform-evidence "$PLATFORM" --require-pass; then
    fail 'failed manual observation satisfied --require-pass'
fi
contains 'matrix_status=fail' "$OUTPUT"

write_observations graphical_login pending
if TEST_PLATFORM=physical TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 TEST_DRM_DRIVER=intel \
    run_matrix --lane intel --observations "$OBSERVATIONS" --platform-evidence "$PLATFORM" --require-pass; then
    fail 'pending manual observation satisfied --require-pass'
fi
contains 'matrix_status=pending' "$OUTPUT"

write_observations update_rollback deferred
if TEST_PLATFORM=physical TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 TEST_DRM_DRIVER=intel \
    run_matrix --lane intel --observations "$OBSERVATIONS" --platform-evidence "$PLATFORM" --require-pass; then
    fail 'deferred manual observation satisfied --require-pass'
fi
contains 'matrix_status=partial' "$OUTPUT"

write_observations
if TEST_PLATFORM=physical TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 TEST_DRM_DRIVER=intel \
    TEST_SHELL=no run_matrix --lane intel --observations "$OBSERVATIONS" --platform-evidence "$PLATFORM" --require-pass; then
    fail 'missing application preflight satisfied --require-pass'
fi
contains 'preflight_blockers=shell' "$OUTPUT"
contains 'matrix_status=blocked' "$OUTPUT"

write_observations
if TEST_PLATFORM=physical TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 TEST_DRM_DRIVER=amd \
    run_matrix --lane intel --observations "$OBSERVATIONS" --platform-evidence "$PLATFORM" --require-pass; then
    fail 'AMD hardware satisfied the Intel lane'
fi
contains 'preflight_blockers=lane_mismatch' "$OUTPUT"
contains 'matrix_status=blocked' "$OUTPUT"

write_observations
write_platform partial
if TEST_PLATFORM=physical TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 TEST_DRM_DRIVER=intel \
    run_matrix --lane intel --observations "$OBSERVATIONS" --platform-evidence "$PLATFORM" --require-pass; then
    fail 'partial platform evidence satisfied physical --require-pass'
fi
contains 'preflight_blockers=platform_evidence' "$OUTPUT"
contains 'matrix_status=blocked' "$OUTPUT"

write_platform
if TEST_PLATFORM=physical TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 TEST_DRM_DRIVER=intel \
    run_matrix --lane intel --observations "$OBSERVATIONS" --require-pass; then
    fail 'missing platform evidence satisfied physical --require-pass'
fi
contains 'preflight_blockers=platform_evidence' "$OUTPUT"
contains 'matrix_status=blocked' "$OUTPUT"

printf 'unknown=pass\n' >> "$OBSERVATIONS"
if TEST_PLATFORM=physical TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 TEST_DRM_DRIVER=intel \
    run_matrix --lane intel --observations "$OBSERVATIONS"; then
    fail 'unknown observation record was accepted'
fi

[ "$(stat -f '%Lp' "$OUTPUT" 2>/dev/null || stat -c '%a' "$OUTPUT")" = 600 ] || fail 'matrix output mode is not 0600'
if grep -E 'secret|MAC|serial|command|/home/' "$OUTPUT" >/dev/null 2>&1; then
    fail 'matrix output contains a forbidden private-data marker'
fi

printf '%s\n' 'PASS: M6 alpha matrix runner enforces lane, observation, privacy, and pass boundaries'
