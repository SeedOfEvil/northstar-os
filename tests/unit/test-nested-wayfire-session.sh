#!/bin/sh

# Exercise the user-level nested Wayfire session installer without touching
# the real home directory. This is intentionally host-neutral and only needs
# POSIX shell utilities plus an unprivileged test account.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-nested-session.XXXXXX")
TEST_HOME=$TMP_DIR/home
WAYFIRE_BIN=$TEST_HOME/.local/wayfire-nested/bin/wayfire
SESSION_BIN=$TEST_HOME/.local/bin/northstar-session
SHELL_BIN=$TEST_HOME/.local/bin/northstar-shell
SUPERVISOR_EVENTS=$TMP_DIR/supervisor-events
OUTPUT=$TMP_DIR/output.txt
ERROR_OUTPUT=$TMP_DIR/error.txt

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
    printf '%s\n' 'SKIP: nested session installer test requires an unprivileged account'
    exit 0
fi

mkdir -p "$(dirname "$WAYFIRE_BIN")"
printf '%s\n' '#!/bin/sh' 'exit 0' > "$WAYFIRE_BIN"
chmod 700 "$WAYFIRE_BIN"
mkdir -p "$(dirname "$SESSION_BIN")"
cat > "$SESSION_BIN" <<'STUB'
#!/bin/sh
set -eu
printf 'compositor=%s\n' "$NORTHSTAR_SESSION_COMPOSITOR" > "$NORTHSTAR_TEST_SUPERVISOR_EVENTS"
printf 'shell=%s\n' "$NORTHSTAR_SESSION_SHELL" >> "$NORTHSTAR_TEST_SUPERVISOR_EVENTS"
printf 'backend=%s renderer=%s force=%s\n' "$WLR_BACKENDS" "$WLR_RENDERER" "$WLR_RENDERER_FORCE_SOFTWARE" >> "$NORTHSTAR_TEST_SUPERVISOR_EVENTS"
STUB
printf '%s\n' '#!/bin/sh' 'exit 0' > "$SHELL_BIN"
chmod 700 "$SESSION_BIN" "$SHELL_BIN"
: > "$SUPERVISOR_EVENTS"
export HOME=$TEST_HOME
export NORTHSTAR_WAYFIRE_BIN=$WAYFIRE_BIN
export NORTHSTAR_SESSION_BIN=$SESSION_BIN
export NORTHSTAR_SESSION_SHELL=$SHELL_BIN
export NORTHSTAR_TEST_SUPERVISOR_EVENTS=$SUPERVISOR_EVENTS

sh "$ROOT/tools/install-nested-wayfire-session.sh" > "$OUTPUT" 2> "$ERROR_OUTPUT"
[ -f "$HOME/.xinitrc" ] || fail 'installer did not create .xinitrc'
[ -f "$HOME/.config/wayfire.ini" ] || fail 'installer did not create wayfire.ini'
cmp "$ROOT/config/xinitrc.nested-wayfire" "$HOME/.xinitrc" || fail '.xinitrc differs from the project template'
cmp "$ROOT/config/wayfire-nested.ini" "$HOME/.config/wayfire.ini" || fail 'wayfire.ini differs from the project template'
grep -F 'mode = 1280x800' "$HOME/.config/wayfire.ini" >/dev/null || fail 'wayfire.ini does not request the full 1280x800 X11 output'
grep -F '  ipc ' "$HOME/.config/wayfire.ini" >/dev/null || fail 'wayfire.ini does not enable Wayfire IPC'
grep -F '  ipc-rules ' "$HOME/.config/wayfire.ini" >/dev/null || fail 'wayfire.ini does not enable Wayfire IPC window rules'
grep -F '  xdg-activation ' "$HOME/.config/wayfire.ini" >/dev/null || fail 'wayfire.ini does not enable XDG activation'
grep -F 'binding_search = <ctrl> KEY_K' "$HOME/.config/wayfire.ini" >/dev/null \
    || fail 'wayfire.ini does not bind unified search at the compositor'
grep -F 'command_search = northstar-shell-command toggle-search' "$HOME/.config/wayfire.ini" >/dev/null \
    || fail 'wayfire.ini does not route the search binding to the shell control socket'
if grep -E '^[[:space:]]*terminal[[:space:]]*=[[:space:]]*qterminal[[:space:]]*$' "$HOME/.config/wayfire.ini" >/dev/null; then
    fail 'wayfire.ini still autostarts QTerminal'
fi
pass 'installer creates the nested session files'

sh "$ROOT/tools/install-nested-wayfire-session.sh" > "$OUTPUT" 2> "$ERROR_OUTPUT"
grep -F 'Already installed' "$OUTPUT" >/dev/null || fail 'installer was not idempotent'
pass 'installer is idempotent when files are unchanged'

printf '%s\n' 'custom xinitrc' > "$HOME/.xinitrc"
printf '%s\n' 'custom wayfire config' > "$HOME/.config/wayfire.ini"
if sh "$ROOT/tools/install-nested-wayfire-session.sh" > "$OUTPUT" 2> "$ERROR_OUTPUT"; then
    fail 'installer replaced custom files without --force'
fi
grep -F 'refusing to replace existing file' "$ERROR_OUTPUT" >/dev/null || fail 'installer did not explain the conflict'
grep -F 'custom xinitrc' "$HOME/.xinitrc" >/dev/null || fail 'installer changed the custom xinitrc'
pass 'installer preserves conflicting custom files'

sh "$ROOT/tools/install-nested-wayfire-session.sh" --force > "$OUTPUT" 2> "$ERROR_OUTPUT"
cmp "$ROOT/config/xinitrc.nested-wayfire" "$HOME/.xinitrc" || fail '--force did not install .xinitrc'
cmp "$ROOT/config/wayfire-nested.ini" "$HOME/.config/wayfire.ini" || fail '--force did not install wayfire.ini'
grep -F 'mode = 1280x800' "$HOME/.config/wayfire.ini" >/dev/null || fail '--force removed the full 1280x800 X11 output'
xinitrc_backup_found=0
for backup_file in "$HOME"/.xinitrc.northstar-backup-*; do
    if [ -f "$backup_file" ]; then
        xinitrc_backup_found=1
        break
    fi
done
[ "$xinitrc_backup_found" -eq 1 ] || fail '--force did not create an xinitrc backup'
wayfire_config_backup_found=0
for backup_file in "$HOME/.config"/wayfire.ini.northstar-backup-*; do
    if [ -f "$backup_file" ]; then
        wayfire_config_backup_found=1
        break
    fi
done
[ "$wayfire_config_backup_found" -eq 1 ] || fail '--force did not create a Wayfire config backup'
pass 'installer backs up custom files before --force replacement'

sh "$ROOT/tools/install-nested-wayfire-session.sh" --supervised --force > "$OUTPUT" 2> "$ERROR_OUTPUT"
if ! cmp "$ROOT/config/xinitrc.nested-wayfire-supervised" "$HOME/.xinitrc"; then
    fail '--supervised did not install the supervised .xinitrc'
fi
if ! grep -F 'northstar-session' "$OUTPUT" >/dev/null; then
    fail '--supervised did not report the session supervisor'
fi
pass 'installer supports an explicit supervised nested session'

sh "$ROOT/config/xinitrc.nested-wayfire-supervised"
if ! grep -F "compositor=$WAYFIRE_BIN" "$SUPERVISOR_EVENTS" >/dev/null; then
    fail 'supervised .xinitrc did not pass the Wayfire binary'
fi
if ! grep -F "shell=$SHELL_BIN" "$SUPERVISOR_EVENTS" >/dev/null; then
    fail 'supervised .xinitrc did not pass the shell binary'
fi
if ! grep -F 'backend=x11 renderer=pixman force=1' "$SUPERVISOR_EVENTS" >/dev/null; then
    fail 'supervised .xinitrc did not set software X11 renderer variables'
fi
pass 'supervised .xinitrc passes the compositor and shell through the supervisor'

printf '%s\n' 'All nested Wayfire session tests passed.'
