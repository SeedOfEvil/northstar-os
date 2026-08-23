#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
HELPER=$ROOT/apps/bluetooth/northstar-bluetooth-scan
TMP=$(mktemp -d "${TMPDIR:-/tmp}/northstar-bluetooth-scan.XXXXXX")
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
fail(){ printf 'FAIL: %s\n' "$1" >&2; exit 1; }

cat > "$TMP/hccontrol" <<'EOF'
#!/bin/sh
case "$*" in
  "-n ubt0hci read_node_list") printf '%s\n' 'Name ID Num hooks' 'ubt0hci 1 3';;
  "-N -n ubt0hci inquiry") printf '%s\n' 'Inquiry result #0' '       BD_ADDR: aa:bb:cc:dd:ee:ff' 'Inquiry complete. Status: No error [00]';;
  "-n ubt0hci read_connection_list") printf '%s\n' 'Remote BD_ADDR    Handle Type Mode Role Encrypt Pending Queue State' '11:22:33:44:55:66 41 ACL 0 MAST NONE 0 0 OPEN';;
  "-n ubt0hci read_scan_enable") printf '%s\n' 'Scan enable: Inquiry Scan enabled. Page Scan enabled [0x3]';;
  "-n ubt0hci remote_name_request aa:bb:cc:dd:ee:ff") printf '%s\n' 'Name: Test Mouse';;
  *) exit 1;;
esac
EOF
chmod +x "$TMP/hccontrol"
printf '%s\t%s\n' '11:22:33:44:55:66' 'Test_Phone' > "$TMP/hosts"
printf '%s\n' '11:22:33:44:55:66' > "$TMP/paired"
out=$(env NORTHSTAR_BLUETOOTH_HCCONTROL_PATH="$TMP/hccontrol" \
    NORTHSTAR_BLUETOOTH_HOSTS_PATH="$TMP/hosts" \
    NORTHSTAR_BLUETOOTH_PAIRED_PATH="$TMP/paired" sh "$HELPER")
printf '%s\n' "$out" | grep -Fx 'NORTHSTAR_BLUETOOTH_SCAN=1' >/dev/null ||
    fail 'scan marker missing'
printf '%s\n' "$out" | grep -Fx 'discoverable=1' >/dev/null ||
    fail 'discoverability state missing'
printf '%s\n' "$out" |
    grep -Fx 'device=aabbccddeeff|54657374204d6f757365|0|0|0' >/dev/null ||
    fail 'discoverable device record was not encoded'
printf '%s\n' "$out" |
    grep -Fx 'device=112233445566|546573745f50686f6e65|1|1|1' >/dev/null ||
    fail 'remembered connected device state was not encoded'
if env NORTHSTAR_BLUETOOTH_HCCONTROL_PATH="$TMP/hccontrol" \
    NORTHSTAR_BLUETOOTH_NODE='bad;node' sh "$HELPER" >/dev/null 2>&1; then
    fail 'invalid controller name was accepted'
fi
printf '%s\n' 'PASS: Bluetooth scanner reports remembered, paired, and connected device state'
