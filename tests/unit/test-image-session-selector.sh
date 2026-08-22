#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
SELECTOR=$ROOT/image/session/northstar-session-selector
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-session-selector.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

FAKE_ROOT=$TMP_DIR/root
SOURCE=$FAKE_ROOT/usr/local/share/northstar/image-sessions
mkdir -p "$SOURCE" "$FAKE_ROOT/var/db/northstar"
cp "$ROOT/config/session/northstar.desktop" "$SOURCE/northstar.desktop"
cp "$ROOT/config/session/northstar-proxmox.desktop" \
    "$SOURCE/northstar-image-proxmox.desktop"
cp "$ROOT/apps/first-boot/northstar-first-boot.desktop" \
    "$SOURCE/northstar-first-boot.desktop"
cat > "$SOURCE/northstar-installer.desktop" <<'EOF'
[Desktop Entry]
Name=Northstar Installer
Exec=/usr/local/bin/northstar-installer-session
Type=XSession
EOF
printf '%s\n' 'status=pending' > "$FAKE_ROOT/var/db/northstar/first-boot.pending"

run_selector() {
    env NORTHSTAR_SESSION_SELECTOR_TEST_MODE=1 \
        NORTHSTAR_SESSION_SELECTOR_ROOT="$FAKE_ROOT" \
        NORTHSTAR_SESSION_SELECTOR_CP_PATH=/bin/cp \
        NORTHSTAR_SESSION_SELECTOR_RM_PATH=/bin/rm \
        sh "$SELECTOR"
}

run_selector | grep -Fx 'NORTHSTAR_SESSION_MODE=fallback' >/dev/null \
    || fail 'no-DRM system did not select fallback'
[ -f "$FAKE_ROOT/var/run/northstar/xsessions/northstar-image-proxmox.desktop" ] \
    || fail 'fallback descriptor was not published'
[ ! -e "$FAKE_ROOT/var/run/northstar/wayland-sessions/northstar.desktop" ] \
    || fail 'native descriptor was published without DRM'
[ -f "$FAKE_ROOT/var/run/northstar/xsessions/northstar-first-boot.desktop" ] \
    || fail 'pending first-boot descriptor was not published'

mkdir -p "$FAKE_ROOT/dev/dri" "$FAKE_ROOT/dev/drm"
printf '%s\n' card > "$FAKE_ROOT/dev/drm/0"
printf '%s\n' render > "$FAKE_ROOT/dev/drm/128"
ln -s ../drm/0 "$FAKE_ROOT/dev/dri/card0"
ln -s ../drm/128 "$FAKE_ROOT/dev/dri/renderD128"
run_selector | grep -Fx 'NORTHSTAR_SESSION_MODE=native' >/dev/null \
    || fail 'FreeBSD DRM symlinks did not select native Wayland'
[ -f "$FAKE_ROOT/var/run/northstar/wayland-sessions/northstar.desktop" ] \
    || fail 'native descriptor was not published'
[ ! -e "$FAKE_ROOT/var/run/northstar/xsessions/northstar-image-proxmox.desktop" ] \
    || fail 'fallback descriptor remained published with DRM ready'
grep -Fx 'mode=native' "$FAKE_ROOT/var/run/northstar/session-mode.conf" >/dev/null \
    || fail 'native session mode state was not recorded'

rm -f "$FAKE_ROOT/dev/drm/128" "$FAKE_ROOT/dev/dri/renderD128" \
    "$FAKE_ROOT/var/db/northstar/first-boot.pending"
run_selector >/dev/null
[ ! -e "$FAKE_ROOT/var/run/northstar/xsessions/northstar-first-boot.desktop" ] \
    || fail 'sealed first-boot descriptor remained published'
grep -Fx 'mode=fallback' "$FAKE_ROOT/var/run/northstar/session-mode.conf" >/dev/null \
    || fail 'missing render node did not return to fallback'

printf '%s\n' render > "$FAKE_ROOT/dev/drm/128"
ln -s ../drm/128 "$FAKE_ROOT/dev/dri/renderD128"
printf '%s\n' 'purpose=northstar-installer-media' \
    > "$FAKE_ROOT/var/db/northstar/installer-media.conf"
run_selector >/dev/null
[ -f "$FAKE_ROOT/var/run/northstar/xsessions/northstar-installer.desktop" ] \
    || fail 'installer media session was not published'
[ ! -e "$FAKE_ROOT/var/run/northstar/wayland-sessions/northstar.desktop" ] \
    || fail 'native desktop session was exposed on installer media'
[ ! -e "$FAKE_ROOT/var/run/northstar/xsessions/northstar-image-proxmox.desktop" ] \
    || fail 'fallback desktop session was exposed on installer media'
[ ! -e "$FAKE_ROOT/var/run/northstar/xsessions/northstar-first-boot.desktop" ] \
    || fail 'first-boot session was exposed on installer media'
grep -Fx 'mode=native' "$FAKE_ROOT/var/run/northstar/session-mode.conf" >/dev/null \
    || fail 'installer media did not retain native hardware state'

if grep -Eq '^\[output:' "$ROOT/config/wayfire-native.ini"; then
    fail 'native Wayfire configuration hardcodes an output'
fi
grep -F '[output:X11-1]' "$ROOT/config/wayfire-nested.ini" >/dev/null \
    || fail 'Proxmox Wayfire configuration lost its bounded X11 output'

printf '%s\n' 'PASS: image session selector separates native DRM and Proxmox fallback sessions'
