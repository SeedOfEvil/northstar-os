#!/bin/sh

set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
HELPER=$ROOT/apps/installer/northstar-installer-recovery
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-installer-recovery.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM
LOG=$TMP_DIR/executor.log
fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }

cat > "$TMP_DIR/executor" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$NORTHSTAR_TEST_LOG"
printf '%s\n' 'DISK_MUTATION=none'
EOF
cat > "$TMP_DIR/engine" <<'EOF'
#!/bin/sh
printf 'engine %s\n' "$*" >> "$NORTHSTAR_TEST_LOG"
printf '%s\n' 'INSTALLER_STATUS=idle' 'RECOVERY_ACTION=none' 'DISK_MUTATION=none'
EOF
chmod +x "$TMP_DIR/executor" "$TMP_DIR/engine"

run_helper() {
    env NORTHSTAR_INSTALLER_RECOVERY_TEST_MODE=1 \
      NORTHSTAR_INSTALLER_RECOVERY_EXECUTOR="$TMP_DIR/executor" \
      NORTHSTAR_INSTALLER_RECOVERY_ENGINE="$TMP_DIR/engine" \
      NORTHSTAR_TEST_LOG="$LOG" sh "$HELPER" "$@"
}

transaction=nstar-install-0123456789abcdef-4202
run_helper --capabilities | grep -Fx 'disk_mutation=none' >/dev/null \
    || fail 'recovery capabilities omit the no-mutation boundary'
run_helper --status >/dev/null
grep -Fx -- 'engine --status' "$LOG" >/dev/null \
    || fail 'status request was not dispatched to the fixed engine'
run_helper --diagnostics "$transaction" >/dev/null
grep -Fx -- "--diagnostics $transaction" "$LOG" >/dev/null \
    || fail 'diagnostic request was not dispatched exactly'
run_helper --prepare-retry "$transaction" --confirm-device md42 >/dev/null
grep -Fx -- "--prepare-retry $transaction --confirm-device md42" "$LOG" >/dev/null \
    || fail 'retry request was not dispatched exactly'

before=$(wc -l < "$LOG" | tr -d ' ')
for arguments in \
    "--diagnostics invalid" \
    "--prepare-retry $transaction --confirm-device ../md42" \
    "--prepare-retry $transaction --wrong md42"; do
    if run_helper $arguments >/dev/null 2>&1; then
        fail "unsafe recovery arguments were accepted: $arguments"
    fi
done
after=$(wc -l < "$LOG" | tr -d ' ')
[ "$before" -eq "$after" ] || fail 'unsafe recovery request reached the executor'

if grep -Eq 'gpart|newfs|zpool|zfs|mount|umount|rm[[:space:]]+-rf' "$HELPER"; then
    fail 'recovery wrapper contains a disk mutation command'
fi
printf '%s\n' 'PASS: installer recovery helper accepts only fixed non-mutating diagnostics and retry preparation'
