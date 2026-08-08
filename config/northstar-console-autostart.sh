#!/bin/sh

# This file is sourced from the user's login profile. It intentionally starts
# Northstar only from a local virtual console when the explicit enable marker
# exists; SSH and serial logins remain ordinary shells.

northstar_console_autostart() {
    [ "${NORTHSTAR_NO_AUTOSTART:-0}" = 1 ] && return 0
    [ "${NORTHSTAR_CONSOLE_AUTOSTART_ATTEMPTED:-0}" = 1 ] && return 0

    case "$(tty 2>/dev/null || true)" in
        /dev/ttyv[0-7])
            ;;
        *)
            return 0
            ;;
    esac

    [ -z "${DISPLAY:-}" ] || return 0
    [ -z "${WAYLAND_DISPLAY:-}" ] || return 0

    enabled_marker=${NORTHSTAR_CONSOLE_AUTOSTART_MARKER:-$HOME/.config/northstar/console-autostart.enabled}
    [ -f "$enabled_marker" ] || return 0

    startx_bin=${NORTHSTAR_STARTX_BIN:-/usr/local/bin/startx}
    if [ ! -x "$startx_bin" ]; then
        printf '%s\n' "ERROR: Northstar console autostart cannot find $startx_bin" >&2
        return 0
    fi

    export NORTHSTAR_CONSOLE_AUTOSTART_ATTEMPTED=1
    exec "$startx_bin"
}

northstar_console_autostart
