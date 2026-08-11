#!/bin/sh

# Root-isolated transaction orchestration smoke. Fake pkg/bectl boundaries
# prove ordering, target verification, rollback scheduling, and home-data
# preservation without mutating the host package database or boot environments.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TRANSACTION=$ROOT/src/update/northstar-update-transaction
TMP_DIR=$(mktemp -d /tmp/northstar-transaction.XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

[ "$(id -u)" -eq 0 ] || { echo 'FAIL: run transactional update smoke as root' >&2; exit 1; }
mkdir -p "$TMP_DIR/state" "$TMP_DIR/request"
printf '%s\n' 'home-data-survives' > "$TMP_DIR/home-sentinel"
printf '%s\n' '0.9.0' > "$TMP_DIR/version"

cat > "$TMP_DIR/broker" <<'BROKER'
#!/bin/sh
request=
while [ "$#" -gt 0 ]; do
    case "$1" in --request) request=$2; shift 2 ;; *) shift ;; esac
done
cat > "$request" <<'REQUEST'
protocol=1
operation=create-before
channel=development
repository_revision=74
source_revision=abcdef1234567890
catalogue_sha256=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
signature_fingerprint=abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789
boot_environment=northstar-before-development-r74-abcdef123456
plan_status=verified
authorization=interactive-confirmation
REQUEST
chmod 0600 "$request"
printf '%s\n' 'target_package=northstar|1.0.0'
BROKER

cat > "$TMP_DIR/helper" <<'HELPER'
#!/bin/sh
request=$2
operation=$(awk -F= '$1 == "operation" { print $2 }' "$request")
printf '%s\n' "$operation" >> "$NORTHSTAR_TEST_EVENTS"
HELPER

cat > "$TMP_DIR/pkg" <<'PKG'
#!/bin/sh
if [ "$1" = query ] && [ "$2" = -a ]; then
    printf 'northstar|%s\n' "$(cat "$NORTHSTAR_TEST_VERSION")"
elif [ "$1" = query ]; then
    cat "$NORTHSTAR_TEST_VERSION"
elif [ "$1" = upgrade ]; then
    printf '%s\n' upgrade >> "$NORTHSTAR_TEST_EVENTS"
    [ "${NORTHSTAR_TEST_FAIL:-0}" -eq 0 ] || exit 1
    printf '%s\n' '1.0.0' > "$NORTHSTAR_TEST_VERSION"
else
    exit 2
fi
PKG
chmod 0700 "$TMP_DIR/broker" "$TMP_DIR/helper" "$TMP_DIR/pkg"

run_transaction() {
    env NORTHSTAR_UPDATE_BROKER="$TMP_DIR/broker" \
        NORTHSTAR_UPDATE_HELPER="$TMP_DIR/helper" \
        NORTHSTAR_UPDATE_PKG="$TMP_DIR/pkg" \
        NORTHSTAR_UPDATE_POLICY="$TMP_DIR/policy" \
        NORTHSTAR_UPDATE_METADATA="$TMP_DIR/metadata" \
        NORTHSTAR_UPDATE_STATE_DIR="$TMP_DIR/state" \
        NORTHSTAR_UPDATE_REQUEST_DIR="$TMP_DIR/request" \
        NORTHSTAR_TEST_EVENTS="$TMP_DIR/events" \
        NORTHSTAR_TEST_VERSION="$TMP_DIR/version" \
        NORTHSTAR_TEST_FAIL="${NORTHSTAR_TEST_FAIL:-0}" \
        sh "$TRANSACTION" "$@"
}

: > "$TMP_DIR/events"
run_transaction --apply-update --confirm
[ "$(paste -sd, "$TMP_DIR/events")" = 'create-before,upgrade' ] || { echo 'FAIL: boot environment was not created before pkg' >&2; exit 1; }
grep -Fx 'status=updated' "$TMP_DIR/state/update-state.conf" >/dev/null
[ "$(cat "$TMP_DIR/home-sentinel")" = home-data-survives ]
run_transaction --rollback --confirm
grep -Fx rollback "$TMP_DIR/events" >/dev/null

: > "$TMP_DIR/events"
printf '%s\n' '0.9.0' > "$TMP_DIR/version"
NORTHSTAR_TEST_FAIL=1
export NORTHSTAR_TEST_FAIL
if run_transaction --apply-update --confirm; then
    echo 'FAIL: injected package failure unexpectedly succeeded' >&2
    exit 1
fi
[ "$(paste -sd, "$TMP_DIR/events")" = 'create-before,upgrade,rollback' ] || { echo 'FAIL: failed update did not schedule rollback' >&2; exit 1; }
grep -Fx 'status=rollback-scheduled' "$TMP_DIR/state/update-state.conf" >/dev/null
[ "$(cat "$TMP_DIR/home-sentinel")" = home-data-survives ]

printf '%s\n' 'PASS: transactional update ordering, verification, rollback, and home preservation'
