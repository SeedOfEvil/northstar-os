#!/bin/sh

# Install or remove the explicit Northstar SDDM/basic-VGA fallback policy.
# This changes only the managed SDDM drop-in and sddm_enable rc.conf key.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
PREFIX=${NORTHSTAR_PREFIX:-/usr/local}
CONFIG_DIR=${NORTHSTAR_SDDM_CONFIG_DIR:-$PREFIX/etc/sddm.conf.d}
CONFIG_FILE=$CONFIG_DIR/20-northstar-proxmox.conf
SOURCE_CONFIG=$PROJECT_DIR/config/sddm/northstar-proxmox.conf
STATE_DIR=${NORTHSTAR_SDDM_STATE_DIR:-/var/db/northstar}
STATE_FILE=$STATE_DIR/sddm-fallback.enabled
GREETER_LINK=$PREFIX/bin/sddm-greeter
GREETER_TARGET=sddm-greeter-qt6
GREETER_LINK_STATE=$STATE_DIR/sddm-greeter.link
ACTION=enable

usage() {
    cat <<USAGE
Usage: install-sddm-fallback.sh [--enable|--disable]

Install or remove Northstar's explicit Proxmox/basic-VGA SDDM policy.

Environment:
  NORTHSTAR_PREFIX          package prefix (default: /usr/local)
  NORTHSTAR_SDDM_CONFIG_DIR SDDM drop-in directory
  NORTHSTAR_SDDM_STATE_DIR  managed state directory
USAGE
}

die() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

backup_conflict() {
    target=$1
    source=$2
    if [ -e "$target" ] && ! cmp -s "$source" "$target"; then
        timestamp=$(date '+%Y%m%d%H%M%S')
        backup=$target.northstar-backup-$timestamp
        cp -p "$target" "$backup"
        printf '%s\n' "Backed up $target to $backup"
    fi
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --enable)
            ACTION=enable
            ;;
        --disable)
            ACTION=disable
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            usage >&2
            exit 2
            ;;
    esac
    shift
done

[ "$(id -u)" -eq 0 ] || die 'run this installer as root'
[ -x "$PREFIX/bin/sddm" ] || die "SDDM is not installed below $PREFIX"

if [ "$ACTION" = enable ]; then
    [ -x "$PREFIX/bin/northstar-session-x11" ] || die "missing fallback launcher: $PREFIX/bin/northstar-session-x11"
    [ -f "$PREFIX/share/sddm/themes/northstar/metadata.desktop" ] || die 'Northstar SDDM theme is not installed'
    [ -f "$PREFIX/share/wayland-sessions/northstar.desktop" ] || die 'Northstar Wayland session is not installed'
    [ -f "$SOURCE_CONFIG" ] || die "missing source configuration: $SOURCE_CONFIG"
    [ -x "$PREFIX/bin/$GREETER_TARGET" ] || die "missing FreeBSD Qt6 greeter: $PREFIX/bin/$GREETER_TARGET"

    install -d -m 755 "$CONFIG_DIR"
    install -d -m 700 "$STATE_DIR"
    backup_conflict "$CONFIG_FILE" "$SOURCE_CONFIG"
    install -m 644 "$SOURCE_CONFIG" "$CONFIG_FILE"

    if [ -e "$GREETER_LINK" ] || [ -L "$GREETER_LINK" ]; then
        if [ ! -L "$GREETER_LINK" ] || [ "$(readlink "$GREETER_LINK")" != "$GREETER_TARGET" ]; then
            die "refusing to replace existing SDDM greeter: $GREETER_LINK"
        fi
    else
        ln -s "$GREETER_TARGET" "$GREETER_LINK"
        : > "$GREETER_LINK_STATE"
        chmod 600 "$GREETER_LINK_STATE"
    fi

    if command -v sysrc >/dev/null 2>&1; then
        sysrc sddm_enable=YES >/dev/null
    else
        die 'sysrc is unavailable; cannot enable the FreeBSD SDDM service safely'
    fi

    : > "$STATE_FILE"
    chmod 600 "$STATE_FILE"
    printf '%s\n' 'Enabled Northstar SDDM Proxmox fallback policy.'
    printf '%s\n' 'Before rebooting: disable console autostart for the desktop user and keep SSH/recovery access available.'
    printf '%s\n' 'The SDDM session to select is: Northstar (Proxmox X11 fallback)'
else
    if [ -e "$CONFIG_FILE" ]; then
        if cmp -s "$SOURCE_CONFIG" "$CONFIG_FILE"; then
            rm -f "$CONFIG_FILE"
        else
            die "refusing to remove a changed SDDM drop-in: $CONFIG_FILE"
        fi
    fi
    if [ -f "$STATE_FILE" ]; then
        sysrc sddm_enable=NO >/dev/null
        rm -f "$STATE_FILE"
    fi
    if [ -f "$GREETER_LINK_STATE" ]; then
        if [ -L "$GREETER_LINK" ] && [ "$(readlink "$GREETER_LINK")" = "$GREETER_TARGET" ]; then
            rm -f "$GREETER_LINK"
        fi
        rm -f "$GREETER_LINK_STATE"
    fi
    rmdir "$STATE_DIR" 2>/dev/null || true
    printf '%s\n' 'Disabled Northstar SDDM Proxmox fallback policy.'
fi
