#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
HELPER=$ROOT/apps/wifi/northstar-wifi-configure
POLICY=$ROOT/packaging/polkit/org.northstar.wifi.policy
TMP=$(mktemp -d "${TMPDIR:-/tmp}/northstar-wifi-test.XXXXXX")
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
fail(){ printf 'FAIL: %s\n' "$1" >&2; exit 1; }
grep -Fx '      <allow_any>auth_admin_keep</allow_any>' "$POLICY" >/dev/null || fail 'unclassified graphical sessions cannot request administrator authentication'
FAKE=$TMP/root
mkdir -p "$FAKE/etc" "$TMP/bin"
printf '%s\n' 'user_config=preserve' '# BEGIN NORTHSTAR MANAGED WIFI' 'old-secret-material' '# END NORTHSTAR MANAGED WIFI' > "$FAKE/etc/wpa_supplicant.conf"
cp "$FAKE/etc/wpa_supplicant.conf" "$TMP/original"
cat > "$TMP/bin/sysctl" <<'EOF'
#!/bin/sh
printf '%s\n' iwm0
EOF
cat > "$TMP/bin/sysrc" <<'EOF'
#!/bin/sh
printf 'sysrc:%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
EOF
cat > "$TMP/bin/ifconfig" <<'EOF'
#!/bin/sh
if [ "$1" = -l ]; then [ -f "$NORTHSTAR_TEST_STATE" ] && printf '%s\n' 'em0 wlan0 lo0' || printf '%s\n' 'em0 lo0'; exit; fi
if [ "$#" -ge 2 ] && [ "$2" = create ]; then : > "$NORTHSTAR_TEST_STATE"; printf 'ifconfig:%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"; exit; fi
if [ "$#" -ge 3 ] && [ "$2" = list ] && [ "$3" = scan ]; then
  printf '%s\n' 'SSID/MESH ID    BSSID              CHAN RATE    S:N     INT CAPS' 'Test Net        aa:bb:cc:dd:ee:ff    1   54M -42:-95 100 RSN' 'Cafe            11:22:33:44:55:66    6   54M -70:-95 100'
  exit
fi
if [ "$#" -eq 1 ] && [ "$NORTHSTAR_TEST_ASSOCIATED" = yes ]; then printf '%s\n' 'wlan0: flags=8843<UP,RUNNING>' '    inet 192.0.2.10 netmask 0xffffff00' '    status: associated'; exit; fi
printf 'ifconfig:%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
EOF
cat > "$TMP/bin/service" <<'EOF'
#!/bin/sh
printf 'service:%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
EOF
cat > "$TMP/bin/wpa_passphrase" <<'EOF'
#!/bin/sh
IFS= read -r secret
printf 'network={\n\tssid="%s"\n\t#psk="%s"\n\tpsk=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n}\n' "$1" "$secret"
EOF
cat > "$TMP/bin/sleep" <<'EOF'
#!/bin/sh
exit 0
EOF
chmod +x "$TMP/bin/"*
run(){
  env NORTHSTAR_WIFI_TEST_MODE=1 NORTHSTAR_WIFI_ROOT="$FAKE" NORTHSTAR_WIFI_IFCONFIG_PATH="$TMP/bin/ifconfig" NORTHSTAR_WIFI_SYSCTL_PATH="$TMP/bin/sysctl" NORTHSTAR_WIFI_SYSRC_PATH="$TMP/bin/sysrc" NORTHSTAR_WIFI_SERVICE_PATH="$TMP/bin/service" NORTHSTAR_WIFI_WPA_PASSPHRASE_PATH="$TMP/bin/wpa_passphrase" NORTHSTAR_WIFI_SLEEP_PATH="$TMP/bin/sleep" NORTHSTAR_WIFI_CONNECT_ATTEMPTS=1 NORTHSTAR_TEST_EVENTS="$TMP/events" NORTHSTAR_TEST_STATE="$TMP/state" NORTHSTAR_TEST_ASSOCIATED="${NORTHSTAR_TEST_ASSOCIATED:-yes}" sh "$HELPER" "$@"
}
: > "$TMP/events"
scan=$(run --scan)
printf '%s\n' "$scan" | grep -Fx 'NORTHSTAR_WIFI_SCAN=1' >/dev/null || fail 'scan protocol marker missing'
printf '%s\n' "$scan" | grep -Fx 'network=54657374204e6574|secured|-42' >/dev/null || fail 'secured network was not safely encoded'
printf '%s\n' "$scan" | grep -Fx 'network=43616665|open|-70' >/dev/null || fail 'open network was not safely encoded'
grep -F 'wlans_iwm0=wlan0' "$TMP/events" >/dev/null || fail 'wireless clone was not persisted'
grep -F 'ifconfig_wlan0=WPA SYNCDHCP' "$TMP/events" >/dev/null || fail 'wireless startup policy was not persisted'
cat > "$TMP/request" <<'EOF'
protocol=1
ssid_hex=54657374204e6574
security=secured
EOF
printf '%s\n' 'correct horse' | run --connect "$TMP/request" >/dev/null
config=$FAKE/etc/wpa_supplicant.conf
grep -Fx 'user_config=preserve' "$config" >/dev/null || fail 'unrelated configuration was not preserved'
grep -Fx '    ssid=54657374204e6574' "$config" >/dev/null || fail 'selected SSID hex was not stored'
grep -Fx '    psk=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' "$config" >/dev/null || fail 'derived PSK was not stored'
if grep -F 'correct horse' "$config" >/dev/null; then fail 'plaintext passphrase reached durable configuration'; fi
if grep -F 'old-secret-material' "$config" >/dev/null; then fail 'previous managed block was retained'; fi
cp "$config" "$TMP/before-failure"
NORTHSTAR_TEST_ASSOCIATED=no
export NORTHSTAR_TEST_ASSOCIATED
if printf '%s\n' 'different pass' | run --connect "$TMP/request" >/dev/null 2>&1; then fail 'failed association was accepted'; fi
cmp "$TMP/before-failure" "$config" >/dev/null || fail 'failed association did not restore prior configuration'
cat > "$TMP/bad-request" <<'EOF'
protocol=1
ssid_hex=54657374
security=secured
password=forbidden
EOF
if printf '%s\n' 'correct horse' | run --connect "$TMP/bad-request" >/dev/null 2>&1; then fail 'secret-bearing request was accepted'; fi
printf '%s\n' 'PASS: Wi-Fi helper scans safely, stores only a derived PSK, preserves config, and rolls back'
