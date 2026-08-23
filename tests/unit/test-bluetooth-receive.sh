#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
HELPER=$ROOT/apps/bluetooth/northstar-bluetooth-receive
POLICY=$ROOT/packaging/polkit/org.northstar.bluetooth.policy
TMP=$(mktemp -d "${TMPDIR:-/tmp}/northstar-bluetooth-receive.XXXXXX")
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

mkdir -p "$TMP/root/home/tester" "$TMP/bin"
cat > "$TMP/bin/obexapp" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" > "$NORTHSTAR_TEST_EVENTS"
EOF
chmod +x "$TMP/bin/obexapp"

out=$(env NORTHSTAR_BLUETOOTH_TEST_MODE=1 \
    NORTHSTAR_BLUETOOTH_ROOT="$TMP/root" \
    NORTHSTAR_BLUETOOTH_TEST_USER=tester \
    NORTHSTAR_BLUETOOTH_OBEX_PATH="$TMP/bin/obexapp" \
    NORTHSTAR_TEST_EVENTS="$TMP/events" \
    sh "$HELPER")
printf '%s\n' "$out" | grep -Fx NORTHSTAR_BLUETOOTH_AUTHORIZED=1 >/dev/null ||
    fail 'authorization marker missing'
grep -Fx -- "-s -d -S -u tester -r $TMP/root/home/tester/Downloads" \
    "$TMP/events" >/dev/null || fail 'secure user-scoped OBEX server arguments are wrong'
[ -d "$TMP/root/home/tester/Downloads" ] || fail 'Downloads folder was not prepared'
if env NORTHSTAR_BLUETOOTH_TEST_MODE=1 \
    NORTHSTAR_BLUETOOTH_ROOT="$TMP/root" \
    NORTHSTAR_BLUETOOTH_TEST_USER='../root' \
    NORTHSTAR_BLUETOOTH_OBEX_PATH="$TMP/bin/obexapp" \
    sh "$HELPER" >/dev/null 2>&1; then
    fail 'unsafe desktop user was accepted'
fi
grep -F '/usr/local/libexec/northstar-bluetooth-receive' "$POLICY" >/dev/null ||
    fail 'PolicyKit action does not pin the receive helper'
grep -F '<allow_active>auth_admin_keep</allow_active>' "$POLICY" >/dev/null ||
    fail 'Bluetooth receiving does not require active administrator authorization'
printf '%s\n' 'PASS: Bluetooth receiving registers through a protected user-scoped helper'
