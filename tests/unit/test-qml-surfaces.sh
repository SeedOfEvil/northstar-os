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
contains src/shell/DesktopBackground.qml "anchors.right: parent.right"
contains src/shell/DesktopBackground.qml "mapToItem("
contains src/shell/DesktopBackground.qml "onDoubleClicked: desktopBackground.openDesktopEntry"
contains src/shell/DesktopBackground.qml 'text: "Refresh Desktop"'
contains src/shell/DesktopBackground.qml "Qt.Key_Return"
contains src/shell/DesktopBackground.qml "northstarLogoSource"
contains src/shell/DesktopBackground.qml 'GradientStop { position: 1.0; color: lunar.desktopBottom }'
contains src/shell/layershellsurface.cpp 'surface->setExclusiveZone(-1)'
contains src/shell/DockWindow.qml "northstarWindowController.windows"
contains src/shell/DockWindow.qml "toggleMinimize(modelData.viewId)"
contains src/shell/DockWindow.qml 'anchors.horizontalCenter: parent.horizontalCenter'
contains src/shell/DockWindow.qml 'Behavior on scale'
contains src/shell/DockWindow.qml 'text: "Trash"'
contains src/shell/DockWindow.qml 'lunar.dockGlass'
contains src/shell/WindowDragController.qml 'function clampedX(candidate)'
contains src/shell/WindowDragController.qml 'function prepareForOpen()'
contains src/shell/WindowDragController.qml 'property bool hasCustomPosition: false'
contains src/shell/QuickSettings.qml 'WindowDragController {'
contains src/shell/QuickSettings.qml 'quickSettingsDragHandle.mapToGlobal'
contains src/shell/SystemMenu.qml 'menuDragHandle.mapToGlobal'
contains src/shell/NotificationCenterWindow.qml 'notificationDragHandle.mapToGlobal'
contains src/shell/ApplicationOverview.qml 'overviewDragHandle.mapToGlobal'
contains src/shell/FileBrowserWindow.qml 'text: "New File"'
contains src/shell/FileBrowserWindow.qml 'text: "Delete"'
contains src/shell/FileBrowserWindow.qml 'title: "Open with an application"'
contains src/shell/FileBrowserWindow.qml 'text: "Show All"'
contains src/shell/FileBrowserWindow.qml 'setFilesGridView'
contains src/shell/FileBrowserWindow.qml 'id: searchDebounceTimer'
contains src/shell/FileBrowserWindow.qml 'interval: 220'
contains src/shell/FileBrowserWindow.qml 'searchDebounceTimer.restart()'
contains src/shell/FileBrowserWindow.qml 'function launchFilePath(path)'
contains src/shell/FileBrowserWindow.qml 'function openWithSearch(query)'
contains src/shell/FileBrowserWindow.qml 'function toggleMaximize()'
contains src/shell/FileBrowserWindow.qml 'function openDesktopEntry(path, isDirectory, isLaunchable)'
contains src/shell/FileBrowserWindow.qml 'files.launchFilePath(path)'
contains src/shell/FileBrowserWindow.qml 'compatibleApplications.length !== 1'
contains src/shell/FileBrowserWindow.qml 'associationDialog.showAllApplications = associationDialog.extension.length === 0'
contains src/shell/FileBrowserWindow.qml 'files.hide()'
contains apps/text-editor/TextEditorWindow.qml 'requestActivate()'
contains src/shell/SettingsWindow.qml 'text: "Reset Desktop Icon Layout"'
contains src/shell/SettingsWindow.qml 'text: "Restart Northstar Shell"'
contains src/shell/SettingsWindow.qml 'requestShellRestart()'
contains src/shell/SoftwareCenterWindow.qml 'function toggleMaximize()'
contains src/shell/ShellWindow.qml 'fileBrowserWindow.openWithSearch(query)'
contains src/shell/LunarPalette.qml 'readonly property color accentBright'
contains src/shell/SoftwareCenterWindow.qml 'Read-only inventory'
contains src/shell/SoftwareCenterWindow.qml 'Northstar application catalog'
contains src/shell/SoftwareCenterWindow.qml 'Search packages and applications...'
contains src/shell/SoftwareCenterWindow.qml 'Launching uses the same validated catalog'
contains src/shell/SoftwareCenterWindow.qml 'Review Update Plan'
contains src/shell/SoftwareCenterWindow.qml 'No changes have been made'
contains src/shell/SoftwareCenterWindow.qml 'Apply Update (protected)'
contains src/shell/SoftwareCenterWindow.qml 'updateAuthorization.refresh()'
contains src/shell/SystemMenu.qml 'Log Out of Northstar'
contains apps/welcome/WelcomeWindow.qml 'text: "Getting Started"'
contains apps/welcome/WelcomeWindow.qml 'qrc:/Northstar/Welcome/northstar-welcome.svg'
contains apps/welcome/WelcomeWindow.qml 'informational'
contains apps/welcome/WelcomeWindow.qml 'northstarSessionStatus'
contains apps/welcome/main.cpp 'northstarSessionStatus'
contains apps/welcome/main.cpp 'qrc:/Northstar/Welcome/WelcomeWindow.qml'
contains apps/text-editor/TextEditorWindow.qml 'qrc:/Northstar/TextEditor/northstar-text-editor.svg'
contains apps/text-editor/main.cpp 'qrc:/Northstar/TextEditor/TextEditorWindow.qml'
contains apps/text-editor/CMakeLists.txt 'QT_RESOURCE_ALIAS "northstar-text-editor.svg"'
contains apps/text-editor/TextEditorWindow.qml 'width: unsavedDialog.width - (2 * unsavedDialog.padding)'
contains apps/text-editor/TextEditorWindow.qml '"Save As..."'
contains apps/text-editor/TextEditorWindow.qml 'defaultSaveDirectory'
contains apps/text-editor/TextEditorWindow.qml 'saveAsDialog'

[ -r "$ROOT/apps/samples/NorthstarTextEditor.app/Contents/Info.plist" ] \
    || fail 'Text Editor bundle manifest is missing'
contains apps/samples/NorthstarTextEditor.app/Contents/Info.plist 'DocumentExtensions'

printf '%s\n' 'All Northstar QML surface contract checks passed.'
