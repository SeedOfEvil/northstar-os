#!/bin/sh

# Deterministic shell-level tests for the M0 scripts. The fixtures replace
# FreeBSD-specific commands in PATH, so these tests can run on a FreeBSD CI
# worker without changing the host.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-m0-test.XXXXXX")
STUB_DIR=$TMP_DIR/stubs
STATE_DIR=$TMP_DIR/state
LOG=$TMP_DIR/commands.log
INSTALLED=$STATE_DIR/installed.txt
SYSRC_STATE=$STATE_DIR/sysrc.txt
SERVICE_STATE=$STATE_DIR/services.txt
VIDEO_MEMBER=$STATE_DIR/video-member
OUTPUT=$TMP_DIR/output.txt
ERROR_OUTPUT=$TMP_DIR/error.txt

mkdir -p "$STUB_DIR" "$STATE_DIR"
: > "$LOG"
: > "$INSTALLED"
: > "$SYSRC_STATE"
: > "$SERVICE_STATE"

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

assert_not_contains() {
    unexpected=$1
    file=$2
    if grep -F "$unexpected" "$file" >/dev/null 2>&1; then
        fail "did not expect '$unexpected' in $file"
    fi
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

reset_state() {
    : > "$LOG"
    : > "$INSTALLED"
    : > "$SYSRC_STATE"
    : > "$SERVICE_STATE"
    rm -f "$VIDEO_MEMBER"
}

write_stubs() {
    cat > "$STUB_DIR/freebsd-version" <<'STUB'
#!/bin/sh
release=${NORTHSTAR_TEST_RELEASE:-15.1-RELEASE-p1}
case "${1:-}" in
    -u|-k) printf '%s\n' "$release" ;;
    -kru) printf '%s\n' "$release" ;;
    *) exit 2 ;;
esac
STUB

    cat > "$STUB_DIR/uname" <<'STUB'
#!/bin/sh
case "${1:-}" in
    -m) printf '%s\n' "${NORTHSTAR_TEST_ARCH:-amd64}" ;;
    -a) printf '%s\n' 'FreeBSD northstar-test 15.1-RELEASE-p1 amd64' ;;
    *) printf '%s\n' 'FreeBSD' ;;
esac
STUB

    cat > "$STUB_DIR/sysctl" <<'STUB'
#!/bin/sh
if [ "${1:-}" = -n ]; then
    printf '%s\n' "${NORTHSTAR_TEST_BOOT:-UEFI}"
else
    printf 'machdep.bootmethod: %s\n' "${NORTHSTAR_TEST_BOOT:-UEFI}"
fi
STUB

    cat > "$STUB_DIR/mount" <<'STUB'
#!/bin/sh
printf '%s\n' "zroot/ROOT/default / ${NORTHSTAR_TEST_ROOT_TYPE:-zfs} rw 0 0"
STUB

    cat > "$STUB_DIR/zfs" <<'STUB'
#!/bin/sh
exit 0
STUB

    cat > "$STUB_DIR/bectl" <<'STUB'
#!/bin/sh
case "${1:-}" in
    check) exit 0 ;;
    list) printf '%s\n' 'zroot/ROOT/default' ;;
    *) exit 0 ;;
esac
STUB

    cat > "$STUB_DIR/pkg" <<'STUB'
#!/bin/sh
set -eu
printf 'pkg %s\n' "$*" >> "$NORTHSTAR_TEST_LOG"
case "${1:-}" in
    update)
        exit 0
        ;;
    search)
        last=
        for argument do last=$argument; done
        case "$last" in
            missing*) exit 1 ;;
            *) exit 0 ;;
        esac
        ;;
    info)
        case "${2:-}" in
            -e)
                package=$3
                while IFS= read -r installed; do
                    [ "$installed" = "$package" ] && exit 0
                done < "$NORTHSTAR_TEST_INSTALLED"
                exit 1
                ;;
            -a)
                while IFS= read -r installed; do
                    [ -n "$installed" ] && printf '%s-1.0\n' "$installed"
                done < "$NORTHSTAR_TEST_INSTALLED"
                exit 0
                ;;
            *) exit 2 ;;
        esac
        ;;
    install)
        shift
        for argument do
            [ "$argument" = -y ] && continue
            printf '%s\n' "$argument" >> "$NORTHSTAR_TEST_INSTALLED"
        done
        exit 0
        ;;
    *) exit 2 ;;
esac
STUB

    cat > "$STUB_DIR/sysrc" <<'STUB'
#!/bin/sh
set -eu
printf 'sysrc %s\n' "$*" >> "$NORTHSTAR_TEST_LOG"
case "${1:-}" in
    -n)
        key=$2
        while IFS= read -r setting; do
            case "$setting" in
                "$key"=*) printf '%s\n' "${setting#*=}"; exit 0 ;;
            esac
        done < "$NORTHSTAR_TEST_SYSRC"
        exit 1
        ;;
    -c)
        setting=$2
        while IFS= read -r current; do
            [ "$current" = "$setting" ] && exit 0
        done < "$NORTHSTAR_TEST_SYSRC"
        exit 1
        ;;
    *)
        setting=$1
        while IFS= read -r current; do
            [ "$current" = "$setting" ] && exit 0
        done < "$NORTHSTAR_TEST_SYSRC"
        printf '%s\n' "$setting" >> "$NORTHSTAR_TEST_SYSRC"
        exit 0
        ;;
esac
STUB

    cat > "$STUB_DIR/service" <<'STUB'
#!/bin/sh
set -eu
printf 'service %s\n' "$*" >> "$NORTHSTAR_TEST_LOG"
service_name=$1
action=$2
case "$action" in
    status)
        while IFS= read -r current; do
            [ "$current" = "$service_name=running" ] && exit 0
        done < "$NORTHSTAR_TEST_SERVICES"
        exit 1
        ;;
    start)
        printf '%s=running\n' "$service_name" >> "$NORTHSTAR_TEST_SERVICES"
        exit 0
        ;;
    *) exit 2 ;;
esac
STUB

    cat > "$STUB_DIR/pw" <<'STUB'
#!/bin/sh
set -eu
printf 'pw %s\n' "$*" >> "$NORTHSTAR_TEST_LOG"
case "${1:-}" in
    groupshow)
        [ "${2:-}" = video ] || exit 1
        printf '%s\n' 'video:*:44:'
        exit 0
        ;;
    groupmod)
        : > "$NORTHSTAR_TEST_VIDEO_MEMBER"
        exit 0
        ;;
    *) exit 2 ;;
esac
STUB

    cat > "$STUB_DIR/id" <<'STUB'
#!/bin/sh
case "${1:-}" in
    -u)
        if [ "$#" -eq 1 ]; then
            printf '%s\n' 0
            exit 0
        fi
        [ "${2:-}" = dev ] && [ "${NORTHSTAR_TEST_USER_EXISTS:-1}" -eq 1 ] && printf '%s\n' 1000 && exit 0
        exit 1
        ;;
    -Gn)
        [ "${2:-}" = dev ] || exit 1
        if [ -f "$NORTHSTAR_TEST_VIDEO_MEMBER" ]; then
            printf '%s\n' 'video wheel'
        else
            printf '%s\n' 'wheel'
        fi
        ;;
    -un)
        printf '%s\n' dev
        ;;
    dev)
        [ "${NORTHSTAR_TEST_USER_EXISTS:-1}" -eq 1 ]
        ;;
    *) exit 1 ;;
esac
STUB
}

write_stubs

export NORTHSTAR_TEST_LOG=$LOG
export NORTHSTAR_TEST_INSTALLED=$INSTALLED
export NORTHSTAR_TEST_SYSRC=$SYSRC_STATE
export NORTHSTAR_TEST_SERVICES=$SERVICE_STATE
export NORTHSTAR_TEST_VIDEO_MEMBER=$VIDEO_MEMBER
export PATH=$STUB_DIR:$PATH

run_expect 0 sh "$ROOT/tools/check-host.sh"
pass 'check-host accepts the supported fixture'

NORTHSTAR_TEST_RELEASE=14.4-RELEASE
export NORTHSTAR_TEST_RELEASE
run_expect 1 sh "$ROOT/tools/check-host.sh"
pass 'check-host rejects an unsupported release'
unset NORTHSTAR_TEST_RELEASE

NORTHSTAR_TEST_ARCH=i386
export NORTHSTAR_TEST_ARCH
run_expect 1 sh "$ROOT/tools/check-host.sh"
pass 'check-host rejects an unsupported architecture'
unset NORTHSTAR_TEST_ARCH

run_expect 2 sh "$ROOT/tools/bootstrap-dev.sh"
pass 'bootstrap requires an explicit user'

reset_state
NORTHSTAR_TEST_USER_EXISTS=0
export NORTHSTAR_TEST_USER_EXISTS
run_expect 1 sh "$ROOT/tools/bootstrap-dev.sh" --user missing --manifest "$ROOT/packaging/manifests/bootstrap-packages.txt" --capture "$TMP_DIR/missing-user.txt"
assert_not_contains 'pkg update' "$LOG"
pass 'bootstrap rejects an unknown development user before package work'
NORTHSTAR_TEST_USER_EXISTS=1
export NORTHSTAR_TEST_USER_EXISTS

missing_manifest=$TMP_DIR/missing-packages.txt
printf '%s\n' available missing-package > "$missing_manifest"
reset_state
run_expect 1 sh "$ROOT/tools/bootstrap-dev.sh" --user dev --manifest "$missing_manifest" --capture "$TMP_DIR/missing-package.txt"
assert_contains 'pkg update' "$LOG"
assert_contains 'pkg search -U -e missing-package' "$LOG"
assert_not_contains 'pkg install' "$LOG"
assert_not_contains 'sysrc dbus_enable=YES' "$LOG"
pass 'bootstrap aborts before installation when a package is unavailable'

dry_manifest=$TMP_DIR/dry-run-packages.txt
printf '%s\n' available qterminal > "$dry_manifest"
reset_state
run_expect 0 sh "$ROOT/tools/bootstrap-dev.sh" --user dev --manifest "$dry_manifest" --capture "$TMP_DIR/dry-run.txt" --dry-run
assert_not_contains 'pkg ' "$LOG"
assert_not_contains 'sysrc dbus_enable=YES' "$LOG"
assert_not_contains 'service dbus start' "$LOG"
assert_not_contains 'pw groupmod' "$LOG"
[ ! -e "$TMP_DIR/dry-run.txt" ] || fail 'dry-run unexpectedly wrote a capture'
pass 'bootstrap dry-run performs no mutations'

capture=$TMP_DIR/success.txt
reset_state
run_expect 0 sh "$ROOT/tools/bootstrap-dev.sh" --user dev --manifest "$dry_manifest" --capture "$capture"
[ -f "$capture" ] || fail 'successful bootstrap did not write capture'
assert_contains 'pkg install -y' "$LOG"
assert_contains 'pw groupmod video -m dev' "$LOG"
assert_contains 'service dbus start' "$LOG"
assert_contains '[installed_packages]' "$capture"
pass 'bootstrap performs the first installation and writes capture'

: > "$LOG"
run_expect 0 sh "$ROOT/tools/bootstrap-dev.sh" --user dev --manifest "$dry_manifest" --capture "$capture"
assert_not_contains 'pkg install' "$LOG"
assert_not_contains 'sysrc dbus_enable=YES' "$LOG"
assert_not_contains 'service dbus start' "$LOG"
assert_not_contains 'pw groupmod' "$LOG"
pass 'bootstrap is idempotent on the second run'

diagnostics=$TMP_DIR/diagnostics
WAYLAND_DISPLAY=wayland-secret DISPLAY=:secret DBUS_SESSION_BUS_ADDRESS=secret-token XDG_RUNTIME_DIR=/secret/runtime QT_QPA_PLATFORM=wayland
export WAYLAND_DISPLAY DISPLAY DBUS_SESSION_BUS_ADDRESS XDG_RUNTIME_DIR QT_QPA_PLATFORM
run_expect 0 sh "$ROOT/tools/collect-diagnostics.sh" --output "$diagnostics"
for diagnostic_file in host.txt packages.txt services.txt session.txt; do
    [ -f "$diagnostics/$diagnostic_file" ] || fail "diagnostics file is missing: $diagnostic_file"
done
assert_not_contains 'secret-token' "$diagnostics/session.txt"
assert_not_contains '/secret/runtime' "$diagnostics/session.txt"
pass 'diagnostics writes the expected files without sensitive values'

printf 'All M0 script tests passed.\n'
