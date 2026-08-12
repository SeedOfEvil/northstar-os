#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
HELPER=$ROOT/apps/recovery/northstar-boot-environment
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-be-recovery.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
assert_contains() { printf '%s\n' "$2" | grep -F "$1" >/dev/null 2>&1 || fail "missing: $1"; }

cat > "$TMP_DIR/fake-bectl" <<'EOF'
#!/bin/sh
set -eu
STATE=${NORTHSTAR_FAKE_BECTL_STATE:?}
case "$1" in
    list)
        if [ -f "$STATE" ]; then
            printf 'default\tN\t/\t4.43G\t2026-08-05 22:35\n'
            printf 'northstar-before-development-r78-017fc81040bb\tR\t-\t1.18G\t2026-08-10 19:48\n'
        else
            printf 'default\tNR\t/\t4.43G\t2026-08-05 22:35\n'
            printf 'northstar-before-development-r78-017fc81040bb\t-\t-\t1.18G\t2026-08-10 19:48\n'
        fi
        ;;
    activate)
        [ "$2" = northstar-before-development-r78-017fc81040bb ] || exit 9
        : > "$STATE"
        ;;
    *) exit 8 ;;
esac
EOF
chmod 700 "$TMP_DIR/fake-bectl"

run_helper() {
    NORTHSTAR_BOOT_ENVIRONMENT_TEST_MODE=1 \
    NORTHSTAR_BOOT_ENVIRONMENT_BECTL="$TMP_DIR/fake-bectl" \
    NORTHSTAR_FAKE_BECTL_STATE="$TMP_DIR/selected" \
        "$HELPER" "$@"
}

output=$(run_helper --capabilities)
assert_contains 'operations=status,activate' "$output"
assert_contains 'destructive-operations=none' "$output"

output=$(run_helper --status)
assert_contains 'BOOT_ENVIRONMENT_RECOVERY=1' "$output"
assert_contains 'COUNT=2' "$output"
assert_contains 'ENTRY_0=default|NR|/|4.43G|2026-08-05 22:35|yes|yes|no|no' "$output"
assert_contains 'ENTRY_1=northstar-before-development-r78-017fc81040bb|-|-|1.18G|2026-08-10 19:48|no|no|yes|yes' "$output"

if run_helper --activate default --confirm default >/dev/null 2>&1; then
    fail 'default environment escaped the Northstar namespace'
fi
if run_helper --activate northstar-before-development-r78-017fc81040bb \
    --confirm northstar-before-development-r77-badbeef >/dev/null 2>&1; then
    fail 'mismatched confirmation was accepted'
fi

output=$(run_helper --activate northstar-before-development-r78-017fc81040bb \
    --confirm northstar-before-development-r78-017fc81040bb)
assert_contains 'ACTIVATION=scheduled' "$output"
assert_contains 'ACTIVE_NEXT=yes' "$output"
[ -f "$TMP_DIR/selected" ] || fail 'fake bectl activation was not invoked'

output=$(run_helper --activate northstar-before-development-r78-017fc81040bb \
    --confirm northstar-before-development-r78-017fc81040bb)
assert_contains 'ACTIVATION=already-selected' "$output"

cat > "$TMP_DIR/fake-bectl" <<'EOF'
#!/bin/sh
printf 'unsafe|name\tNR\t/\t1G\t2026-08-10 19:48\n'
EOF
chmod 700 "$TMP_DIR/fake-bectl"
if run_helper --status >/dev/null 2>&1; then
    fail 'unsafe bectl inventory was accepted'
fi

printf '%s\n' 'PASS: bounded boot-environment inventory and activation contract'
