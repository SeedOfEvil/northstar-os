#!/bin/sh

# Repeatable checks for the unprivileged M1 shell session. The --restart mode
# terminates only the exact shell PID recorded in the pid file and verifies
# that an existing qterminal client remains alive.

set -eu

PROG=${0##*/}
PID_FILE=${NORTHSTAR_SHELL_PIDFILE:-/tmp/northstar-shell-m1-live.pid}
LOG_FILE=${NORTHSTAR_SHELL_LOG:-/tmp/northstar-shell-m1-live.log}
SHELL_BIN=${NORTHSTAR_SHELL_BIN:-}
RESTART=0

usage() {
    cat <<USAGE
Usage: $PROG [--restart]

Check the current unprivileged Northstar shell session. With --restart,
restart only the shell PID in NORTHSTAR_SHELL_PIDFILE and verify that existing
qterminal clients survive.

Environment:
  NORTHSTAR_SHELL_BIN       exact shell binary for --restart (required there)
  NORTHSTAR_SHELL_PIDFILE   shell PID file (default: $PID_FILE)
  NORTHSTAR_SHELL_LOG       shell log for --restart (default: $LOG_FILE)
USAGE
}

die() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --restart)
            RESTART=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1"
            ;;
    esac
done

[ "$(id -u)" -ne 0 ] || die 'the shell smoke check must run unprivileged'
[ -r "$PID_FILE" ] || die "shell PID file is not readable: $PID_FILE"
[ -n "${WAYLAND_DISPLAY:-}" ] || die 'WAYLAND_DISPLAY is not set'
[ -n "${XDG_RUNTIME_DIR:-}" ] || die 'XDG_RUNTIME_DIR is not set'
[ -e "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ] || die 'the Wayland socket is not present'

pid=$(sed -n '1p' "$PID_FILE")
case "$pid" in
    ''|*[!0-9]*) die "invalid shell PID: $pid" ;;
esac

kill -0 "$pid" 2>/dev/null || die "shell process is not alive: $pid"
shell_user=$(ps -o user= -p "$pid" 2>/dev/null | awk '{$1=$1; print}')
[ "$shell_user" = "$(id -un)" ] || die "shell user is $shell_user, expected $(id -un)"

shell_command=$(ps -o command= -p "$pid" 2>/dev/null | awk '{$1=$1; print}')
case "$shell_command" in
    *northstar-shell*) ;;
    *) die "PID $pid is not northstar-shell: $shell_command" ;;
esac

procstat -f "$pid" 2>/dev/null | grep -F "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" >/dev/null \
    || die "shell PID $pid does not hold the expected Wayland socket"

qterminal_count() {
    pgrep -x qterminal 2>/dev/null | wc -l | tr -d ' '
}

printf 'PASS: live shell is PID %s as %s on %s\n' "$pid" "$shell_user" "$WAYLAND_DISPLAY"

[ "$RESTART" -eq 1 ] || exit 0
[ -n "$SHELL_BIN" ] || die 'NORTHSTAR_SHELL_BIN is required with --restart'
[ -x "$SHELL_BIN" ] || die "shell binary is not executable: $SHELL_BIN"

qterminal_before=$(qterminal_count)
case "$shell_command" in
    *"$SHELL_BIN"*) ;;
    *) die "PID $pid does not match requested shell binary: $SHELL_BIN" ;;
esac

kill -TERM "$pid"
waited=0
while kill -0 "$pid" 2>/dev/null && [ "$waited" -lt 5 ]; do
    sleep 1
    waited=$((waited + 1))
done
kill -0 "$pid" 2>/dev/null && die "shell PID $pid did not stop"

: > "$LOG_FILE"
nohup env \
    XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
    WAYLAND_DISPLAY="$WAYLAND_DISPLAY" \
    ${DISPLAY:+DISPLAY="$DISPLAY"} \
    ${XAUTHORITY:+XAUTHORITY="$XAUTHORITY"} \
    QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-wayland}" \
    MOZ_ENABLE_WAYLAND="${MOZ_ENABLE_WAYLAND:-1}" \
    "$SHELL_BIN" >"$LOG_FILE" 2>&1 < /dev/null &
new_pid=$!
printf '%s\n' "$new_pid" > "$PID_FILE"
sleep 2
kill -0 "$new_pid" 2>/dev/null || {
    sed -n '1,120p' "$LOG_FILE" >&2 || true
    die 'restarted shell exited'
}

qterminal_after=$(qterminal_count)
[ "$qterminal_before" = "$qterminal_after" ] \
    || die "qterminal count changed from $qterminal_before to $qterminal_after"
procstat -f "$new_pid" 2>/dev/null | grep -F "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" >/dev/null \
    || die "restarted shell does not hold the expected Wayland socket"
if grep -Eiq 'qrc:.*(error|no such file)|unable to configure' "$LOG_FILE"; then
    sed -n '1,120p' "$LOG_FILE" >&2
    die 'restarted shell reported a QML or layer-shell startup error'
fi

printf 'PASS: shell-only restart created PID %s and preserved %s qterminal client(s)\n' \
    "$new_pid" "$qterminal_after"
