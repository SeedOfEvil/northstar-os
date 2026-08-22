#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
HELPER=$ROOT/apps/bluetooth/northstar-bluetooth-configure
POLICY=$ROOT/packaging/polkit/org.northstar.bluetooth.policy
TMP=$(mktemp -d "${TMPDIR:-/tmp}/northstar-bluetooth-configure.XXXXXX")
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
fail(){ printf 'FAIL: %s\n' "$1" >&2; exit 1; }

FAKE=$TMP/root
mkdir -p "$FAKE/etc/bluetooth" "$TMP/bin"
printf '%s\t%s\n' 'aa:bb:cc:dd:ee:ff' 'Old_Phone' > "$FAKE/etc/bluetooth/hosts"
cat > "$FAKE/etc/bluetooth/hcsecd.conf" <<'EOF'
device {
    bdaddr aa:bb:cc:dd:ee:ff;
    name "Old Phone";
    key nokey;
    pin nopin;
}
EOF
cat > "$TMP/request" <<'EOF'
protocol=1
address_hex=aabbccddeeff
name_hex=546573742050686f6e65
EOF
chmod 600 "$TMP/request"

cat > "$TMP/bin/sysrc" <<'EOF'
#!/bin/sh
printf 'sysrc=%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
EOF
cat > "$TMP/bin/service" <<'EOF'
#!/bin/sh
printf 'service=%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
EOF
cat > "$TMP/bin/hccontrol" <<'EOF'
#!/bin/sh
printf 'hccontrol=%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
case "$*" in
  "-n ubt0hci create_connection aa:bb:cc:dd:ee:ff"*) exit 0;;
  *) exit 1;;
esac
EOF
chmod +x "$TMP/bin/sysrc" "$TMP/bin/service" "$TMP/bin/hccontrol"

run_helper() {
    env NORTHSTAR_BLUETOOTH_TEST_MODE=1 \
        NORTHSTAR_BLUETOOTH_ROOT="$FAKE" \
        NORTHSTAR_BLUETOOTH_SYSRC_PATH="$TMP/bin/sysrc" \
        NORTHSTAR_BLUETOOTH_SERVICE_PATH="$TMP/bin/service" \
        NORTHSTAR_BLUETOOTH_HCCONTROL_PATH="$TMP/bin/hccontrol" \
        NORTHSTAR_TEST_EVENTS="$TMP/events" \
        sh "$HELPER" --pair "$TMP/request"
}
out=$(printf '%s\n' 654321 | run_helper)
printf '%s\n' "$out" | grep -Fx 'NORTHSTAR_BLUETOOTH_AUTHORIZED=1' >/dev/null ||
    fail 'authorization marker missing'
printf '%s\n' "$out" | grep -Fx 'NORTHSTAR_BLUETOOTH_PAIR_REQUEST=PASS' >/dev/null ||
    fail 'pairing marker missing'
grep -Fx 'aa:bb:cc:dd:ee:ff	Test_Phone' "$FAKE/etc/bluetooth/hosts" >/dev/null ||
    fail 'remembered host was not replaced'
[ "$(grep -ci 'bdaddr[[:space:]]*aa:bb:cc:dd:ee:ff' "$FAKE/etc/bluetooth/hcsecd.conf")" -eq 1 ] ||
    fail 'pairing configuration was duplicated'
grep -F 'pin	"654321";' "$FAKE/etc/bluetooth/hcsecd.conf" >/dev/null ||
    fail 'pairing PIN was not installed'
grep -F 'service=hcsecd onerestart' "$TMP/events" >/dev/null ||
    fail 'hcsecd was not restarted'
grep -F 'hccontrol=-n ubt0hci create_connection aa:bb:cc:dd:ee:ff' "$TMP/events" >/dev/null ||
    fail 'pairing connection was not initiated'
if grep -Eiq 'pin|password|secret|key' "$TMP/request"; then
    fail 'credential material was written to the request'
fi

cp "$TMP/request" "$TMP/bad-request"
printf '%s\n' 'pin=654321' >> "$TMP/bad-request"
mv "$TMP/bad-request" "$TMP/request"
chmod 600 "$TMP/request"
before=$(sha256 -q "$FAKE/etc/bluetooth/hcsecd.conf")
if printf '%s\n' 654321 | run_helper >/dev/null 2>&1; then
    fail 'credential field in request was accepted'
fi
[ "$(sha256 -q "$FAKE/etc/bluetooth/hcsecd.conf")" = "$before" ] ||
    fail 'invalid request changed the pairing configuration'

grep -F '/usr/local/libexec/northstar-bluetooth-configure' "$POLICY" >/dev/null ||
    fail 'PolicyKit action does not pin the Bluetooth helper'
grep -F '<allow_active>auth_admin_keep</allow_active>' "$POLICY" >/dev/null ||
    fail 'Bluetooth pairing does not require active administrator authorization'
printf '%s\n' 'PASS: protected Bluetooth pairing validates requests, keeps PIN off the request and argv, and replaces remembered state'
