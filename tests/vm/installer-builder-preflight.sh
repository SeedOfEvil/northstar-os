#!/bin/sh

# Keep fake-tool installer contracts unprivileged while reserving the real
# disposable GEOM/ZFS probe for the root-owned release builder boundary.
set -eu

fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
usage() {
    printf 'usage: %s --test-user USER\n' "${0##*/}" >&2
    exit 64
}

test_user=
while [ "$#" -gt 0 ]; do
    case "$1" in
        --test-user)
            [ "$#" -ge 2 ] || usage
            test_user=$2
            shift 2
            ;;
        *) usage ;;
    esac
done

[ "$(id -u)" -eq 0 ] || fail 'run this builder preflight as root'
[ "$(uname -s)" = FreeBSD ] || fail 'FreeBSD is required'
case "$test_user" in
    ''|*[!A-Za-z0-9_-]*) fail 'test user is missing or unsafe' ;;
esac
pw usershow "$test_user" >/dev/null 2>&1 || fail 'test user does not exist'
[ "$(id -u "$test_user")" -ne 0 ] || fail 'test user must be unprivileged'

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
case "$ROOT" in
    *[!A-Za-z0-9_./-]*) fail 'project path contains unsupported characters' ;;
esac

run_unprivileged_test() {
    relative=$1
    script=$ROOT/$relative
    [ -f "$script" ] || fail "missing test: $relative"
    [ "$(stat -f '%u' "$script")" -eq 0 ] \
        || fail "test is not root-owned: $relative"
    mode=$(stat -f '%Lp' "$script")
    [ $((mode % 100 / 10 & 2)) -eq 0 ] \
        || fail "test is group-writable: $relative"
    [ $((mode % 10 & 2)) -eq 0 ] \
        || fail "test is world-writable: $relative"
    su -m "$test_user" -c "exec /bin/sh $script"
}

run_unprivileged_test tests/unit/test-image-assembler.sh
run_unprivileged_test tests/unit/test-installer-executor.sh
sh "$ROOT/tests/vm/installer-zfs-reset-smoke.sh"

printf 'PASS: unprivileged installer contracts and privileged ZFS reset smoke succeeded\n'
