#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
HELPER=$ROOT/src/power/northstar-radio
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-radio-helper.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

cat > "$TMP_DIR/id" <<'STUB'
#!/bin/sh
printf '%s\n' "$NORTHSTAR_TEST_UID"
STUB
cat > "$TMP_DIR/sudo" <<'STUB'
#!/bin/sh
printf '%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
STUB
cat > "$TMP_DIR/ifconfig" <<'STUB'
#!/bin/sh
if [ "$#" -eq 1 ] && [ "$1" = -l ]; then
    printf '%s\n' "$NORTHSTAR_TEST_INTERFACES"
    exit 0
fi
printf 'ifconfig:%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
STUB
cat > "$TMP_DIR/service" <<'STUB'
#!/bin/sh
printf 'service:%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
STUB
chmod +x "$TMP_DIR/id" "$TMP_DIR/sudo" "$TMP_DIR/ifconfig" "$TMP_DIR/service"

run_helper() {
    env NORTHSTAR_RADIO_ID_PATH="$TMP_DIR/id" \
        NORTHSTAR_RADIO_SUDO_PATH="$TMP_DIR/sudo" \
        NORTHSTAR_RADIO_IFCONFIG_PATH="$TMP_DIR/ifconfig" \
        NORTHSTAR_RADIO_SERVICE_PATH="$TMP_DIR/service" \
        NORTHSTAR_RADIO_SELF_PATH=/usr/local/bin/northstar-radio \
        NORTHSTAR_TEST_UID="${NORTHSTAR_TEST_UID:-1002}" \
        NORTHSTAR_TEST_INTERFACES="${NORTHSTAR_TEST_INTERFACES:-em0 wlan7 lo0}" \
        NORTHSTAR_TEST_EVENTS="$TMP_DIR/events" \
        sh "$HELPER" "$@"
}

wait_for_event() {
    expected=$1
    attempts=0
    while ! grep -Fx "$expected" "$TMP_DIR/events" >/dev/null 2>&1; do
        attempts=$((attempts + 1))
        [ "$attempts" -lt 50 ] || return 1
        sleep 0.01
    done
}

: > "$TMP_DIR/events"
run_helper wifi off
grep -Fx -- '-n /usr/local/bin/northstar-radio wifi off' "$TMP_DIR/events" >/dev/null \
    || fail 'unprivileged Wi-Fi action did not re-enter the exact installed boundary'
[ "$(wc -l < "$TMP_DIR/events" | tr -d ' ')" -eq 1 ] \
    || fail 'unprivileged action reached a mutation command before sudo'

for arguments in 'wifi' 'wifi OFF' 'network on' 'wifi on extra'; do
    if run_helper $arguments >/dev/null 2>&1; then
        fail "malformed action was accepted: $arguments"
    fi
done
[ "$(wc -l < "$TMP_DIR/events" | tr -d ' ')" -eq 1 ] \
    || fail 'malformed action reached sudo'

: > "$TMP_DIR/events"
NORTHSTAR_TEST_UID=0 run_helper wifi off
grep -Fx 'ifconfig:wlan7 down' "$TMP_DIR/events" >/dev/null \
    || fail 'root Wi-Fi off did not resolve and lower the wireless interface'

: > "$TMP_DIR/events"
NORTHSTAR_TEST_UID=0 run_helper wifi on
grep -Fx 'ifconfig:wlan7 up' "$TMP_DIR/events" >/dev/null \
    || fail 'root Wi-Fi on did not resolve and raise the wireless interface'
wait_for_event 'service:wpa_supplicant restart wlan7' \
    || fail 'root Wi-Fi on did not asynchronously restart WPA association'
wait_for_event 'service:dhclient restart wlan7' \
    || fail 'root Wi-Fi on did not asynchronously restart DHCP'

: > "$TMP_DIR/events"
NORTHSTAR_TEST_UID=0 run_helper bluetooth off
grep -Fx 'service:bluetooth stop' "$TMP_DIR/events" >/dev/null \
    || fail 'root Bluetooth off did not stop the bounded service'

if NORTHSTAR_TEST_UID=0 NORTHSTAR_TEST_INTERFACES='em0 lo0' \
    run_helper wifi on >/dev/null 2>&1; then
    fail 'root Wi-Fi action passed without a wireless interface'
else
    status=$?
    [ "$status" -eq 69 ] || fail "absent wireless interface exited $status instead of 69"
fi

printf '%s\n' 'PASS: radio helper validates fixed words before exact self-elevation and root mutation'
