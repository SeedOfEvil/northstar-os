#!/bin/sh

# Deterministic M2 session-supervisor tests. Fake compositor and shell
# commands make the restart and ownership behavior testable without starting
# the live desktop.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-session-test.XXXXXX")
STUB_DIR=$TMP_DIR/stubs
LOG_DIR=$TMP_DIR/logs
RUNTIME_DIR=$TMP_DIR/runtime
SHELL_COUNT=$TMP_DIR/shell-count
COMPOSITOR_EVENTS=$TMP_DIR/compositor-events
DBUS_EVENTS=$TMP_DIR/dbus-events
CONSOLEKIT_EVENTS=$TMP_DIR/consolekit-events
AUTH_AGENT_EVENTS=$TMP_DIR/auth-agent-events
OUTPUT=$TMP_DIR/output.txt
ERROR_OUTPUT=$TMP_DIR/error.txt
SENTINEL_PID=
SESSION_PID=

mkdir -p "$STUB_DIR" "$LOG_DIR" "$RUNTIME_DIR"
: > "$SHELL_COUNT"
: > "$COMPOSITOR_EVENTS"
: > "$AUTH_AGENT_EVENTS"
: > "$CONSOLEKIT_EVENTS"
chmod 0700 "$RUNTIME_DIR"

cleanup() {
    if [ -n "$SENTINEL_PID" ] && kill -0 "$SENTINEL_PID" 2>/dev/null; then
        kill -TERM "$SENTINEL_PID" 2>/dev/null || true
        wait "$SENTINEL_PID" 2>/dev/null || true
    fi
    if [ -n "$SESSION_PID" ] && kill -0 "$SESSION_PID" 2>/dev/null; then
        kill -TERM "$SESSION_PID" 2>/dev/null || true
        wait "$SESSION_PID" 2>/dev/null || true
    fi
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

run_expect() {
    expected_status=$1
    shift
    if "$@" > "$OUTPUT" 2> "$ERROR_OUTPUT"; then
        actual_status=0
    else
        actual_status=$?
    fi
    if [ "$actual_status" -ne "$expected_status" ]; then
        cat "$OUTPUT" >&2 || true
        cat "$ERROR_OUTPUT" >&2 || true
        fail "expected exit $expected_status, got $actual_status: $*"
    fi
}

cat > "$STUB_DIR/fake-compositor" <<'STUB'
#!/bin/sh
set -eu
socket_path=$NORTHSTAR_TEST_RUNTIME_DIR/${WAYLAND_DISPLAY:-wayland-0}
mkdir -p "$NORTHSTAR_TEST_RUNTIME_DIR"
: > "$socket_path"
printf 'start\n' >> "$NORTHSTAR_TEST_COMPOSITOR_EVENTS"
stop() {
    rm -f "$socket_path"
    printf 'stop\n' >> "$NORTHSTAR_TEST_COMPOSITOR_EVENTS"
    exit 0
}
trap stop INT TERM HUP
while :; do
    sleep 1
done
STUB

cat > "$STUB_DIR/fake-shell" <<'STUB'
#!/bin/sh
set -eu
count=$(cat "$NORTHSTAR_TEST_SHELL_COUNT")
count=$((count + 1))
printf '%s\n' "$count" > "$NORTHSTAR_TEST_SHELL_COUNT"
[ -e "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ] || exit 43
if [ "${NORTHSTAR_TEST_SHELL_ALWAYS_FAIL:-0}" -eq 1 ]; then
    exit 7
fi
if [ "$count" -eq 1 ]; then
    exit 42
fi
exit 0
STUB

chmod 0755 "$STUB_DIR/fake-compositor" "$STUB_DIR/fake-shell"

cat > "$STUB_DIR/fake-consolekit" <<'STUB'
#!/bin/sh
set -eu
printf 'invoked\n' >> "$NORTHSTAR_TEST_CONSOLEKIT_EVENTS"
exec "$@"
STUB
cat > "$STUB_DIR/fake-dbus-run-session" <<'STUB'
#!/bin/sh
set -eu
printf 'invoked\n' >> "$NORTHSTAR_TEST_DBUS_EVENTS"
[ "${1:-}" = -- ] || exit 2
shift
exec "$@"
STUB

cat > "$STUB_DIR/hold-shell" <<'STUB'
#!/bin/sh
stop() {
    exit 0
}
trap stop INT TERM HUP
while :; do
    sleep 1
done
STUB

cat > "$STUB_DIR/fake-auth-agent" <<'STUB'
#!/bin/sh
printf 'start\n' >> "$NORTHSTAR_TEST_AUTH_AGENT_EVENTS"
stop() {
    printf 'stop\n' >> "$NORTHSTAR_TEST_AUTH_AGENT_EVENTS"
    exit 0
}
trap stop INT TERM HUP
while :; do
    sleep 1
done
STUB

chmod 0755 "$STUB_DIR/fake-consolekit" "$STUB_DIR/fake-dbus-run-session" "$STUB_DIR/hold-shell" "$STUB_DIR/fake-auth-agent"

sleep 30 &
SENTINEL_PID=$!

export XDG_RUNTIME_DIR=$RUNTIME_DIR
export NORTHSTAR_SESSION_SKIP_CONSOLEKIT=1
export NORTHSTAR_SESSION_SKIP_DBUS=1
export NORTHSTAR_SESSION_COMPOSITOR=$STUB_DIR/fake-compositor
export NORTHSTAR_SESSION_SHELL=$STUB_DIR/fake-shell
export NORTHSTAR_SESSION_AUTH_AGENT=$STUB_DIR/fake-auth-agent
export NORTHSTAR_SESSION_MAX_SHELL_RESTARTS=2
export NORTHSTAR_SESSION_RESTART_DELAY=0
export NORTHSTAR_SESSION_READY_DELAY=0
export NORTHSTAR_SESSION_EARLY_EXIT_WINDOW=0
export NORTHSTAR_SESSION_LOG_DIR=$LOG_DIR
export NORTHSTAR_SESSION_LOCK_DIR=$TMP_DIR/lock
export NORTHSTAR_SESSION_WAYLAND_DISPLAY=test-wayland
export NORTHSTAR_TEST_COMPOSITOR_EVENTS=$COMPOSITOR_EVENTS
export NORTHSTAR_TEST_DBUS_EVENTS=$DBUS_EVENTS
export NORTHSTAR_TEST_CONSOLEKIT_EVENTS=$CONSOLEKIT_EVENTS
export NORTHSTAR_TEST_AUTH_AGENT_EVENTS=$AUTH_AGENT_EVENTS
export NORTHSTAR_TEST_SHELL_COUNT=$SHELL_COUNT
export NORTHSTAR_TEST_RUNTIME_DIR=$RUNTIME_DIR

run_expect 0 sh "$ROOT/src/session/northstar-session"
[ "$(cat "$SHELL_COUNT")" = 2 ] || fail 'shell was not restarted exactly once'
grep -F 'start' "$COMPOSITOR_EVENTS" >/dev/null || fail 'compositor did not start'
grep -F 'stop' "$COMPOSITOR_EVENTS" >/dev/null || fail 'compositor was not stopped'
grep -F 'start' "$AUTH_AGENT_EVENTS" >/dev/null || fail 'PolicyKit agent did not start'
grep -F 'stop' "$AUTH_AGENT_EVENTS" >/dev/null || fail 'PolicyKit agent was not stopped'
grep -F 'restart 1/2' "$LOG_DIR/session.log" >/dev/null || fail 'restart was not recorded'
grep -F 'state=stopped' "$LOG_DIR/session.status" >/dev/null || fail 'session status did not record stopped state'
grep -F 'wayland_display=test-wayland' "$LOG_DIR/session.status" >/dev/null || fail 'session status did not record Wayland display'
grep -F 'restart_count=1' "$LOG_DIR/session.status" >/dev/null || fail 'session status did not record restart count'
kill -0 "$SENTINEL_PID" 2>/dev/null || fail 'unrelated sentinel process was terminated'
[ ! -d "$TMP_DIR/lock" ] || fail 'session lock was not released'
pass 'session restarts a crashed shell and stops only its compositor'

: > "$SHELL_COUNT"
: > "$COMPOSITOR_EVENTS"
: > "$DBUS_EVENTS"
export NORTHSTAR_SESSION_SKIP_DBUS=0
export NORTHSTAR_SESSION_DBUS_RUNNER=$STUB_DIR/fake-dbus-run-session
export NORTHSTAR_SESSION_LOCK_DIR=$TMP_DIR/dbus-lock
run_expect 0 sh "$ROOT/src/session/northstar-session"
grep -F 'invoked' "$DBUS_EVENTS" >/dev/null || fail 'D-Bus runner was not invoked'
[ ! -d "$TMP_DIR/dbus-lock" ] || fail 'D-Bus session lock was not released'
export NORTHSTAR_SESSION_SKIP_DBUS=1
pass 'session enters exactly one D-Bus wrapper when requested'
: > "$SHELL_COUNT"
: > "$COMPOSITOR_EVENTS"
: > "$CONSOLEKIT_EVENTS"
export NORTHSTAR_SESSION_SKIP_CONSOLEKIT=0
export NORTHSTAR_SESSION_CONSOLEKIT_RUNNER=$STUB_DIR/fake-consolekit
export NORTHSTAR_SESSION_LOCK_DIR=$TMP_DIR/consolekit-lock
run_expect 0 sh "$ROOT/src/session/northstar-session"
grep -F 'invoked' "$CONSOLEKIT_EVENTS" >/dev/null || fail 'ConsoleKit runner was not invoked'
[ ! -d "$TMP_DIR/consolekit-lock" ] || fail 'ConsoleKit session lock was not released'
export NORTHSTAR_SESSION_SKIP_CONSOLEKIT=1
pass 'session enters exactly one ConsoleKit wrapper when requested'

: > "$SHELL_COUNT"
: > "$COMPOSITOR_EVENTS"
SDDM_TMP=$TMP_DIR/sddm-tmp
mkdir -p "$SDDM_TMP"
XDG_RUNTIME_DIR=/var/run/xdg/test-user
TMPDIR=$SDDM_TMP
export XDG_RUNTIME_DIR TMPDIR
NORTHSTAR_TEST_RUNTIME_DIR=$SDDM_TMP/northstar-runtime-$(id -u)
export NORTHSTAR_TEST_RUNTIME_DIR
export NORTHSTAR_SESSION_LOCK_DIR=$TMP_DIR/sddm-runtime-lock
run_expect 0 sh "$ROOT/src/session/northstar-session"
[ -d "$NORTHSTAR_TEST_RUNTIME_DIR" ] \
    || fail 'SDDM runtime path was not replaced with a private directory'
if find "$NORTHSTAR_TEST_RUNTIME_DIR" -maxdepth 1 \
    -name '.northstar-write-probe.*' -print | grep -q .; then
    fail 'runtime writability probe was retained'
fi
XDG_RUNTIME_DIR=$RUNTIME_DIR
TMPDIR=$TMP_DIR
NORTHSTAR_TEST_RUNTIME_DIR=$RUNTIME_DIR
export XDG_RUNTIME_DIR TMPDIR NORTHSTAR_TEST_RUNTIME_DIR
pass 'session replaces the unusable FreeBSD SDDM runtime path'

: > "$SHELL_COUNT"
: > "$COMPOSITOR_EVENTS"
XDG_RUNTIME_DIR=/var/run/user/$(id -u)
TMPDIR=$SDDM_TMP
export XDG_RUNTIME_DIR TMPDIR
NORTHSTAR_TEST_RUNTIME_DIR=$SDDM_TMP/northstar-runtime-$(id -u)
export NORTHSTAR_TEST_RUNTIME_DIR
export NORTHSTAR_SESSION_LOCK_DIR=$TMP_DIR/consolekit-runtime-lock
run_expect 0 sh "$ROOT/src/session/northstar-session"
[ -d "$NORTHSTAR_TEST_RUNTIME_DIR" ] \
    || fail 'ConsoleKit runtime path was not replaced with a private directory'
XDG_RUNTIME_DIR=$RUNTIME_DIR
TMPDIR=$TMP_DIR
NORTHSTAR_TEST_RUNTIME_DIR=$RUNTIME_DIR
export XDG_RUNTIME_DIR TMPDIR NORTHSTAR_TEST_RUNTIME_DIR
pass 'session replaces the unusable FreeBSD ConsoleKit runtime path'

cat > "$STUB_DIR/early-clean-shell" <<'STUB'
#!/bin/sh
set -eu
count=$(cat "$NORTHSTAR_TEST_SHELL_COUNT")
count=$((count + 1))
printf '%s\n' "$count" > "$NORTHSTAR_TEST_SHELL_COUNT"
if [ "$count" -eq 1 ]; then
    exit 0
fi
stop() {
    exit 0
}
trap stop INT TERM HUP
while :; do
    sleep 1
done
STUB
chmod 0755 "$STUB_DIR/early-clean-shell"

: > "$SHELL_COUNT"
: > "$COMPOSITOR_EVENTS"
export NORTHSTAR_SESSION_SHELL=$STUB_DIR/early-clean-shell
export NORTHSTAR_SESSION_EARLY_EXIT_WINDOW=10
export NORTHSTAR_SESSION_MAX_SHELL_RESTARTS=2
export NORTHSTAR_SESSION_LOCK_DIR=$TMP_DIR/early-clean-lock
export NORTHSTAR_SESSION_LOG_DIR=$TMP_DIR/early-clean-logs
sh "$ROOT/src/session/northstar-session" > "$TMP_DIR/early-clean-output.txt" 2>&1 &
SESSION_PID=$!
waited=0
while ! grep -Fx '2' "$SHELL_COUNT" >/dev/null 2>&1 && [ "$waited" -lt 5 ]; do
    sleep 1
    waited=$((waited + 1))
done
grep -Fx '2' "$SHELL_COUNT" >/dev/null 2>&1 \
    || fail 'early clean shell exit was not restarted'
grep -F 'Shell exited cleanly after' "$TMP_DIR/early-clean-logs/session.log" >/dev/null \
    || fail 'early clean shell restart was not recorded'
kill -TERM "$SESSION_PID"
if wait "$SESSION_PID"; then
    :
else
    :
fi
SESSION_PID=
export NORTHSTAR_SESSION_EARLY_EXIT_WINDOW=0
pass 'session recovers from an early clean shell exit during output setup'

: > "$SHELL_COUNT"
: > "$COMPOSITOR_EVENTS"
export NORTHSTAR_TEST_SHELL_ALWAYS_FAIL=1
export NORTHSTAR_SESSION_SHELL=$STUB_DIR/fake-shell
export NORTHSTAR_SESSION_MAX_SHELL_RESTARTS=1
export NORTHSTAR_SESSION_LOG_DIR=$LOG_DIR
export NORTHSTAR_SESSION_LOCK_DIR=$TMP_DIR/limit-lock
run_expect 7 sh "$ROOT/src/session/northstar-session"
[ "$(cat "$SHELL_COUNT")" = 2 ] || fail 'restart limit did not bound shell attempts'
grep -F 'restart limit reached' "$LOG_DIR/session.log" >/dev/null || fail 'restart limit was not recorded'
grep -F 'state=failed' "$LOG_DIR/session.status" >/dev/null || fail 'failed session state was overwritten during cleanup'
grep -F 'last_event=shell-restart-limit-reached' "$LOG_DIR/session.status" >/dev/null || fail 'failed session event was not preserved'
pass 'session enforces the shell crash restart limit'

unset NORTHSTAR_TEST_SHELL_ALWAYS_FAIL
export NORTHSTAR_SESSION_SHELL=$STUB_DIR/hold-shell
export NORTHSTAR_SESSION_LOCK_DIR=$TMP_DIR/live-lock
export NORTHSTAR_SESSION_LOG_DIR=$TMP_DIR/live-logs
live_output=$TMP_DIR/live-output.txt
sh "$ROOT/src/session/northstar-session" > "$live_output" 2>&1 &
SESSION_PID=$!
waited=0
while [ ! -d "$TMP_DIR/live-lock" ] && [ "$waited" -lt 5 ]; do
    sleep 1
    waited=$((waited + 1))
done
[ -d "$TMP_DIR/live-lock" ] || fail 'long-running session did not acquire its lock'
waited=0
while { [ ! -f "$TMP_DIR/live-logs/session.status" ] || ! grep -F 'state=running' "$TMP_DIR/live-logs/session.status" >/dev/null; } && [ "$waited" -lt 5 ]; do
    sleep 1
    waited=$((waited + 1))
done
[ -f "$TMP_DIR/live-logs/session.status" ] || fail 'live session status file was not created'
grep -F 'state=running' "$TMP_DIR/live-logs/session.status" >/dev/null || fail 'live session status did not reach running state'
grep -F 'wayland_display=test-wayland' "$TMP_DIR/live-logs/session.status" >/dev/null || fail 'live session status has wrong Wayland display'
printf '%s\n' 'active-control-sentinel' > "$TMP_DIR/live-logs/session.control"
run_expect 1 sh "$ROOT/src/session/northstar-session"
grep -F 'already running' "$ERROR_OUTPUT" >/dev/null || fail 'duplicate session was not rejected'
grep -F 'state=running' "$TMP_DIR/live-logs/session.status" >/dev/null || fail 'duplicate session clobbered the active status'
grep -F 'active-control-sentinel' "$TMP_DIR/live-logs/session.control" >/dev/null || fail 'duplicate session removed the active control file'
kill -TERM "$SESSION_PID"
if wait "$SESSION_PID"; then
    :
else
    :
fi
SESSION_PID=
[ ! -d "$TMP_DIR/live-lock" ] || fail 'long-running session lock was not released'
grep -F 'state=stopped' "$TMP_DIR/live-logs/session.status" >/dev/null || fail 'live session status did not record stopped state'
pass 'session rejects a duplicate user session'

printf 'All session supervisor tests passed.\n'
