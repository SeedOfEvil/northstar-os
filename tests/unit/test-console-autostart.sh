#!/bin/sh

# Test the opt-in console-login hook and its reversible installer in an
# isolated unprivileged home directory.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-console-autostart.XXXXXX")
TEST_HOME=$TMP_DIR/home
STUB_DIR=$TMP_DIR/stubs
CALLED=$TMP_DIR/startx-called

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT HUP INT TERM

pass() {
    printf 'PASS: %s\n' "$1"
}

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

if [ "$(id -u)" -eq 0 ]; then
    printf '%s\n' 'SKIP: console autostart installer test requires an unprivileged account'
    exit 0
fi

mkdir -p "$TEST_HOME" "$STUB_DIR"
printf '%s\n' 'custom profile content' > "$TEST_HOME/.profile"

HOME=$TEST_HOME sh "$ROOT/tools/install-console-autostart.sh" --enable >/dev/null
[ -f "$TEST_HOME/.config/northstar/console-autostart.sh" ] || fail 'autostart source was not installed'
[ -f "$TEST_HOME/.config/northstar/console-autostart.enabled" ] || fail 'autostart marker was not created'
grep -F 'custom profile content' "$TEST_HOME/.profile" >/dev/null || fail 'existing profile content was not preserved'
grep -F 'northstar-console-autostart: begin' "$TEST_HOME/.profile" >/dev/null || fail 'profile hook was not installed'
pass 'installer enables autostart without replacing the profile'

HOME=$TEST_HOME sh "$ROOT/tools/install-console-autostart.sh" --enable >/dev/null
[ "$(grep -Fc 'northstar-console-autostart: begin' "$TEST_HOME/.profile")" -eq 1 ] || fail 'installer duplicated the profile hook'
pass 'installer is idempotent'

printf '%s\n' '#!/bin/sh' "printf '%s\\n' called > '$CALLED'" > "$STUB_DIR/startx"
printf '%s\n' '#!/bin/sh' "printf '%s\\n' /dev/ttyv0" > "$STUB_DIR/tty"
chmod 700 "$STUB_DIR/startx" "$STUB_DIR/tty"
HOME=$TEST_HOME PATH="$STUB_DIR:$PATH" NORTHSTAR_STARTX_BIN="$STUB_DIR/startx" \
    sh -c '. "$HOME/.config/northstar/console-autostart.sh"'
[ -f "$CALLED" ] || fail 'console autostart did not invoke startx on ttyv0'
pass 'console autostart starts the desktop on a local virtual console'

rm -f "$CALLED"
printf '%s\n' '#!/bin/sh' "printf '%s\\n' /dev/pts/0" > "$STUB_DIR/tty"
HOME=$TEST_HOME PATH="$STUB_DIR:$PATH" NORTHSTAR_STARTX_BIN="$STUB_DIR/startx" \
    sh -c '. "$HOME/.config/northstar/console-autostart.sh"'
[ ! -f "$CALLED" ] || fail 'console autostart ran for a non-console tty'
pass 'console autostart leaves non-console logins alone'

HOME=$TEST_HOME sh "$ROOT/tools/install-console-autostart.sh" --disable >/dev/null
[ ! -f "$TEST_HOME/.config/northstar/console-autostart.enabled" ] || fail 'disable did not remove the marker'
pass 'console autostart can be disabled without removing the profile hook'

sh -n "$ROOT/config/northstar-console-autostart.sh"
printf '%s\n' 'All console autostart tests passed.'
