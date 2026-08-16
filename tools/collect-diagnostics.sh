#!/bin/sh

# Collect non-secret Northstar host diagnostics. This script never captures the
# complete environment, process command lines, credentials, or private keys.

set -eu

PROG=${0##*/}
OUTPUT=${TMPDIR:-/tmp}/northstar-diagnostics-$(date -u '+%Y%m%dT%H%M%SZ')
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

usage() {
    cat <<USAGE
Usage: $PROG [--output DIRECTORY]

Collect sanitized M0 host, package, service, and session diagnostics.
USAGE
}

usage_error() {
    printf 'ERROR: %s\n' "$1" >&2
    usage >&2
    exit 2
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --output)
            [ "$#" -ge 2 ] || usage_error '--output requires a value'
            OUTPUT=$2
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            usage_error "unknown option: $1"
            ;;
    esac
done

mkdir -p "$OUTPUT"
chmod 0700 "$OUTPUT"

write_host() {
    {
        printf '# Northstar host diagnostics\n'
        printf 'captured_at_utc=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
        if command -v freebsd-version >/dev/null 2>&1; then
            freebsd-version -kru 2>&1 || true
        else
            printf 'freebsd-version: unavailable\n'
        fi
        if command -v uname >/dev/null 2>&1; then
            uname -a 2>&1 || true
            uname -m 2>&1 || true
        fi
        if command -v sysctl >/dev/null 2>&1; then
            sysctl machdep.bootmethod 2>&1 || true
        fi
        if command -v mount >/dev/null 2>&1; then
            mount -p 2>&1 || true
        fi
        if command -v zfs >/dev/null 2>&1; then
            zfs list -H -o name,mountpoint 2>&1 || true
        fi
        if command -v bectl >/dev/null 2>&1; then
            bectl list -H 2>&1 || true
        fi
    } > "$OUTPUT/host.txt"
}

write_packages() {
    {
        printf '# Installed package names and versions\n'
        if command -v pkg >/dev/null 2>&1; then
            pkg info -a -q 2>&1 | sort || true
        else
            printf 'pkg: unavailable\n'
        fi
    } > "$OUTPUT/packages.txt"
}

write_services() {
    {
        printf '# Service and group state\n'
        if command -v sysrc >/dev/null 2>&1; then
            printf 'dbus_enable='
            sysrc -n dbus_enable 2>/dev/null || printf 'unknown'
            printf '\nseatd_enable='
            sysrc -n seatd_enable 2>/dev/null || printf 'unknown'
            printf '\n'
        fi
        if command -v service >/dev/null 2>&1; then
            printf '\n[dbus]\n'
            service dbus status 2>&1 || true
            printf '\n[seatd]\n'
            service seatd status 2>&1 || true
        fi
        if command -v pw >/dev/null 2>&1; then
            printf '\n[video_group]\n'
            pw groupshow video 2>&1 || true
        fi
        if command -v id >/dev/null 2>&1; then
            printf '\n[current_user]\n'
            id -un 2>&1 || true
            id -Gn 2>&1 || true
        fi
    } > "$OUTPUT/services.txt"
}

write_session() {
    {
        printf '# Session presence checks; values are intentionally omitted\n'
        if [ -n "${WAYLAND_DISPLAY:-}" ]; then
            printf 'WAYLAND_DISPLAY=present\n'
        else
            printf 'WAYLAND_DISPLAY=absent\n'
        fi
        if [ -n "${DISPLAY:-}" ]; then
            printf 'DISPLAY=present\n'
        else
            printf 'DISPLAY=absent\n'
        fi
        if [ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ]; then
            printf 'DBUS_SESSION_BUS_ADDRESS=present\n'
        else
            printf 'DBUS_SESSION_BUS_ADDRESS=absent\n'
        fi
        if [ -n "${XDG_RUNTIME_DIR:-}" ]; then
            printf 'XDG_RUNTIME_DIR=present\n'
        else
            printf 'XDG_RUNTIME_DIR=absent\n'
        fi
        if [ -n "${QT_QPA_PLATFORM:-}" ]; then
            printf 'QT_QPA_PLATFORM=present\n'
        else
            printf 'QT_QPA_PLATFORM=absent\n'
        fi
    } > "$OUTPUT/session.txt"
}

write_host
write_packages
write_services
write_session
sh "$SCRIPT_DIR/collect-alpha-readiness.sh" --output "$OUTPUT/alpha-readiness.conf" >/dev/null

chmod 0600 "$OUTPUT/host.txt" "$OUTPUT/packages.txt" "$OUTPUT/services.txt" "$OUTPUT/session.txt" "$OUTPUT/alpha-readiness.conf"
printf 'Diagnostics written to %s\n' "$OUTPUT"
