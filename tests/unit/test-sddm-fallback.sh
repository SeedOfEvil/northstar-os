#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
LAUNCHER=$ROOT/src/session/northstar-session-x11
DESKTOP=$ROOT/config/session/northstar-proxmox.desktop
SDDM_CONFIG=$ROOT/config/sddm/northstar-proxmox.conf
INSTALLER=$ROOT/tools/install-sddm-fallback.sh
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-sddm-fallback.XXXXXX")

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT HUP INT TERM

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

pass() {
    printf 'PASS: %s\n' "$1"
}

[ -f "$LAUNCHER" ] || fail 'fallback launcher is missing'
[ -f "$DESKTOP" ] || fail 'fallback desktop descriptor is missing'
[ -f "$SDDM_CONFIG" ] || fail 'fallback SDDM configuration is missing'
[ -f "$INSTALLER" ] || fail 'fallback installer is missing'

sh -n "$LAUNCHER"
sh -n "$INSTALLER"
grep -q '^Exec=northstar-session-x11$' "$DESKTOP"
grep -q '^TryExec=northstar-session-x11$' "$DESKTOP"
grep -q '^DisplayServer=x11$' "$SDDM_CONFIG"
grep -q '^Current=northstar$' "$SDDM_CONFIG"
grep -q 'WLR_BACKENDS=x11' "$LAUNCHER"
grep -q 'WLR_RENDERER=pixman' "$LAUNCHER"
grep -q 'sddm_enable=YES' "$INSTALLER"
grep -q 'sddm-greeter-qt6' "$INSTALLER"

mkdir -p "$TMP_DIR/bin" "$TMP_DIR/runtime"
fake_session=$TMP_DIR/bin/northstar-session
fake_wayfire=$TMP_DIR/bin/wayfire
fake_shell=$TMP_DIR/bin/northstar-shell
output=$TMP_DIR/environment.txt

printf '%s\n' '#!/bin/sh' \
    'printf "%s|%s|%s|%s|%s\n" "$DISPLAY" "$WLR_BACKENDS" "$WLR_RENDERER" "$NORTHSTAR_SESSION_COMPOSITOR" "$NORTHSTAR_SESSION_SHELL" > "$NORTHSTAR_TEST_OUTPUT"' \
    > "$fake_session"
printf '%s\n' '#!/bin/sh' 'exit 0' > "$fake_wayfire"
printf '%s\n' '#!/bin/sh' 'exit 0' > "$fake_shell"
chmod 700 "$fake_session" "$fake_wayfire" "$fake_shell"

DISPLAY=:0 \
XDG_RUNTIME_DIR="$TMP_DIR/runtime" \
NORTHSTAR_WAYFIRE_BIN="$fake_wayfire" \
NORTHSTAR_SESSION_BIN="$fake_session" \
NORTHSTAR_SESSION_SHELL="$fake_shell" \
NORTHSTAR_TEST_OUTPUT="$output" \
    sh "$LAUNCHER"

expected=":0|x11|pixman|$fake_wayfire|$fake_shell"
actual=$(cat "$output")
[ "$actual" = "$expected" ] || fail "fallback environment was not passed through: $actual"

pass 'fallback descriptor, installer contract, and X11 software environment are valid'
