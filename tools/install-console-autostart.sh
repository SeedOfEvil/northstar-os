#!/bin/sh

# Install the opt-in console-login hook for the current unprivileged user.
# This only starts startx from a local FreeBSD virtual console; it never
# starts a graphical session for SSH or serial logins.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SOURCE_FILE=$PROJECT_DIR/config/northstar-console-autostart.sh
PROFILE_TARGET=$HOME/.profile
CONFIG_DIR=$HOME/.config/northstar
TARGET_FILE=$CONFIG_DIR/console-autostart.sh
ENABLED_MARKER=$CONFIG_DIR/console-autostart.enabled

FORCE=0
ACTION=enable

usage() {
    cat <<'USAGE'
Usage: install-console-autostart.sh [--enable|--disable] [--force]

Install or disable Northstar's opt-in console-login autostart hook.
The hook runs only on ttyv0 through ttyv7 and leaves SSH/serial logins alone.
Existing profile content is preserved.
USAGE
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --enable)
            ACTION=enable
            shift
            ;;
        --disable)
            ACTION=disable
            shift
            ;;
        --force)
            FORCE=1
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
    printf '%s\n' 'ERROR: install console autostart as the unprivileged desktop user' >&2
    exit 1
fi

if [ ! -f "$SOURCE_FILE" ]; then
    printf '%s\n' "ERROR: project autostart file is missing: $SOURCE_FILE" >&2
    exit 2
fi

umask 077
mkdir -p "$CONFIG_DIR"

if [ "$ACTION" = disable ]; then
    rm -f "$ENABLED_MARKER"
    printf '%s\n' "Disabled Northstar console autostart for ${USER:-the current user}."
    printf '%s\n' 'The profile hook remains installed and can be re-enabled later.'
    exit 0
fi

if [ -e "$TARGET_FILE" ] || [ -L "$TARGET_FILE" ]; then
    if ! cmp -s "$SOURCE_FILE" "$TARGET_FILE"; then
        if [ "$FORCE" -eq 0 ]; then
            printf '%s\n' "ERROR: refusing to replace existing file: $TARGET_FILE" >&2
            printf '%s\n' 'Re-run with --force to create a backup and install the project file.' >&2
            exit 1
        fi
        backup_file=$TARGET_FILE.northstar-backup-$(date +%Y%m%d%H%M%S)-$$
        mv "$TARGET_FILE" "$backup_file"
        printf '%s\n' "Backed up $TARGET_FILE to $backup_file"
    fi
fi
install -m 700 "$SOURCE_FILE" "$TARGET_FILE"

mkdir -p "$(dirname -- "$PROFILE_TARGET")"
if [ ! -e "$PROFILE_TARGET" ] && [ ! -L "$PROFILE_TARGET" ]; then
    : > "$PROFILE_TARGET"
    chmod 600 "$PROFILE_TARGET"
fi

if ! grep -F 'northstar-console-autostart: begin' "$PROFILE_TARGET" >/dev/null 2>&1; then
    printf '%s\n' '' >> "$PROFILE_TARGET"
    printf '%s\n' '# northstar-console-autostart: begin' >> "$PROFILE_TARGET"
    printf '%s\n' 'if [ -r "$HOME/.config/northstar/console-autostart.sh" ]; then' >> "$PROFILE_TARGET"
    printf '%s\n' '    . "$HOME/.config/northstar/console-autostart.sh"' >> "$PROFILE_TARGET"
    printf '%s\n' 'fi' >> "$PROFILE_TARGET"
    printf '%s\n' '# northstar-console-autostart: end' >> "$PROFILE_TARGET"
fi

: > "$ENABLED_MARKER"
chmod 600 "$ENABLED_MARKER"

printf '%s\n' "Enabled Northstar console autostart for ${USER:-the current user}."
printf '%s\n' 'The desktop will start after the next local ttyv login; SSH logins are unaffected.'
printf '%s\n' 'Disable it with: sh tools/install-console-autostart.sh --disable'
