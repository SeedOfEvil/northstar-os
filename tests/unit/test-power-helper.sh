#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
HELPER=$ROOT/src/power/northstar-power
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-power-helper.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

cat > "$TMP_DIR/id" <<'STUB'
#!/bin/sh
printf '%s\n' "$NORTHSTAR_TEST_UID"
STUB
cat > "$TMP_DIR/pkexec" <<'STUB'
#!/bin/sh
printf 'pkexec:%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
STUB
cat > "$TMP_DIR/shutdown" <<'STUB'
#!/bin/sh
printf 'shutdown:%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
STUB
chmod +x "$TMP_DIR/id" "$TMP_DIR/pkexec" "$TMP_DIR/shutdown"

run_helper() {
    env NORTHSTAR_POWER_ID_PATH="$TMP_DIR/id" \
        NORTHSTAR_POWER_PKEXEC_PATH="$TMP_DIR/pkexec" \
        NORTHSTAR_POWER_SELF_PATH=/usr/local/bin/northstar-power \
        NORTHSTAR_POWER_SHUTDOWN_PATH="$TMP_DIR/shutdown" \
        NORTHSTAR_TEST_UID="${NORTHSTAR_TEST_UID:-1002}" \
        NORTHSTAR_TEST_EVENTS="$TMP_DIR/events" \
        sh "$HELPER" "$@"
}

: > "$TMP_DIR/events"
run_helper restart
grep -Fx 'pkexec:/usr/local/bin/northstar-power restart' "$TMP_DIR/events" >/dev/null \
    || fail 'unprivileged restart did not re-enter the exact installed PolicyKit boundary'
[ "$(wc -l < "$TMP_DIR/events" | tr -d ' ')" -eq 1 ] \
    || fail 'unprivileged restart reached shutdown before authorization'

for arguments in '' 'reboot' 'restart now' 'shutdown now'; do
    if run_helper $arguments >/dev/null 2>&1; then
        fail "malformed power action was accepted: $arguments"
    fi
done
[ "$(wc -l < "$TMP_DIR/events" | tr -d ' ')" -eq 1 ] \
    || fail 'malformed power action reached PolicyKit or shutdown'

: > "$TMP_DIR/events"
NORTHSTAR_TEST_UID=0 run_helper restart
grep -Fx 'shutdown:-r now' "$TMP_DIR/events" >/dev/null \
    || fail 'authorized restart did not invoke the fixed shutdown arguments'

: > "$TMP_DIR/events"
NORTHSTAR_TEST_UID=0 run_helper shutdown
grep -Fx 'shutdown:-p now' "$TMP_DIR/events" >/dev/null \
    || fail 'authorized shutdown did not invoke the fixed shutdown arguments'

printf '%s\n' 'PASS: power helper validates one fixed action before PolicyKit elevation and shutdown'
