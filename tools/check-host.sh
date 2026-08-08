#!/bin/sh

# Read-only M0 host validation for Northstar.
#
# Exit status:
#   0  all supported-host checks passed
#   1  one or more host checks failed
#   2  invalid usage or required validation tooling is unavailable

set -u

PROG=${0##*/}
EXPECTED_RELEASE=${NORTHSTAR_FREEBSD_RELEASE:-15.1-RELEASE}
failures=0

usage() {
    cat <<USAGE
Usage: $PROG

Validate the Northstar M0 host without changing system state.
The supported baseline is FreeBSD ${EXPECTED_RELEASE}, amd64, UEFI,
and a root filesystem mounted from ZFS.
USAGE
}

fail_check() {
    printf 'FAIL: %s\n' "$1" >&2
    failures=$((failures + 1))
}

pass_check() {
    printf 'PASS: %s\n' "$1"
}

require_command() {
    command_name=$1
    if command -v "$command_name" >/dev/null 2>&1; then
        return 0
    fi
    fail_check "required command is unavailable: $command_name"
    return 1
}

release_matches() {
    case "$1" in
        "$EXPECTED_RELEASE"|"$EXPECTED_RELEASE"-p*) return 0 ;;
        *) return 1 ;;
    esac
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --help|-h)
            usage
            exit 0
            ;;
        *)
            printf 'ERROR: unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

printf 'Northstar M0 host check\n'
printf 'Expected release: %s\n' "$EXPECTED_RELEASE"

for command_name in \
    freebsd-version \
    uname \
    sysctl \
    mount \
    awk \
    zfs \
    bectl \
    pkg \
    sysrc \
    service \
    pw
do
    require_command "$command_name" || true
done

if command -v freebsd-version >/dev/null 2>&1; then
    userland_version=$(freebsd-version -u 2>/dev/null || printf 'unavailable')
    kernel_version=$(freebsd-version -k 2>/dev/null || printf 'unavailable')

    if release_matches "$userland_version"; then
        pass_check "userland release is $userland_version"
    else
        fail_check "userland release is $userland_version; expected $EXPECTED_RELEASE"
    fi

    if release_matches "$kernel_version"; then
        pass_check "kernel release is $kernel_version"
    else
        fail_check "kernel release is $kernel_version; expected $EXPECTED_RELEASE"
    fi
fi

if command -v uname >/dev/null 2>&1; then
    architecture=$(uname -m 2>/dev/null || printf 'unavailable')
    if [ "$architecture" = amd64 ]; then
        pass_check 'architecture is amd64'
    else
        fail_check "architecture is $architecture; expected amd64"
    fi
fi

if command -v sysctl >/dev/null 2>&1; then
    boot_method=$(sysctl -n machdep.bootmethod 2>/dev/null || printf 'unavailable')
    if [ "$boot_method" = UEFI ]; then
        pass_check 'boot method is UEFI'
    else
        fail_check "boot method is $boot_method; expected UEFI"
    fi
fi

if command -v mount >/dev/null 2>&1 && command -v awk >/dev/null 2>&1; then
    root_source=$(mount -p 2>/dev/null | awk '$2 == "/" { print $1; exit }')
    root_type=$(mount -p 2>/dev/null | awk '$2 == "/" { print $3; exit }')
    if [ "$root_type" = zfs ] && [ -n "$root_source" ]; then
        pass_check "root filesystem is ZFS ($root_source)"
    else
        fail_check "root filesystem is ${root_type:-unknown} from ${root_source:-unknown}; expected ZFS"
    fi
fi

if command -v bectl >/dev/null 2>&1; then
    if bectl check >/dev/null 2>&1; then
        pass_check 'ZFS boot-environment support is available'
    else
        fail_check 'bectl check failed; a usable ZFS boot environment is required'
    fi
fi

if [ "$failures" -ne 0 ]; then
    printf 'M0 host check failed with %s issue(s).\n' "$failures" >&2
    exit 1
fi

printf 'M0 host check passed.\n'
exit 0
