#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
HELPER=$ROOT/apps/bluetooth/northstar-bluetooth-receive
POLICY=$ROOT/packaging/polkit/org.northstar.bluetooth.policy
TMP=$(mktemp -d "${TMPDIR:-/tmp}/northstar-bluetooth-receive.XXXXXX")
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

mkdir -p "$TMP/root/home/tester" "$TMP/bin" "$TMP/run"
cat > "$TMP/bin/obexapp" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
EOF
chmod +x "$TMP/bin/obexapp"
cat > "$TMP/bin/hccontrol" <<'EOF'
#!/bin/sh
case "$3" in
    read_class_of_device) printf '%s\n' 'Class of device: ff:01:0c';;
    write_class_of_device) printf 'class=%s\n' "$4" >> "$NORTHSTAR_TEST_EVENTS";;
    *) exit 64;;
esac
EOF
chmod +x "$TMP/bin/hccontrol"

out=$(env NORTHSTAR_BLUETOOTH_TEST_MODE=1 \
    NORTHSTAR_BLUETOOTH_ROOT="$TMP/root" \
    NORTHSTAR_BLUETOOTH_TEST_USER=tester \
    NORTHSTAR_BLUETOOTH_OBEX_PATH="$TMP/bin/obexapp" \
    NORTHSTAR_BLUETOOTH_HCCONTROL_PATH="$TMP/bin/hccontrol" \
    NORTHSTAR_BLUETOOTH_RUNTIME_ROOT="$TMP/run" \
    NORTHSTAR_TEST_EVENTS="$TMP/events" \
    sh "$HELPER" --start)
printf '%s\n' "$out" | grep -Fx NORTHSTAR_BLUETOOTH_AUTHORIZED=1 >/dev/null ||
    fail 'authorization marker missing'
grep -Fx -- "-s -d -S -u tester -r $TMP/root/home/tester/Downloads" \
    "$TMP/events" >/dev/null || fail 'secure user-scoped OBEX server arguments are wrong'
grep -Fx 'class=0c:01:10' "$TMP/events" >/dev/null ||
    fail 'Laptop and Object Transfer class was not enabled'
grep -Fx 'class=ff:01:0c' "$TMP/events" >/dev/null ||
    fail 'prior Bluetooth class was not restored'
[ -d "$TMP/root/home/tester/Downloads" ] || fail 'Downloads folder was not prepared'
if env NORTHSTAR_BLUETOOTH_TEST_MODE=1 \
    NORTHSTAR_BLUETOOTH_ROOT="$TMP/root" \
    NORTHSTAR_BLUETOOTH_TEST_USER='../root' \
    NORTHSTAR_BLUETOOTH_OBEX_PATH="$TMP/bin/obexapp" \
    sh "$HELPER" --start >/dev/null 2>&1; then
    fail 'unsafe desktop user was accepted'
fi
grep -F '/usr/local/libexec/northstar-bluetooth-receive' "$POLICY" >/dev/null ||
    fail 'PolicyKit action does not pin the receive helper'
grep -F '<allow_active>auth_admin_keep</allow_active>' "$POLICY" >/dev/null ||
    fail 'Bluetooth receiving does not require active administrator authorization'
printf '%s\n' 'PASS: Bluetooth receiving registers through a protected user-scoped helper'
