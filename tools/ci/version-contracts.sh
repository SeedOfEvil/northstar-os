#!/bin/sh

# Enforce VERSION as Northstar's only product release identity.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
VERSION_FILE=$ROOT/VERSION

die() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

[ -f "$VERSION_FILE" ] && [ ! -L "$VERSION_FILE" ] \
    || die 'VERSION must be a regular non-symlink file'
[ "$(wc -l < "$VERSION_FILE" | tr -d ' ')" -eq 1 ] \
    || die 'VERSION must contain exactly one line'
NORTHSTAR_VERSION=$(sed -n '1p' "$VERSION_FILE")
printf '%s\n' "$NORTHSTAR_VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' \
    || die 'VERSION must contain exactly one semantic version (X.Y.Z)'

grep -F 'file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" NORTHSTAR_VERSION)' \
    "$ROOT/CMakeLists.txt" >/dev/null \
    || die 'CMake does not read the canonical VERSION file'
grep -F 'add_compile_definitions(NORTHSTAR_VERSION_STRING="${PROJECT_VERSION}")' \
    "$ROOT/CMakeLists.txt" >/dev/null \
    || die 'C++ applications do not inherit the canonical project version'
grep -F 'northstar-$(NORTHSTAR_VERSION)-amd64.pkg' "$ROOT/Makefile" >/dev/null \
    || die 'the default image package path does not derive from VERSION'

for source in \
    apps/installer/main.cpp \
    apps/recovery/main.cpp \
    apps/first-boot/main.cpp \
    apps/welcome/main.cpp \
    apps/text-editor/main.cpp \
    src/shell/main.cpp \
    src/shell/northstar-shell-command.cpp; do
    grep -F 'setApplicationVersion(QStringLiteral(NORTHSTAR_VERSION_STRING))' \
        "$ROOT/$source" >/dev/null \
        || die "$source does not use the canonical Northstar version"
done

for manifest in \
    apps/samples/NorthstarWelcome.app/Contents/Info.plist.in \
    apps/samples/NorthstarTextEditor.app/Contents/Info.plist.in \
    apps/samples/NorthstarRecovery.app/Contents/Info.plist.in; do
    [ -f "$ROOT/$manifest" ] || die "bundle manifest template is missing: $manifest"
    grep -F '<string>@NORTHSTAR_VERSION@</string>' "$ROOT/$manifest" >/dev/null \
        || die "$manifest does not use the canonical Northstar version"
done

LOCK=$ROOT/image/manifests/northstar-15.1-amd64-qcow2.lock
LOCK_VERSION=$(sed -n 's/^NORTHSTAR_PACKAGE_VERSION=//p' "$LOCK")
LOCK_PACKAGE=$(sed -n 's/^NORTHSTAR_PACKAGE=//p' "$LOCK")
[ "$LOCK_VERSION" = "$NORTHSTAR_VERSION" ] \
    || die 'the accepted image lock package version does not match VERSION'
[ "$LOCK_PACKAGE" = "northstar-$NORTHSTAR_VERSION-amd64.pkg" ] \
    || die 'the accepted image lock package filename does not match VERSION'

grep -F -- '--version-file FILE' "$ROOT/tools/publish-development-repository.sh" >/dev/null \
    || die 'repository publication is not bound to VERSION'
grep -F 'image lock source revision does not match project commit' \
    "$ROOT/image/scripts/prepare-image-inputs.sh" >/dev/null \
    || die 'image staging is not bound to the selected source revision'

printf 'PASS: Northstar release identity is consistently derived from VERSION=%s\n' \
    "$NORTHSTAR_VERSION"
