#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-alpha-bundle-test.XXXXXX")
MATRIX_OBS=$TMP_DIR/matrix-observations.conf
PLATFORM_OBS=$TMP_DIR/platform-observations.conf

cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT HUP INT TERM
fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
contains() { grep -Fx "$1" "$2" >/dev/null 2>&1 || fail "missing $1"; }

write_observations() {
    {
        printf 'schema_version=1\n'
        for key in graphical_login direct_compositor display_output native_qt xwayland firefox files settings networking audio input shell_recovery update_rollback clean_shutdown; do
            printf '%s=pass\n' "$key"
        done
    } > "$MATRIX_OBS"
    {
        printf 'schema_version=1\n'
        for key in network_connectivity audio_playback volume_control keyboard_input pointer_input suspend_resume; do
            printf '%s=pass\n' "$key"
        done
    } > "$PLATFORM_OBS"
}

run_bundle() {
    NORTHSTAR_ALPHA_TEST_MODE=1 \
    NORTHSTAR_ALPHA_TEST_RELEASE=15.1-RELEASE-p2 \
    NORTHSTAR_ALPHA_TEST_ARCH=amd64 \
    NORTHSTAR_ALPHA_TEST_BOOT=UEFI \
    NORTHSTAR_ALPHA_TEST_ROOTFS=zfs \
    NORTHSTAR_ALPHA_TEST_PLATFORM=${TEST_ALPHA_PLATFORM:-virtual-machine} \
    NORTHSTAR_ALPHA_TEST_DRM_CARDS=${TEST_DRM_CARDS:-0} \
    NORTHSTAR_ALPHA_TEST_DRM_RENDER=${TEST_DRM_RENDER:-0} \
    NORTHSTAR_ALPHA_TEST_DRM_DRIVER=${TEST_DRM_DRIVER:-none} \
    NORTHSTAR_ALPHA_TEST_WIRED=1 \
    NORTHSTAR_ALPHA_TEST_AUDIO=1 \
    NORTHSTAR_ALPHA_TEST_INPUT=2 \
    NORTHSTAR_PLATFORM_TEST_MODE=1 \
    NORTHSTAR_PLATFORM_TEST_CLASS=${TEST_PLATFORM_CLASS:-virtual-machine} \
    NORTHSTAR_MATRIX_TEST_MODE=1 \
        sh "$ROOT/tools/build-alpha-evidence-bundle.sh" "$@"
}

write_observations
VM_BUNDLE=$TMP_DIR/vm-bundle
run_bundle --lane vm --output "$VM_BUNDLE"
contains 'bundle_status=supplemental' "$VM_BUNDLE/bundle.conf"
contains 'lane=vm' "$VM_BUNDLE/bundle.conf"
[ "$(find "$VM_BUNDLE" -maxdepth 1 -type f -print | awk 'END { print NR + 0 }')" -eq 5 ] || fail 'VM bundle file count differs'
run_bundle --verify "$VM_BUNDLE"
if run_bundle --verify "$VM_BUNDLE" --require-pass; then fail 'supplemental VM bundle satisfied --require-pass'; fi

INTEL_BUNDLE=$TMP_DIR/intel-bundle
TEST_ALPHA_PLATFORM=physical TEST_PLATFORM_CLASS=physical TEST_DRM_CARDS=1 TEST_DRM_RENDER=1 TEST_DRM_DRIVER=intel \
    run_bundle --lane intel --output "$INTEL_BUNDLE" \
        --matrix-observations "$MATRIX_OBS" --platform-observations "$PLATFORM_OBS" --require-pass
contains 'bundle_status=pass' "$INTEL_BUNDLE/bundle.conf"
contains 'readiness_claim=intel' "$INTEL_BUNDLE/bundle.conf"
run_bundle --verify "$INTEL_BUNDLE" --require-pass

cp "$INTEL_BUNDLE/alpha-readiness.conf" "$TMP_DIR/readiness.backup"
printf 'tampered=yes\n' >> "$INTEL_BUNDLE/alpha-readiness.conf"
if run_bundle --verify "$INTEL_BUNDLE"; then fail 'tampered readiness record verified'; fi
mv "$TMP_DIR/readiness.backup" "$INTEL_BUNDLE/alpha-readiness.conf"

EXTRA_BUNDLE=$TMP_DIR/extra-bundle
cp -R "$VM_BUNDLE" "$EXTRA_BUNDLE"
chmod 0700 "$EXTRA_BUNDLE"
printf 'unknown\n' > "$EXTRA_BUNDLE/unknown.txt"
chmod 0600 "$EXTRA_BUNDLE/unknown.txt"
if run_bundle --verify "$EXTRA_BUNDLE"; then fail 'unknown bundle file verified'; fi

DUPLICATE_BUNDLE=$TMP_DIR/duplicate-bundle
cp -R "$VM_BUNDLE" "$DUPLICATE_BUNDLE"
chmod 0700 "$DUPLICATE_BUNDLE"
printf 'lane=vm\n' >> "$DUPLICATE_BUNDLE/bundle.conf"
bundle_hash=$(sha256 -q "$DUPLICATE_BUNDLE/bundle.conf" 2>/dev/null || sha256sum "$DUPLICATE_BUNDLE/bundle.conf" | awk '{ print $1 }')
awk -v hash="$bundle_hash" '$2 == "bundle.conf" { print hash "  bundle.conf"; next } { print }' \
    "$DUPLICATE_BUNDLE/SHA256" > "$DUPLICATE_BUNDLE/SHA256.new"
mv "$DUPLICATE_BUNDLE/SHA256.new" "$DUPLICATE_BUNDLE/SHA256"
chmod 0600 "$DUPLICATE_BUNDLE/bundle.conf" "$DUPLICATE_BUNDLE/SHA256"
if run_bundle --verify "$DUPLICATE_BUNDLE"; then fail 'duplicate bundle summary field verified'; fi

if grep -ER 'secret|MAC|serial|interface_name|command_line|/home/' "$VM_BUNDLE" "$INTEL_BUNDLE" >/dev/null 2>&1; then
    fail 'bundle contains a forbidden private-data marker'
fi

if run_bundle --lane vm --output "$VM_BUNDLE"; then fail 'existing output directory was replaced'; fi

printf '%s\n' 'PASS: M6 alpha evidence bundle is atomic, aligned, tamper-evident, private, and pass-gated'
