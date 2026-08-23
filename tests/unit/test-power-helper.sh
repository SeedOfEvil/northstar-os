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
cat > "$TMP_DIR/acpiconf" <<'STUB'
#!/bin/sh
printf 'acpiconf:%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
STUB
cat > "$TMP_DIR/sysctl" <<'STUB'
#!/bin/sh
printf 'sysctl:%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
STUB
chmod +x "$TMP_DIR/id" "$TMP_DIR/pkexec" "$TMP_DIR/shutdown" \
    "$TMP_DIR/acpiconf" "$TMP_DIR/sysctl"

run_helper() {
    env NORTHSTAR_POWER_ID_PATH="$TMP_DIR/id" \
        NORTHSTAR_POWER_PKEXEC_PATH="$TMP_DIR/pkexec" \
        NORTHSTAR_POWER_SELF_PATH=/usr/local/bin/northstar-power \
        NORTHSTAR_POWER_SHUTDOWN_PATH="$TMP_DIR/shutdown" \
        NORTHSTAR_POWER_ACPICONF_PATH="$TMP_DIR/acpiconf" \
        NORTHSTAR_POWER_SYSCTL_PATH="$TMP_DIR/sysctl" \
        NORTHSTAR_POWER_SYSCTL_CONF="$TMP_DIR/sysctl.conf" \
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

for arguments in '' 'reboot' 'restart now' 'shutdown now' 'suspend now' 'lid-suspend-maybe'; do
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

: > "$TMP_DIR/events"
NORTHSTAR_TEST_UID=0 run_helper suspend
sleep 1
grep -Fx 'acpiconf:-s 3' "$TMP_DIR/events" >/dev/null \
    || fail 'authorized sleep did not invoke the fixed S3 request'

: > "$TMP_DIR/events"
cat > "$TMP_DIR/sysctl.conf" <<'EOF'
# Preserve unrelated settings and comments.
kern.randompid="1"
hw.acpi.lid_switch_state="NONE"
hw.acpi.lid_switch_state = "S3"
EOF
NORTHSTAR_TEST_UID=0 run_helper lid-suspend-on
grep -Fx 'sysctl:hw.acpi.lid_switch_state=S3' "$TMP_DIR/events" >/dev/null \
    || fail 'lid sleep enable was not applied to the fixed sysctl key'
grep -Fx '# Preserve unrelated settings and comments.' "$TMP_DIR/sysctl.conf" >/dev/null \
    || fail 'lid sleep enable discarded an unrelated comment'
grep -Fx 'kern.randompid="1"' "$TMP_DIR/sysctl.conf" >/dev/null \
    || fail 'lid sleep enable discarded an unrelated sysctl'
[ "$(grep -Fc 'hw.acpi.lid_switch_state=' "$TMP_DIR/sysctl.conf")" -eq 1 ] \
    || fail 'lid sleep enable did not replace duplicate fixed-key assignments'
grep -Fx 'hw.acpi.lid_switch_state="S3"' "$TMP_DIR/sysctl.conf" >/dev/null \
    || fail 'lid sleep enable did not persist the fixed S3 value'

: > "$TMP_DIR/events"
NORTHSTAR_TEST_UID=0 run_helper lid-suspend-off
grep -Fx 'sysctl:hw.acpi.lid_switch_state=NONE' "$TMP_DIR/events" >/dev/null \
    || fail 'lid sleep disable was not applied to the fixed sysctl key'
grep -Fx 'hw.acpi.lid_switch_state="NONE"' "$TMP_DIR/sysctl.conf" >/dev/null \
    || fail 'lid sleep disable did not persist the fixed NONE value'

rm -f "$TMP_DIR/sysctl.conf"
ln -s "$TMP_DIR/missing-sysctl.conf" "$TMP_DIR/sysctl.conf"
: > "$TMP_DIR/events"
if NORTHSTAR_TEST_UID=0 run_helper lid-suspend-on >/dev/null 2>&1; then
    fail 'lid sleep accepted a symlinked sysctl configuration'
fi
[ ! -s "$TMP_DIR/events" ] \
    || fail 'lid sleep reached the live sysctl after refusing a symlinked configuration'

printf '%s\n' 'PASS: power helper validates fixed lifecycle and lid actions before PolicyKit elevation'
