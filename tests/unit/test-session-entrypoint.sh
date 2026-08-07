#!/bin/sh

# Verify that the built project installs a standard, user-selectable Wayland
# session entry point without writing outside a temporary staging prefix.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
BUILD_DIR=${1:-$ROOT/build}
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-session-install.XXXXXX")
PREFIX=$TMP_DIR/prefix
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

[ "$(id -u)" -ne 0 ] || {
    printf '%s\n' 'SKIP: session entry-point install test requires an unprivileged account'
    exit 0
}

[ -d "$BUILD_DIR" ] || fail "build directory is missing: $BUILD_DIR"

if ! cmake --install "$BUILD_DIR" --prefix "$PREFIX" > "$OUTPUT" 2> "$ERROR_OUTPUT"; then
    cat "$OUTPUT" >&2 || true
    cat "$ERROR_OUTPUT" >&2 || true
    fail 'staged CMake install failed'
fi

[ -x "$PREFIX/bin/northstar-session" ] || fail 'northstar-session was not installed executable'
[ -x "$PREFIX/bin/northstar-shell" ] || fail 'northstar-shell was not installed executable'
if [ ! -f "$PREFIX/share/wayland-sessions/northstar.desktop" ]; then
    fail 'Northstar Wayland session descriptor was not installed'
fi

desktop=$PREFIX/share/wayland-sessions/northstar.desktop
if ! grep -F 'Exec=northstar-session' "$desktop" >/dev/null; then
    fail 'session descriptor does not launch northstar-session'
fi
if ! grep -F 'TryExec=northstar-session' "$desktop" >/dev/null; then
    fail 'session descriptor does not provide TryExec'
fi
if ! grep -F 'DesktopNames=Northstar' "$desktop" >/dev/null; then
    fail 'session descriptor is missing DesktopNames'
fi

if grep -Eiq '(^|[[:space:]])(sudo|su|doas|pkexec|shutdown|reboot)([[:space:]]|$)' "$PREFIX/bin/northstar-session" "$desktop"; then
    fail 'session entry point contains an unapproved privileged lifecycle command'
fi

pass 'staged install contains the unprivileged Northstar session entry point'
printf '%s\n' 'All session entry-point tests passed.'
