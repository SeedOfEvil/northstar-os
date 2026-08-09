#!/bin/sh

# Deterministic tests for the update-helper request boundary. These tests do
# not invoke bectl, pkg, sudo, or any system mutation.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
HELPER=$ROOT/src/update/northstar-update-helper
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-update-helper-test.XXXXXX")
VALID=$TMP_DIR/valid.request
OUTPUT=$TMP_DIR/output.txt
ERROR_OUTPUT=$TMP_DIR/error.txt

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

assert_contains() {
    expected=$1
    file=$2
    grep -F "$expected" "$file" >/dev/null 2>&1 || fail "expected '$expected' in $file"
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

[ -f "$HELPER" ] || fail 'update helper is missing'
sh -n "$HELPER"

run_expect 0 sh "$HELPER" --capabilities
assert_contains 'protocol=1' "$OUTPUT"
assert_contains 'operations=create-before,rollback' "$OUTPUT"
assert_contains 'package-transaction=not-part-of-helper' "$OUTPUT"
pass 'capabilities report the versioned narrow boundary'

cat > "$VALID" <<'REQUEST'
protocol=1
operation=create-before
channel=development
repository_revision=42
source_revision=abcdef1234567890abcdef1234567890abcdef12
catalogue_sha256=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
signature_fingerprint=abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789
boot_environment=northstar-before-development-r42-abcdef123456
plan_status=verified
authorization=interactive-confirmation
REQUEST

run_expect 0 sh "$HELPER" --dry-run "$VALID"
assert_contains 'VALID: verified create-before request' "$OUTPUT"
assert_contains 'Would execute: /sbin/bectl create northstar-before-development-r42-abcdef123456' "$OUTPUT"
assert_contains 'No bectl or pkg command was run.' "$OUTPUT"
pass 'verified create-before request is dry-run only'

sed 's/operation=create-before/operation=delete/' "$VALID" > "$TMP_DIR/bad-operation.request"
run_expect 65 sh "$HELPER" --dry-run "$TMP_DIR/bad-operation.request"
pass 'unsupported operations are rejected'

sed 's/boot_environment=.*/boot_environment=zroot/' "$VALID" > "$TMP_DIR/bad-name.request"
run_expect 65 sh "$HELPER" --dry-run "$TMP_DIR/bad-name.request"
pass 'boot-environment names remain bounded to the Northstar namespace'

sed 's/plan_status=verified/plan_status=unverified/' "$VALID" > "$TMP_DIR/bad-plan.request"
run_expect 65 sh "$HELPER" --dry-run "$TMP_DIR/bad-plan.request"
pass 'unverified plans are rejected'

cat "$VALID" > "$TMP_DIR/duplicate.request"
printf '%s\n' 'protocol=1' >> "$TMP_DIR/duplicate.request"
run_expect 65 sh "$HELPER" --dry-run "$TMP_DIR/duplicate.request"
pass 'duplicate request fields are rejected'

if [ "$(id -u)" -ne 0 ]; then
    run_expect 77 sh "$HELPER" --apply "$VALID"
    pass 'apply requires root before any helper operation'
fi

printf '%s\n' 'All update-helper tests passed.'
