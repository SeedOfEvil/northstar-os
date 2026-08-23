#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
HELPER=$ROOT/apps/bluetooth/northstar-bluetooth-configure
POLICY=$ROOT/packaging/polkit/org.northstar.bluetooth.policy
TMP=$(mktemp -d "${TMPDIR:-/tmp}/northstar-bluetooth-configure.XXXXXX")
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
fail(){ printf 'FAIL: %s\n' "$1" >&2; exit 1; }

FAKE=$TMP/root
mkdir -p "$FAKE/etc/bluetooth" "$FAKE/var/db/northstar" \
    "$FAKE/var/run" "$TMP/bin"
printf '%s\n' 123 > "$FAKE/var/run/hcsecd.pid"
printf '%s\t%s\n' 'aa:bb:cc:dd:ee:ff' 'Old_Phone' > "$FAKE/etc/bluetooth/hosts"
cat > "$FAKE/etc/bluetooth/hcsecd.conf" <<'EOF'
device {
    bdaddr aa:bb:cc:dd:ee:ff;
    name "Old Phone";
    key nokey;
    pin nopin;
}
EOF
cat > "$FAKE/var/db/hcsecd.keys" <<'EOF'
aa:bb:cc:dd:ee:ff 00000000000000000000000000000000
11:22:33:44:55:66 11111111111111111111111111111111
EOF
printf '%s\n' 'aa:bb:cc:dd:ee:ff' '11:22:33:44:55:66' > \
    "$FAKE/var/db/northstar/bluetooth-paired"
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
cat > "$TMP/bin/kill" <<'EOF'
#!/bin/sh
printf 'kill=%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
[ "$*" = '-HUP 123' ] || exit 1
if [ -f "$NORTHSTAR_TEST_PENDING" ]; then
    cat "$NORTHSTAR_TEST_PENDING" >> \
        "$NORTHSTAR_BLUETOOTH_ROOT/var/db/hcsecd.keys"
    rm -f "$NORTHSTAR_TEST_PENDING"
fi
EOF
cat > "$TMP/bin/hccontrol" <<'EOF'
#!/bin/sh
printf 'hccontrol=%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
case "$*" in
  "-n ubt0hci write_scan_enable 3") exit 0;;
  "-n ubt0hci read_connection_list") printf '%s\n' 'Remote BD_ADDR Handle' 'aa:bb:cc:dd:ee:ff 41';;
  "-n ubt0hci disconnect 41 0x16") exit 0;;
  "-n ubt0hci delete_stored_link_key aa:bb:cc:dd:ee:ff") exit 0;;
  *) exit 1;;
esac
EOF
cat > "$TMP/bin/northstar-bluetooth-ssp" <<'EOF'
#!/bin/sh
[ "$1" = --listen ] && [ "$2" = ubt0hci ] \
    && [ "$3" = aa:bb:cc:dd:ee:ff ] || exit 65
if grep -qi 'aa:bb:cc:dd:ee:ff' "$NORTHSTAR_BLUETOOTH_ROOT/var/db/hcsecd.keys"; then
    printf '%s\n' 'ERROR: stale target link key reached the SSP agent' >&2
    exit 65
fi
printf '%s\n' NORTHSTAR_BLUETOOTH_CONFIRM=654321
IFS= read -r decision
[ "$decision" = accept ] || exit 125
cat > "$NORTHSTAR_TEST_PENDING" <<'KEYEOF'
aa:bb:cc:dd:ee:ff aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
KEYEOF
printf '%s\n' NORTHSTAR_BLUETOOTH_PAIRED=CONFIRMED
EOF
chmod +x "$TMP/bin/sysrc" "$TMP/bin/service" "$TMP/bin/hccontrol" \
    "$TMP/bin/kill" "$TMP/bin/northstar-bluetooth-ssp"

run_helper() {
    env NORTHSTAR_BLUETOOTH_TEST_MODE=1 \
        NORTHSTAR_BLUETOOTH_ROOT="$FAKE" \
        NORTHSTAR_BLUETOOTH_SYSRC_PATH="$TMP/bin/sysrc" \
        NORTHSTAR_BLUETOOTH_SERVICE_PATH="$TMP/bin/service" \
        NORTHSTAR_BLUETOOTH_KILL_PATH="$TMP/bin/kill" \
        NORTHSTAR_BLUETOOTH_HCCONTROL_PATH="$TMP/bin/hccontrol" \
        NORTHSTAR_BLUETOOTH_SSP_PATH="$TMP/bin/northstar-bluetooth-ssp" \
        NORTHSTAR_TEST_PENDING="$TMP/pending-link-key" \
        NORTHSTAR_TEST_EVENTS="$TMP/events" \
        sh "$HELPER" "$@"
}
out=$(printf '%s\n' accept | run_helper --pair "$TMP/request")
printf '%s\n' "$out" | grep -Fx 'NORTHSTAR_BLUETOOTH_AUTHORIZED=1' >/dev/null ||
    fail 'authorization marker missing'
printf '%s\n' "$out" | grep -Fx 'NORTHSTAR_BLUETOOTH_CONFIRM=654321' >/dev/null ||
    fail 'numeric confirmation marker missing'
printf '%s\n' "$out" | grep -Fx 'NORTHSTAR_BLUETOOTH_PAIR=PASS' >/dev/null ||
    fail 'pairing marker missing'
grep -Fx 'aa:bb:cc:dd:ee:ff	Test_Phone' "$FAKE/etc/bluetooth/hosts" >/dev/null ||
    fail 'remembered host was not replaced'
[ "$(grep -ci 'bdaddr[[:space:]]*aa:bb:cc:dd:ee:ff' "$FAKE/etc/bluetooth/hcsecd.conf")" -eq 1 ] ||
    fail 'pairing configuration was duplicated'
grep -F 'pin	nopin;' "$FAKE/etc/bluetooth/hcsecd.conf" >/dev/null ||
    fail 'legacy PIN authentication was not disabled for SSP'
grep -F 'service=hcsecd onerestart' "$TMP/events" >/dev/null ||
    fail 'hcsecd was not restarted'
grep -F 'kill=-HUP 123' "$TMP/events" >/dev/null ||
    fail 'hcsecd was not signalled to persist its cached link key'
grep -F 'hccontrol=-n ubt0hci delete_stored_link_key aa:bb:cc:dd:ee:ff' \
    "$TMP/events" >/dev/null || fail 'stale controller link key was not deleted'
grep -F 'aa:bb:cc:dd:ee:ff aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' \
    "$FAKE/var/db/hcsecd.keys" >/dev/null ||
    fail 'fresh SSP link key was not persisted'
grep -Fx 'aa:bb:cc:dd:ee:ff' "$FAKE/var/db/northstar/bluetooth-paired" >/dev/null ||
    fail 'persisted paired state was not recorded'
if grep -Eiq 'pin|password|secret|key' "$TMP/request"; then
    fail 'credential material was written to the request'
fi

cp "$TMP/request" "$TMP/bad-request"
printf '%s\n' 'pin=654321' >> "$TMP/bad-request"
mv "$TMP/bad-request" "$TMP/request"
chmod 600 "$TMP/request"
before=$(sha256 -q "$FAKE/etc/bluetooth/hcsecd.conf")
if printf '%s\n' accept | run_helper --pair "$TMP/request" >/dev/null 2>&1; then
    fail 'credential field in request was accepted'
fi
[ "$(sha256 -q "$FAKE/etc/bluetooth/hcsecd.conf")" = "$before" ] ||
    fail 'invalid request changed the pairing configuration'

visibility=$(run_helper --discoverable on)
printf '%s\n' "$visibility" |
    grep -Fx 'NORTHSTAR_BLUETOOTH_DISCOVERABLE=on' >/dev/null ||
    fail 'discoverability was not enabled'
grep -F 'hccontrol=-n ubt0hci write_scan_enable 3' "$TMP/events" >/dev/null ||
    fail 'controller was not made discoverable and connectable'

cat > "$TMP/request" <<'EOF'
protocol=1
address_hex=aabbccddeeff
EOF
chmod 600 "$TMP/request"
forgot=$(run_helper --forget "$TMP/request")
printf '%s\n' "$forgot" | grep -Fx 'NORTHSTAR_BLUETOOTH_FORGET=PASS' >/dev/null ||
    fail 'forget marker missing'
if grep -qi 'aa:bb:cc:dd:ee:ff' "$FAKE/etc/bluetooth/hosts" \
    "$FAKE/etc/bluetooth/hcsecd.conf" "$FAKE/var/db/hcsecd.keys" \
    "$FAKE/var/db/northstar/bluetooth-paired"; then
    fail 'selected device state was not fully removed'
fi
grep -qi '11:22:33:44:55:66' "$FAKE/var/db/hcsecd.keys" ||
    fail 'forget removed another device key'
grep -qi '11:22:33:44:55:66' "$FAKE/var/db/northstar/bluetooth-paired" ||
    fail 'forget removed another paired-state entry'
grep -F 'hccontrol=-n ubt0hci disconnect 41 0x16' "$TMP/events" >/dev/null ||
    fail 'active selected-device connection was not disconnected'
grep -F 'hccontrol=-n ubt0hci delete_stored_link_key aa:bb:cc:dd:ee:ff' \
    "$TMP/events" >/dev/null || fail 'controller link key was not deleted'

grep -F '/usr/local/libexec/northstar-bluetooth-configure' "$POLICY" >/dev/null ||
    fail 'PolicyKit action does not pin the Bluetooth helper'
grep -F '<allow_active>auth_admin_keep</allow_active>' "$POLICY" >/dev/null ||
    fail 'Bluetooth management does not require active administrator authorization'
printf '%s\n' 'PASS: protected Bluetooth management pairs, changes visibility, and forgets only the selected device'
