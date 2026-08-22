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
  "-n ubt0hci remote_name_request aa:bb:cc:dd:ee:ff") printf '%s\n' 'BD_ADDR: aa:bb:cc:dd:ee:ff' 'Name: Test Keyboard';;
  *) exit 1;;
esac
EOF
chmod +x "$TMP/hccontrol"
out=$(env NORTHSTAR_BLUETOOTH_HCCONTROL_PATH="$TMP/hccontrol" sh "$HELPER")
printf '%s\n' "$out" | grep -Fx 'NORTHSTAR_BLUETOOTH_SCAN=1' >/dev/null || fail 'scan marker missing'
printf '%s\n' "$out" | grep -Fx 'device=aabbccddeeff|54657374204b6579626f617264' >/dev/null || fail 'device record was not encoded'
if env NORTHSTAR_BLUETOOTH_HCCONTROL_PATH="$TMP/hccontrol" NORTHSTAR_BLUETOOTH_NODE='bad;node' sh "$HELPER" >/dev/null 2>&1; then
    fail 'invalid controller name was accepted'
fi
printf '%s\n' 'PASS: Bluetooth scanner validates the controller and encodes device records'
