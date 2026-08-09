#!/bin/sh

# Lightweight visual-surface contract checks. These do not claim to replace
# interactive noVNC acceptance; they catch accidental loss of the product
# wiring that makes the visual check meaningful.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)

fail()
{
    printf '%s\n' "FAIL: $*" >&2
    exit 1
}

contains()
{
    file=$1
    needle=$2
    grep -F -- "$needle" "$ROOT/$file" >/dev/null 2>&1 \
        || fail "$file is missing required surface contract: $needle"
}

required_files="\
src/shell/DesktopBackground.qml \
src/shell/DockWindow.qml \
src/shell/FileBrowserWindow.qml \
src/shell/SettingsWindow.qml \
src/shell/SoftwareCenterWindow.qml \
src/shell/SystemMenu.qml"

for file in $required_files; do
    [ -r "$ROOT/$file" ] || fail "missing QML surface: $file"
done

contains src/shell/DesktopBackground.qml "setPosition("
contains src/shell/DesktopBackground.qml "nearestFreePosition("
contains src/shell/DesktopBackground.qml "onDoubleClicked: desktopBackground.openDesktopEntry"
contains src/shell/DesktopBackground.qml 'text: "Refresh Desktop"'
contains src/shell/DesktopBackground.qml "Qt.Key_Return"
contains src/shell/DesktopBackground.qml "northstarLogoSource"
contains src/shell/DockWindow.qml "northstarWindowController.windows"
contains src/shell/DockWindow.qml "toggleMinimize(modelData.viewId)"
contains src/shell/DockWindow.qml 'text: "Trash"'
contains src/shell/FileBrowserWindow.qml 'text: "New File"'
contains src/shell/FileBrowserWindow.qml 'text: "Delete"'
contains src/shell/FileBrowserWindow.qml 'title: "Open with an application"'
contains src/shell/FileBrowserWindow.qml 'text: "Show All"'
contains src/shell/SettingsWindow.qml 'text: "Reset Desktop Icon Layout"'
contains src/shell/SoftwareCenterWindow.qml 'Read-only inventory'
contains src/shell/SystemMenu.qml 'Log Out of Northstar'
contains apps/welcome/WelcomeWindow.qml 'text: "Getting Started"'
contains apps/welcome/WelcomeWindow.qml 'informational'
contains apps/welcome/WelcomeWindow.qml 'northstarSessionStatus'
contains apps/welcome/main.cpp 'northstarSessionStatus'

[ -r "$ROOT/apps/samples/NorthstarTextEditor.app/Contents/Info.plist" ] \
    || fail 'Text Editor bundle manifest is missing'
contains apps/samples/NorthstarTextEditor.app/Contents/Info.plist 'DocumentExtensions'

printf '%s\n' 'All Northstar QML surface contract checks passed.'
