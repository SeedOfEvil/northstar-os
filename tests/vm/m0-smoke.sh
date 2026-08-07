#!/bin/sh

# M0 session smoke test. Run this as the unprivileged development user from
# inside the Wayfire session after bootstrap-dev.sh and a new login.

set -eu

LAUNCH=0
TMP_DIR=
WAYFIRE_BIN=${NORTHSTAR_WAYFIRE_BIN:-}

usage() {
    cat <<USAGE
Usage: $0 [--launch]

Check the M0 package/session prerequisites. Set NORTHSTAR_WAYFIRE_BIN to
validate the user-local nested Wayfire binary instead of the package binary.
With --launch, briefly start
QTerminal through Wayland, Firefox through Wayland, and xterm through
Xwayland, then clean up the processes started by this test.
USAGE
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --launch)
            LAUNCH=1
            shift
            ;;
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

if [ "$(id -u)" -eq 0 ]; then
    printf '%s\n' 'ERROR: run the smoke test as the unprivileged development user' >&2
    exit 1
fi

for command_name in pkg qterminal firefox xterm dbus-run-session Xwayland; do
    command -v "$command_name" >/dev/null 2>&1 || {
        printf 'ERROR: required command is unavailable: %s\n' "$command_name" >&2
        exit 1
    }
done

if [ -n "$WAYFIRE_BIN" ]; then
    [ -x "$WAYFIRE_BIN" ] || {
        printf 'ERROR: nested Wayfire binary is not executable: %s\n' "$WAYFIRE_BIN" >&2
        exit 1
    }
    WAYFIRE_MODE="nested ($WAYFIRE_BIN)"
else
    command -v wayfire >/dev/null 2>&1 || {
        printf '%s\n' 'ERROR: required command is unavailable: wayfire' >&2
        exit 1
    }
    WAYFIRE_MODE=package
fi

for package_name in xwayland qterminal firefox xterm; do
    pkg info -e "$package_name" >/dev/null 2>&1 || {
        printf 'ERROR: required package is not installed: %s\n' "$package_name" >&2
        exit 1
    }
done

[ -n "${WAYLAND_DISPLAY:-}" ] || {
    printf '%s\n' 'ERROR: WAYLAND_DISPLAY is not set; start this from the Wayfire session' >&2
    exit 1
}
[ -n "${DISPLAY:-}" ] || {
    printf '%s\n' 'ERROR: DISPLAY is not set; Xwayland is not available in this session' >&2
    exit 1
}

printf 'PASS: M0 packages and Wayland/Xwayland session variables are present (%s)\n' "$WAYFIRE_MODE"

if [ "$LAUNCH" -eq 0 ]; then
    cat <<COMMANDS
Manual visual smoke commands:
  QT_QPA_PLATFORM=wayland qterminal
  MOZ_ENABLE_WAYLAND=1 firefox
  xterm

Use --launch for short process-level launch checks.
COMMANDS
    exit 0
fi

TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-m0-smoke.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

launch_and_check() {
    label=$1
    shift
    log_file=$TMP_DIR/$(printf '%s' "$label" | tr ' ' '_').log

    "$@" > "$log_file" 2>&1 &
    process_id=$!
    sleep 2
    if kill -0 "$process_id" >/dev/null 2>&1; then
        printf 'PASS: %s launched (pid %s)\n' "$label" "$process_id"
        kill "$process_id" >/dev/null 2>&1 || true
        wait "$process_id" >/dev/null 2>&1 || true
    else
        printf 'FAIL: %s did not remain running\n' "$label" >&2
        cat "$log_file" >&2 || true
        return 1
    fi
}

launch_and_check 'qterminal-wayland' env QT_QPA_PLATFORM=wayland qterminal
launch_and_check 'firefox-wayland' env MOZ_ENABLE_WAYLAND=1 firefox --new-instance --profile "$TMP_DIR/firefox-profile"
launch_and_check 'xterm-xwayland' xterm

printf 'M0 smoke checks passed (%s).\n' "$WAYFIRE_MODE"
