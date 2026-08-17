#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-platform-evidence-test.XXXXXX")
OUTPUT=$TMP_DIR/platform.conf
OBSERVATIONS=$TMP_DIR/observations.conf
TEMPLATE=$TMP_DIR/template.conf

cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT HUP INT TERM
fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
contains() { grep -Fx "$1" "$2" >/dev/null 2>&1 || fail "missing $1"; }

write_observations() {
    override_key=${1:-none}
    override_value=${2:-pass}
    {
        printf 'schema_version=1\n'
        for key in network_connectivity audio_playback volume_control keyboard_input pointer_input suspend_resume; do
            value=pass
            [ "$key" != "$override_key" ] || value=$override_value
            printf '%s=%s\n' "$key" "$value"
        done
    } > "$OBSERVATIONS"
}

run_collector() {
    NORTHSTAR_PLATFORM_TEST_MODE=1 \
    NORTHSTAR_PLATFORM_TEST_CLASS=${TEST_CLASS:-virtual-machine} \
    NORTHSTAR_PLATFORM_TEST_WIRED_DEVICES=${TEST_WIRED_DEVICES:-1} \
    NORTHSTAR_PLATFORM_TEST_WIRED_ACTIVE=${TEST_WIRED_ACTIVE:-1} \
    NORTHSTAR_PLATFORM_TEST_WIFI_DEVICES=${TEST_WIFI_DEVICES:-0} \
    NORTHSTAR_PLATFORM_TEST_WIFI_ACTIVE=${TEST_WIFI_ACTIVE:-0} \
    NORTHSTAR_PLATFORM_TEST_DEFAULT_ROUTE=${TEST_DEFAULT_ROUTE:-yes} \
    NORTHSTAR_PLATFORM_TEST_DNS=${TEST_DNS:-yes} \
    NORTHSTAR_PLATFORM_TEST_AUDIO_DEVICES=${TEST_AUDIO_DEVICES:-1} \
    NORTHSTAR_PLATFORM_TEST_MIXER_AVAILABLE=${TEST_MIXER_AVAILABLE:-yes} \
    NORTHSTAR_PLATFORM_TEST_MIXER_READABLE=${TEST_MIXER_READABLE:-yes} \
    NORTHSTAR_PLATFORM_TEST_INPUT_DEVICES=${TEST_INPUT_DEVICES:-2} \
    NORTHSTAR_PLATFORM_TEST_KEYBOARD=${TEST_KEYBOARD:-yes} \
    NORTHSTAR_PLATFORM_TEST_POINTER=${TEST_POINTER:-yes} \
    NORTHSTAR_PLATFORM_TEST_ACPI=${TEST_ACPI:-yes} \
    NORTHSTAR_PLATFORM_TEST_SUSPEND_COMMAND=${TEST_SUSPEND_COMMAND:-yes} \
        sh "$ROOT/tools/collect-platform-evidence.sh" --output "$OUTPUT" "$@"
}

run_collector --write-template "$TEMPLATE"
contains 'platform_class=virtual-machine' "$OUTPUT"
contains 'capability_status=supplemental' "$OUTPUT"
contains 'platform_status=inventory-only' "$OUTPUT"
contains 'suspend_resume=pending' "$TEMPLATE"
[ "$(stat -f '%Lp' "$OUTPUT" 2>/dev/null || stat -c '%a' "$OUTPUT")" = 600 ] || fail 'output mode is not 0600'
[ "$(stat -f '%Lp' "$TEMPLATE" 2>/dev/null || stat -c '%a' "$TEMPLATE")" = 600 ] || fail 'template mode is not 0600'

write_observations
run_collector --observations "$OBSERVATIONS"
contains 'platform_status=supplemental' "$OUTPUT"
contains 'manual_pass_count=6' "$OUTPUT"

TEST_CLASS=physical run_collector --observations "$OBSERVATIONS" --require-pass
contains 'capability_status=ready' "$OUTPUT"
contains 'platform_status=pass' "$OUTPUT"

write_observations audio_playback fail
if TEST_CLASS=physical run_collector --observations "$OBSERVATIONS" --require-pass; then
    fail 'failed observation satisfied --require-pass'
fi
contains 'platform_status=fail' "$OUTPUT"

write_observations suspend_resume deferred
if TEST_CLASS=physical run_collector --observations "$OBSERVATIONS" --require-pass; then
    fail 'deferred observation satisfied --require-pass'
fi
contains 'platform_status=partial' "$OUTPUT"

write_observations
if TEST_CLASS=physical TEST_AUDIO_DEVICES=0 run_collector --observations "$OBSERVATIONS" --require-pass; then
    fail 'missing audio device satisfied --require-pass'
fi
contains 'capability_status=blocked' "$OUTPUT"
contains 'capability_blockers=audio_device' "$OUTPUT"
contains 'platform_status=blocked' "$OUTPUT"

write_observations
if TEST_CLASS=physical TEST_WIRED_ACTIVE=0 TEST_DEFAULT_ROUTE=no TEST_DNS=no \
    run_collector --observations "$OBSERVATIONS" --require-pass; then
    fail 'missing network capability satisfied --require-pass'
fi
contains 'capability_blockers=active_network,default_route,dns' "$OUTPUT"

printf 'unknown=pass\n' >> "$OBSERVATIONS"
if TEST_CLASS=physical run_collector --observations "$OBSERVATIONS"; then
    fail 'unknown observation record was accepted'
fi

if grep -E 'secret|MAC|serial|command_line|interface_name|/home/' "$OUTPUT" >/dev/null 2>&1; then
    fail 'platform output contains a forbidden private-data marker'
fi

printf '%s\n' 'PASS: M6 platform evidence enforces passive capability, observation, privacy, and pass boundaries'
