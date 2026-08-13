#!/bin/sh

# Build the supplemental Proxmox nested Wayfire lane. Developer builds remain
# user-local by default; release packaging uses an explicit system prefix and
# DESTDIR without replacing FreeBSD's package-managed Wayfire.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
PATCH_FILE=$PROJECT_DIR/packaging/patches/wayfire/0001-allow-x11-pixman-without-drm.patch

WAYFIRE_REPOSITORY=${WAYFIRE_REPOSITORY:-https://github.com/WayfireWM/wayfire.git}
WAYFIRE_TAG=${WAYFIRE_TAG:-v0.10.1}
WAYFIRE_SOURCE_DIR=${WAYFIRE_SOURCE_DIR:-$HOME/src/wayfire-nested-v0.10.1}
WAYFIRE_BUILD_DIR=${WAYFIRE_BUILD_DIR:-$WAYFIRE_SOURCE_DIR/build}
WAYFIRE_PREFIX=${WAYFIRE_PREFIX:-$HOME/.local/wayfire-nested}
WAYFIRE_DESTDIR=${WAYFIRE_DESTDIR:-}

usage() {
    cat <<'USAGE'
Usage: build-nested-wayfire.sh [--help]

Builds Wayfire v0.10.1 with Northstar's supplemental X11/pixman/no-DRM
compatibility patch and installs it below ~/.local by default.

Environment overrides:
  WAYFIRE_SOURCE_DIR  source checkout directory
  WAYFIRE_BUILD_DIR   Meson build directory
  WAYFIRE_PREFIX       compiled install prefix
  WAYFIRE_DESTDIR      optional package-staging root for Meson install
USAGE
}

if [ "$#" -gt 0 ]; then
    case "$1" in
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
fi

for command_name in git meson ninja pkg-config; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        printf '%s\n' "ERROR: required command is missing: $command_name" >&2
        exit 2
    fi
done

if [ ! -f "$PATCH_FILE" ]; then
    printf '%s\n' "ERROR: patch file is missing: $PATCH_FILE" >&2
    exit 2
fi

mkdir -p "$(dirname "$WAYFIRE_SOURCE_DIR")"

if [ ! -d "$WAYFIRE_SOURCE_DIR/.git" ]; then
    if [ -e "$WAYFIRE_SOURCE_DIR" ]; then
        printf '%s\n' "ERROR: source path exists but is not a Git checkout: $WAYFIRE_SOURCE_DIR" >&2
        exit 1
    fi
    git clone --depth 1 --branch "$WAYFIRE_TAG" "$WAYFIRE_REPOSITORY" "$WAYFIRE_SOURCE_DIR"
fi

if git -C "$WAYFIRE_SOURCE_DIR" apply --check "$PATCH_FILE" >/dev/null 2>&1; then
    git -C "$WAYFIRE_SOURCE_DIR" apply "$PATCH_FILE"
elif git -C "$WAYFIRE_SOURCE_DIR" apply --reverse --check "$PATCH_FILE" >/dev/null 2>&1; then
    printf '%s\n' 'Wayfire nested compatibility patch is already applied.'
else
    printf '%s\n' "ERROR: source checkout is not compatible with $PATCH_FILE" >&2
    exit 1
fi

if [ -f "$WAYFIRE_BUILD_DIR/build.ninja" ]; then
    meson setup "$WAYFIRE_BUILD_DIR" "$WAYFIRE_SOURCE_DIR" \
        --reconfigure \
        --prefix="$WAYFIRE_PREFIX" \
        -Duse_system_wlroots=enabled \
        -Duse_system_wfconfig=enabled \
        -Dxwayland=enabled \
        -Dtests=disabled
else
    meson setup "$WAYFIRE_BUILD_DIR" "$WAYFIRE_SOURCE_DIR" \
        --prefix="$WAYFIRE_PREFIX" \
        --buildtype=release \
        -Duse_system_wlroots=enabled \
        -Duse_system_wfconfig=enabled \
        -Dxwayland=enabled \
        -Dtests=disabled
fi

ninja -C "$WAYFIRE_BUILD_DIR"
if [ -n "$WAYFIRE_DESTDIR" ]; then
    mkdir -p "$WAYFIRE_DESTDIR"
    DESTDIR=$WAYFIRE_DESTDIR ninja -C "$WAYFIRE_BUILD_DIR" install
    installed_binary=$WAYFIRE_DESTDIR${WAYFIRE_PREFIX%/}/bin/wayfire
else
    ninja -C "$WAYFIRE_BUILD_DIR" install
    installed_binary=${WAYFIRE_PREFIX%/}/bin/wayfire
fi
[ -x "$installed_binary" ] || {
    printf '%s\n' "ERROR: installed Wayfire binary is missing: $installed_binary" >&2
    exit 1
}
installed_root=${installed_binary%/bin/wayfire}
printf '%s\n' "${WAYFIRE_PREFIX%/}" > "$installed_root/.northstar-compiled-prefix"
chmod 0444 "$installed_root/.northstar-compiled-prefix"

printf '%s\n' "Built nested Wayfire: $installed_binary"
printf '%s\n' 'Use it only with WLR_BACKENDS=x11 and WLR_RENDERER=pixman.'
