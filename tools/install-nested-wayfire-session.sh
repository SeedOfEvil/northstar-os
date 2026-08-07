#!/bin/sh

# Install the supplemental basic-VGA Wayfire session files for one user.
# Existing user configuration is never replaced unless --force is supplied;
# forced replacements are moved to a timestamped backup first.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
XINITRC_SOURCE=$PROJECT_DIR/config/xinitrc.nested-wayfire
SUPERVISED_XINITRC_SOURCE=$PROJECT_DIR/config/xinitrc.nested-wayfire-supervised
WAYFIRE_CONFIG_SOURCE=$PROJECT_DIR/config/wayfire-nested.ini

FORCE=0
SUPERVISED=0

usage() {
    cat <<'USAGE'
Usage: install-nested-wayfire-session.sh [--force] [--supervised]

Install the user-level .xinitrc and Wayfire configuration for the
supplemental Proxmox basic-VGA lane.

The nested Wayfire binary must already exist at
~/.local/wayfire-nested/bin/wayfire, or at NORTHSTAR_WAYFIRE_BIN.
Existing files are preserved. Use --force to back them up and replace them.
Use --supervised to install the opt-in .xinitrc that starts northstar-session;
the user-local Northstar binaries must already be installed.
USAGE
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --force)
            FORCE=1
            shift
            ;;
        --supervised)
            SUPERVISED=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            printf '%s\n' "ERROR: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [ "$(id -u)" -eq 0 ]; then
    printf '%s\n' 'ERROR: install the nested session as the unprivileged development user' >&2
    exit 1
fi

if [ -z "${HOME:-}" ]; then
    printf '%s\n' 'ERROR: HOME is not set' >&2
    exit 1
fi

if [ "$SUPERVISED" -eq 1 ]; then
    XINITRC_SOURCE=$SUPERVISED_XINITRC_SOURCE
fi

wayfire_bin=${NORTHSTAR_WAYFIRE_BIN:-$HOME/.local/wayfire-nested/bin/wayfire}
if [ ! -x "$wayfire_bin" ]; then
    printf '%s\n' "ERROR: nested Wayfire binary is unavailable: $wayfire_bin" >&2
    printf '%s\n' 'Build it with: make nested-wayfire' >&2
    exit 1
fi

for source_file in "$XINITRC_SOURCE" "$WAYFIRE_CONFIG_SOURCE"; do
    if [ ! -f "$source_file" ]; then
        printf '%s\n' "ERROR: project session file is missing: $source_file" >&2
        exit 2
    fi
done

xinitrc_target=$HOME/.xinitrc
wayfire_config_target=$HOME/.config/wayfire.ini

destination_conflict() {
    source_file=$1
    target_file=$2
    if [ -e "$target_file" ] || [ -L "$target_file" ]; then
        if cmp -s "$source_file" "$target_file"; then
            return 1
        fi
        if [ "$FORCE" -eq 0 ]; then
            printf '%s\n' "ERROR: refusing to replace existing file: $target_file" >&2
            printf '%s\n' 'Re-run with --force to create a backup and install the project file.' >&2
            return 0
        fi
    fi
    return 1
}

if destination_conflict "$XINITRC_SOURCE" "$xinitrc_target"; then
    exit 1
fi
if destination_conflict "$WAYFIRE_CONFIG_SOURCE" "$wayfire_config_target"; then
    exit 1
fi

backup_and_install() {
    source_file=$1
    target_file=$2
    mode=$3

    mkdir -p "$(dirname -- "$target_file")"
    if [ -e "$target_file" ] || [ -L "$target_file" ]; then
        if cmp -s "$source_file" "$target_file"; then
            printf '%s\n' "Already installed: $target_file"
            return 0
        fi
        backup_file=$target_file.northstar-backup-$(date +%Y%m%d%H%M%S)-$$
        mv "$target_file" "$backup_file"
        printf '%s\n' "Backed up $target_file to $backup_file"
    fi
    install -m "$mode" "$source_file" "$target_file"
    printf '%s\n' "Installed: $target_file"
}

umask 077
backup_and_install "$XINITRC_SOURCE" "$xinitrc_target" 700
backup_and_install "$WAYFIRE_CONFIG_SOURCE" "$wayfire_config_target" 600

cat <<SUMMARY
Nested Wayfire session installed for ${USER:-the current user}.
Start it from the Proxmox console as this user with:
  startx
SUMMARY
if [ "$SUPERVISED" -eq 1 ]; then
    printf '%s\n' 'The opt-in session starts northstar-session and supervises the shell.'
else
    printf '%s\n' 'The session starts Wayfire directly.'
fi
printf '%s\n' 'The session uses X11/pixman and is supplemental; it does not satisfy the direct DRM/KMS gate.'
