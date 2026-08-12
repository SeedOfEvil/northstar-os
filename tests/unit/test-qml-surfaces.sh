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
src/ui/LunarPalette.qml \
src/ui/NativeWindowMoveHandler.qml \
src/ui/NativeWindowResizeHandler.qml \
src/ui/NorthstarWindowControls.qml \
src/ui/NorthstarWindowFrame.qml \
src/ui/NorthstarWindowTitleBar.qml \
src/shell/DesktopBackground.qml \
src/shell/DockWindow.qml \
src/shell/FileBrowserWindow.qml \
src/shell/SettingsWindow.qml \
src/shell/SoftwareCenterWindow.qml \
src/shell/SystemMenu.qml"
required_files="$required_files src/shell/SearchOverlay.qml"
required_files="$required_files src/shell/QuickLookWindow.qml"
required_files="$required_files apps/first-boot/FirstBootWindow.qml"
required_files="$required_files apps/installer/InstallerWindow.qml"

for file in $required_files; do
    [ -r "$ROOT/$file" ] || fail "missing QML surface: $file"
done

contains src/shell/DesktopBackground.qml "setPosition("
contains src/shell/DesktopBackground.qml "nearestFreePosition("
contains src/shell/DesktopBackground.qml "restoreSavedDesktopPositions()"
contains src/shell/DesktopBackground.qml "onWidthChanged: desktopPositionRestoreTimer.restart()"
contains src/shell/DesktopBackground.qml "onHeightChanged: desktopPositionRestoreTimer.restart()"
contains src/shell/DesktopBackground.qml "anchors.right: parent.right"
contains src/shell/DesktopBackground.qml "mapToItem("
contains src/shell/DesktopBackground.qml "onDoubleClicked: desktopBackground.openDesktopEntry"
contains src/shell/DesktopBackground.qml 'text: "Refresh Desktop"'
contains src/shell/DesktopBackground.qml "Qt.Key_Return"
contains src/shell/DesktopBackground.qml "northstarLogoSource"
contains src/shell/DesktopBackground.qml 'GradientStop { position: 1.0; color: lunar.desktopBottom }'
contains src/shell/layershellsurface.cpp 'surface->setExclusiveZone(-1)'
contains src/shell/DockWindow.qml "northstarWindowController.applicationGroups"
contains src/shell/DockWindow.qml "pinnedApplicationModel.movePinned"
contains src/shell/DockWindow.qml '"Unpin from Dock"'
contains src/shell/DockWindow.qml 'dockAppMenu.y = -dockAppMenu.implicitHeight'
contains src/shell/DockWindow.qml 'applicationIconSize'
contains src/shell/DockWindow.qml 'applicationIconVerticalOffset'
contains src/shell/DockWindow.qml 'dockContent.implicitWidth + 20'
contains src/shell/ApplicationOverview.qml '"Pin to Dock"'
contains src/shell/ApplicationOverview.qml 'pinnedApplications.isPinned'
contains src/shell/ApplicationOverview.qml 'contentWidth: pinnedShortcutsRow.width'
contains src/shell/DockWindow.qml 'anchors.leftMargin: 10'
contains src/shell/DockWindow.qml 'Behavior on scale'
contains src/shell/DockWindow.qml 'text: "Trash"'
contains src/shell/DockWindow.qml 'lunar.dockGlass'
contains src/shell/WindowDragController.qml 'function clampedX(candidate)'
contains src/shell/WindowDragController.qml 'function prepareForOpen()'
contains src/shell/WindowDragController.qml 'property bool hasCustomPosition: false'
contains src/shell/WindowDragController.qml 'window.startSystemMove()'
contains src/shell/QuickSettings.qml 'WindowDragController {'
contains src/ui/NativeWindowMoveHandler.qml 'DragHandler {'
contains src/ui/NativeWindowMoveHandler.qml 'window.startSystemMove()'
contains src/ui/NativeWindowMoveHandler.qml 'PointerHandler.CanTakeOverFromAnything'
contains src/ui/NativeWindowResizeHandler.qml 'window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)'
contains src/ui/NorthstarWindowControls.qml 'showMinimized()'
contains src/ui/NorthstarWindowControls.qml 'closeDestroysWindow'
contains src/ui/NorthstarWindowTitleBar.qml 'NorthstarWindowControls {'
contains src/shell/QuickSettings.qml 'NativeWindowMoveHandler {'
contains src/shell/QuickSettings.qml 'controller.refresh()'
contains src/shell/QuickSettings.qml 'controller.toggleDoNotDisturb()'
contains src/shell/QuickSettings.qml 'controller.setVolume(Math.round(value))'
contains src/shell/QuickSettings.qml 'controller.soundAvailable'
contains src/shell/QuickSettings.qml 'settingsWindow.openSettings()'
contains src/shell/ShellWindow.qml 'controller: northstarQuickSettingsController'
contains src/shell/SystemMenu.qml 'NativeWindowMoveHandler {'
contains src/shell/NotificationCenterWindow.qml 'NativeWindowMoveHandler {'
contains src/shell/ApplicationOverview.qml 'NativeWindowMoveHandler {'
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
contains src/shell/FileBrowserWindow.qml 'function presentWindow()'
contains src/shell/FileBrowserWindow.qml 'files.showNormal()'
contains src/shell/FileBrowserWindow.qml 'function toggleMaximize()'
contains src/shell/FileBrowserWindow.qml 'files.startSystemMove()'
contains src/shell/FileBrowserWindow.qml 'NativeWindowMoveHandler {'
contains src/shell/FileBrowserWindow.qml 'function openDesktopEntry(path, isDirectory, isLaunchable)'
contains src/shell/FileBrowserWindow.qml 'files.launchFilePath(path)'
contains src/shell/FileBrowserWindow.qml 'compatibleApplications.length !== 1'
contains src/shell/FileBrowserWindow.qml 'associationDialog.showAllApplications = associationDialog.extension.length === 0'
contains src/shell/FileBrowserWindow.qml 'files.hide()'
contains apps/text-editor/TextEditorWindow.qml 'requestActivate()'
contains src/shell/SettingsWindow.qml 'text: "Reset Desktop Icon Layout"'
contains src/shell/SettingsWindow.qml 'text: "Restart Northstar Shell"'
contains src/shell/SettingsWindow.qml 'requestShellRestart()'
contains src/shell/SettingsWindow.qml 'settings.startSystemMove()'
contains src/shell/SettingsWindow.qml 'NativeWindowMoveHandler {'
contains src/shell/SoftwareCenterWindow.qml 'function toggleMaximize()'
contains src/shell/SoftwareCenterWindow.qml 'software.startSystemMove()'
contains src/shell/SoftwareCenterWindow.qml 'NativeWindowMoveHandler {'
contains src/shell/ShellWindow.qml 'sequence: "Ctrl+K"'
contains src/shell/ShellWindow.qml 'searchOverlay.openSearch("")'
contains src/shell/ShellWindow.qml 'northstarSearchController'
contains src/shell/SearchOverlay.qml 'placeholderText: "Search apps, files, and actions"'
contains src/shell/SearchOverlay.qml 'controller.activateResult(index)'
contains src/shell/SearchOverlay.qml 'Qt.Key_Down'
contains src/shell/SearchOverlay.qml 'Qt.Key_Escape'
contains src/shell/QuickLookWindow.qml 'objectName: "quickLookWindow"'
contains src/shell/QuickLookWindow.qml 'function presentPath(path, navigationRoot, originWindow)'
contains src/shell/QuickLookWindow.qml 'previewController.previewPath(path, navigationRoot || "")'
contains src/shell/QuickLookWindow.qml 'quickLook.showNormal()'
contains src/shell/QuickLookWindow.qml 'quickLook.showMaximized()'
contains src/shell/QuickLookWindow.qml 'visibility === Window.Maximized'
contains src/shell/QuickLookWindow.qml 'function restoreOriginFocus()'
contains src/shell/QuickLookWindow.qml 'origin.restorePreviewFocus()'
contains src/shell/QuickLookWindow.qml 'previewController.kind === "text"'
contains src/shell/QuickLookWindow.qml 'previewController.kind === "image"'
contains src/shell/QuickLookWindow.qml 'previewController.kind === "folder"'
contains src/shell/QuickLookWindow.qml 'NorthstarWindowTitleBar'
contains src/shell/QuickLookWindow.qml 'NativeWindowResizeHandler'
contains src/shell/FileBrowserWindow.qml 'function previewSelectedEntry()'
contains src/shell/FileBrowserWindow.qml 'function restorePreviewFocus()'
contains src/shell/FileBrowserWindow.qml 'event.key === Qt.Key_Space'
contains src/shell/DesktopBackground.qml 'function previewEntry(entry)'
contains src/shell/DesktopBackground.qml 'function restorePreviewFocus()'
contains src/shell/DesktopBackground.qml 'event.key === Qt.Key_Space'
contains src/shell/searchcontroller.cpp 'MaximumScannedEntries = 20000'
contains src/shell/searchcontroller.cpp 'std::make_shared<std::atomic_bool>'
contains src/ui/LunarPalette.qml 'readonly property color accentBright'
contains src/shell/SoftwareCenterWindow.qml 'Read-only inventory'
contains src/shell/SoftwareCenterWindow.qml 'Northstar application catalog'
contains src/shell/SoftwareCenterWindow.qml 'Search packages and applications...'
contains src/shell/SoftwareCenterWindow.qml 'Launching uses the same validated catalog'
contains src/shell/SoftwareCenterWindow.qml 'Review Update Plan'
contains src/shell/SoftwareCenterWindow.qml 'No changes have been made yet'
contains src/shell/SoftwareCenterWindow.qml 'Apply Verified Update'
contains src/shell/SoftwareCenterWindow.qml 'Schedule Rollback'
contains src/shell/SoftwareCenterWindow.qml 'software.updateAuthorization.applyUpdate()'
contains src/shell/SoftwareCenterWindow.qml 'id: updateStatusView'
contains src/shell/SoftwareCenterWindow.qml 'Protected transaction completed successfully'
contains src/shell/SoftwareCenterWindow.qml 'software.authorizationStatusText'
contains src/shell/packagetrustcontroller.cpp 'GenericConfigLocation'
contains src/shell/packagetrustcontroller.cpp '/northstar/repository-policy.conf'
contains src/shell/updateplancontroller.cpp '/northstar/repository-metadata.json'
contains src/shell/SoftwareCenterWindow.qml 'updateAuthorization.refresh()'
contains src/shell/SoftwareCenterWindow.qml 'software.updatePlan.publicationManifestSha256'
contains src/shell/SoftwareCenterWindow.qml 'software.updatePlan.packageProvenance'
contains src/shell/SoftwareCenterWindow.qml 'text: "Verified channel"'
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
contains apps/text-editor/TextEditorWindow.qml 'anchors.right: parent.right'
contains apps/first-boot/FirstBootWindow.qml 'Welcome to Northstar'
contains apps/first-boot/FirstBootWindow.qml 'firstBootController.validateProfile('
contains apps/first-boot/FirstBootWindow.qml 'firstBootController.provision('
contains apps/first-boot/FirstBootWindow.qml 'echoMode: TextInput.Password'
contains apps/first-boot/FirstBootWindow.qml 'function onSecretsCleared()'
contains apps/first-boot/FirstBootWindow.qml 'import Northstar.Ui 1.0'
contains packaging/polkit/org.northstar.firstboot.policy '<allow_active>yes</allow_active>'
contains packaging/polkit/org.northstar.firstboot.policy '/usr/local/libexec/northstar-first-boot-provision'
contains apps/first-boot/northstar-first-boot-session 'first-boot.pending'
contains apps/first-boot/northstar-first-boot-session 'exec "$WIZARD"'
contains apps/installer/InstallerWindow.qml 'Select a destination'
contains apps/installer/InstallerWindow.qml 'Type " + installerController.selectedDevice'
contains apps/installer/InstallerWindow.qml 'permanently erased'
contains apps/installer/InstallerWindow.qml 'installerController.confirmationReady'
contains apps/installer/InstallerWindow.qml 'Awaiting Release Media'
contains apps/installer/InstallerWindow.qml 'Export Diagnostics'
contains apps/installer/InstallerWindow.qml 'Prepare Clean Retry'
contains apps/installer/InstallerWindow.qml 'installerRecoveryController.retryConfirmationReady'
contains apps/installer/CMakeLists.txt 'northstar-installer-source-verify'
contains apps/installer/northstar-installer-source-verify '/var/run/northstar-installer/source'
contains apps/installer/northstar-installer-source-verify '/usr/local/share/northstar/installer/source-signing.pem'
contains apps/installer/northstar-installer-engine 'SOURCE_VERIFY=/usr/local/libexec/northstar-installer-source-verify'
contains apps/installer/northstar-installer-engine 'execution=guarded-executor-only'
contains apps/installer/northstar-installer-executor '/etc/northstar/installer-execution.conf'
contains apps/installer/northstar-installer-executor '--confirm-device DEVICE'
contains apps/installer/northstar-installer-executor 'installer source media must be mounted read-only'
contains apps/installer/northstar-installer-executor '--prepare-retry TRANSACTION_ID --confirm-device DEVICE'
contains apps/installer/northstar-installer-executor 'PRIVATE_DATA=excluded'
contains apps/installer/northstar-installer-recovery '--prepare-retry TRANSACTION_ID --confirm-device DEVICE'
contains packaging/polkit/org.northstar.installer.policy '/usr/local/libexec/northstar-installer-engine'
contains packaging/polkit/org.northstar.installer.policy '/usr/local/libexec/northstar-installer-executor'
contains packaging/polkit/org.northstar.installer.policy '/usr/local/libexec/northstar-installer-recovery'
contains packaging/polkit/org.northstar.installer.policy '<allow_active>auth_admin</allow_active>'
contains apps/recovery/RecoveryWindow.qml 'NorthstarWindowTitleBar {'
contains apps/recovery/RecoveryWindow.qml 'NativeWindowResizeHandler {'
contains apps/recovery/RecoveryWindow.qml 'bootEnvironmentController.refresh()'
contains apps/recovery/RecoveryWindow.qml 'if (!recoverySelfTest)'
contains apps/recovery/RecoveryWindow.qml 'bootEnvironmentController.scheduleActivation()'
contains apps/recovery/RecoveryWindow.qml 'Use Recovery Point...'
contains apps/recovery/northstar-boot-environment '--activate NAME --confirm NAME'
contains apps/recovery/northstar-boot-environment 'destructive-operations=none'
contains packaging/polkit/org.northstar.recovery.policy '/usr/local/libexec/northstar-boot-environment'
contains packaging/polkit/org.northstar.recovery.policy '<allow_active>auth_admin</allow_active>'
[ -r "$ROOT/apps/samples/NorthstarRecovery.app/Contents/Info.plist" ] \
    || fail 'Northstar Recovery bundle manifest is missing'
contains apps/samples/NorthstarRecovery.app/Contents/Info.plist 'org.northstar.Recovery'

for surface in \
    src/shell/FileBrowserWindow.qml \
    src/shell/SettingsWindow.qml \
    src/shell/SoftwareCenterWindow.qml \
    apps/welcome/WelcomeWindow.qml \
    apps/text-editor/TextEditorWindow.qml; do
    contains "$surface" 'import Northstar.Ui 1.0'
    contains "$surface" 'NorthstarWindowFrame {'
    contains "$surface" 'NorthstarWindowTitleBar {'
    contains "$surface" 'NativeWindowResizeHandler {'
done

[ -r "$ROOT/apps/samples/NorthstarTextEditor.app/Contents/Info.plist" ] \
    || fail 'Text Editor bundle manifest is missing'
contains apps/samples/NorthstarTextEditor.app/Contents/Info.plist 'DocumentExtensions'

printf '%s\n' 'All Northstar QML surface contract checks passed.'
