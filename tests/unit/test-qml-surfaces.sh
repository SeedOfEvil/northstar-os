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

# The panel is the only shell surface that can hold keyboard focus, so every
# transient surface must hand focus back when it closes. Without this the
# application ends up with no focused window and Ctrl+K stops reopening search.
contains src/shell/ShellWindow.qml 'function restoreShellFocus()'
contains src/shell/ShellWindow.qml 'onVisibleChanged: if (!visible) root.restoreShellFocus()'
contains src/shell/layershellsurface.cpp 'KeyboardInteractivityOnDemand'
contains src/shell/layershellsurface.cpp 'KeyboardInteractivityExclusive'
contains src/shell/layershellsurface.cpp 'restoreKeyboardFocus'
contains src/shell/ShellWindow.qml 'northstarShellFocus.restore()'
contains src/shell/main.cpp 'shellFocus.setPanelWindow(window)'
contains src/shell/main.cpp 'application.setQuitOnLastWindowClosed(false)'
contains src/shell/main.cpp 'QGuiApplication::screenAdded'
contains src/shell/main.cpp 'QGuiApplication::screenRemoved'
contains src/shell/main.cpp 'outputRefreshTimer.setInterval(750)'
contains src/shell/main.cpp 'Northstar surfaces rebuilt after output change'
contains src/shell/main.cpp 'runShellSelfTest'
contains src/shell/main.cpp '--qml-self-test'
contains src/shell/ShellWindow.qml 'function runShellCommand(command)'
contains src/shell/ShellWindow.qml 'function onCommandReceived(command)'
contains src/shell/main.cpp 'shellCommandServer.start()'
contains src/shell/shellcommandserver.cpp 'QLocalServer::UserAccessOption'
contains config/wayfire-nested.ini 'binding_search = <ctrl> KEY_K'
contains config/wayfire-nested.ini 'command_search = northstar-shell-command toggle-search'
contains config/wayfire-native.ini 'command_search = northstar-shell-command toggle-search'
if grep -Eq '^\[output:' "$ROOT/config/wayfire-native.ini"; then
    fail 'native Wayfire configuration hardcodes a physical output'
fi
# The compositor resolves the binding's command by name, so the session must
# put the installed prefix on PATH or the global shortcut silently does nothing.
contains src/session/northstar-session-x11 'session_bin_dir=$(dirname -- "$session_bin")'
contains src/session/northstar-session 'shell_bin_dir=$(dirname -- "$shell_path")'
contains src/shell/shellcommandserver.cpp 'resolveSocketPath'
contains src/shell/northstar-shell-command.cpp 'ShellCommandServer::resolveSocketPath()'
contains src/session/northstar-session-x11 'export PATH'
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
# A radio tile only offers to be pressed where this build can actually change it.
contains src/shell/QuickSettings.qml 'controller.setWifiEnabled('
contains src/shell/QuickSettings.qml 'controller.setBluetoothEnabled('
contains src/shell/QuickSettings.qml 'enabled: wifiTile.writable'
contains src/shell/QuickSettings.qml 'enabled: bluetoothTile.writable'
contains src/shell/QuickSettings.qml 'controller.setVolume(Math.round(value))'
contains src/shell/QuickSettings.qml 'controller.setDisplayBrightness(Math.round(value))'
contains src/shell/QuickSettings.qml 'controller.displayWritable'
contains src/shell/QuickSettings.qml 'powerController.refreshBattery()'
contains src/shell/QuickSettings.qml 'powerController.batteryStatus'
contains src/shell/QuickSettings.qml 'powerController.requestSuspend()'
contains src/shell/QuickSettings.qml 'powerController.suspendAvailable'
contains src/shell/QuickSettings.qml 'controller.setMuted(!quickSettings.controller.muted)'
contains src/shell/QuickSettings.qml 'controller.setSoundOutput(valueAt(currentIndex))'
contains src/shell/QuickSettings.qml 'controller.soundAvailable'
contains src/shell/QuickSettings.qml 'height: mixerControls.implicitHeight + 20'
contains src/shell/QuickSettings.qml 'height: Math.min(contentColumn.implicitHeight + 32,'
contains src/shell/QuickSettings.qml 'settingsWindow.openSettings()'
contains src/shell/ShellWindow.qml 'controller: northstarQuickSettingsController'
contains src/shell/ShellWindow.qml 'northstarPowerController.batteryPercentage'
contains src/shell/ShellWindow.qml 'powerController: northstarPowerController'
contains src/shell/ShellWindow.qml 'id: batteryIndicator'
contains src/shell/ShellWindow.qml '14 * northstarPowerController.batteryPercentage / 100'
contains src/shell/main.cpp 'QStringLiteral("Low battery")'
contains src/shell/SystemMenu.qml 'NativeWindowMoveHandler {'
contains src/shell/SystemMenu.qml 'menu.powerController.requestSuspend()'
contains src/shell/NotificationCenterWindow.qml 'NativeWindowMoveHandler {'
# History now survives a restart, so the panel has to show an age rather than
# the stored ISO timestamp, and keep the exact time reachable.
contains src/shell/NotificationCenterWindow.qml 'text: modelData.displayTime'
contains src/shell/NotificationCenterWindow.qml 'ToolTip.text: modelData.timestamp'
contains src/shell/ApplicationOverview.qml 'NativeWindowMoveHandler {'
contains src/shell/FileBrowserWindow.qml 'text: "New File"'
contains src/shell/FileBrowserWindow.qml 'text: "Delete"'
contains src/shell/FileBrowserWindow.qml 'title: "Open with an application"'
contains src/shell/FileBrowserWindow.qml 'text: "Show All"'
contains src/shell/FileBrowserWindow.qml 'setFilesGridView'
contains src/shell/FileBrowserWindow.qml 'id: searchDebounceTimer'
contains src/shell/FileBrowserWindow.qml 'id: tabBar'
contains src/shell/FileBrowserWindow.qml 'function newTab()'
contains src/shell/FileBrowserWindow.qml 'function activateTab(index)'
contains src/shell/FileBrowserWindow.qml 'id: locationField'
contains src/shell/FileBrowserWindow.qml 'sequences: [StandardKey.Copy]'
contains src/shell/FileBrowserWindow.qml 'fileBrowserController.pasteClipboard()'
contains src/shell/FileBrowserWindow.qml 'fileBrowserController.transferActive'
contains src/shell/FileBrowserWindow.qml 'activeTab.label'
contains src/shell/FileBrowserWindow.qml 'id: pasteConflictDialog'
contains src/shell/FileBrowserWindow.qml 'fileBrowserController.undoLastTransfer()'
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
# Settings v2 renders every control from the catalog, so the surface contract
# checks the catalog wiring and the declarations that back those controls.
contains src/shell/SettingsWindow.qml 'settings.settingsCatalog.setQuery(text)'
contains src/shell/SettingsWindow.qml 'settings.settingsCatalog.clearQuery()'
contains src/shell/SettingsWindow.qml 'settings.settingsCatalog.setSelectedSection(sectionId)'
contains src/shell/SettingsEntryControl.qml 'control.catalog.setValue(control.entry.id, value)'
contains src/shell/SettingsWindow.qml 'settings.settingsCatalog.invoke(entry.id)'
contains src/shell/SettingsWindow.qml 'model: settings.hasCatalog ? settings.settingsCatalog.entries : []'
contains src/shell/SettingsWindow.qml 'model: settings.hasCatalog ? settings.settingsCatalog.sections : []'
contains src/shell/SettingsWindow.qml 'ScrollBar.vertical: ScrollBar {'
contains src/shell/SettingsWindow.qml 'id: sectionList'
contains src/shell/SettingsWindow.qml 'active: sectionList.contentHeight > sectionList.height'
contains src/shell/SettingsWindow.qml 'width: sectionList.width - sectionScrollBar.width - 6'
contains src/shell/SettingsWindow.qml 'active: entryList.contentHeight > entryList.height'
contains src/shell/SettingsWindow.qml 'policy: ScrollBar.AsNeeded'
contains src/shell/SettingsWindow.qml 'id: statusArea'
contains src/shell/SettingsWindow.qml 'wrapMode: Text.WordWrap'
contains src/shell/SettingsWindow.qml 'Math.max(minimumSurfaceHeight, 720)'
contains src/shell/SettingsEntryControl.qml 'entry && entry.kind === "choice" ? 250'
contains src/shell/SettingsEntryControl.qml 'width: 250'
contains src/shell/main.cpp 'qrc:/Northstar/Shell/DisplayModeConfirmationWindow.qml'
contains src/shell/main.cpp 'displayConfirmationObject'
contains src/shell/main.cpp '&QuickSettingsController::displayModeApplied'
contains src/shell/main.cpp 'outputRefreshTimer.start()'
contains src/shell/DisplayModeConfirmationWindow.qml 'confirmation.controller.displayModePending'
contains src/shell/DisplayModeConfirmationWindow.qml 'confirmation.controller.displayModeSecondsRemaining'
contains src/shell/DisplayModeConfirmationWindow.qml 'confirmation.controller.previousDisplayMode'
contains src/shell/DisplayModeConfirmationWindow.qml 'confirmation.controller.currentDisplayMode'
contains src/shell/DisplayModeConfirmationWindow.qml 'confirmation.controller.revertDisplayMode()'
contains src/shell/DisplayModeConfirmationWindow.qml 'confirmation.controller.keepDisplayMode()'
contains src/shell/DisplayModeConfirmationWindow.qml 'modality: Qt.ApplicationModal'
contains src/shell/DisplayModeConfirmationWindow.qml 'confirmation.pendingAction = "revert"'
contains src/shell/DisplayModeConfirmationWindow.qml 'confirmation.pendingAction = "keep"'
contains src/shell/SettingsWindow.qml 'placeholderText: "Search settings"'
contains src/shell/SettingsWindow.qml 'text: modelData.unavailableReason'
contains src/shell/SettingsWindow.qml 'visible: settings.searching'
contains src/shell/SettingsWindow.qml 'id: confirmDialog'
contains src/shell/SettingsWindow.qml 'sequences: [StandardKey.Find]'
contains src/shell/ShellWindow.qml 'settingsCatalog: northstarSettingsCatalog'
contains src/shell/main.cpp 'northstarSettingsCatalog'
contains src/shell/main.cpp 'registerDesktopSettings(&settingsCatalog'
contains src/shell/desktopsettings.cpp 'session.restartshell'
contains src/shell/desktopsettings.cpp 'requestShellRestart()'
contains src/shell/desktopsettings.cpp 'desktop.resetlayout'
contains src/shell/desktopsettings.cpp 'sound.volume'
contains src/shell/desktopsettings.cpp 'sound.output'
contains src/shell/desktopsettings.cpp 'sound.mute'
contains src/shell/desktopsettings.cpp 'sound.balance'
contains src/shell/desktopsettings.cpp 'sound.test'
contains src/shell/desktopsettings.cpp 'testSoundAvailable()'
contains src/shell/desktopsettings.cpp 'notifications.donotdisturb'
contains src/shell/desktopsettings.cpp 'appearance.dark'
contains src/shell/settingscatalog.cpp 'unavailableReason'
contains src/shell/SettingsWindow.qml 'settings.startSystemMove()'
contains src/shell/SettingsWindow.qml 'objectName: "settingsWindow"'
contains src/shell/main.cpp 'surface->findChild<QObject *>(QStringLiteral("settingsWindow"))'
contains src/shell/main.cpp 'QMetaObject::invokeMethod(settingsWindow, "openSettings"'
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
contains src/shell/SoftwareCenterWindow.qml 'Installed inventory and the pinned FreeBSD package catalogue'
contains src/shell/SoftwareCenterWindow.qml 'Northstar application catalog'
contains src/shell/SoftwareCenterWindow.qml 'Search packages and applications...'
contains src/shell/SoftwareCenterWindow.qml 'Launching uses the same validated catalog'
contains src/shell/SoftwareCenterWindow.qml 'Review Update Plan'
contains src/shell/SoftwareCenterWindow.qml 'Review Install'
contains src/shell/SoftwareCenterWindow.qml 'Review Removal'
contains src/shell/SoftwareCenterWindow.qml 'software.packageMutation.planInstall(software.selectedPackage)'
contains src/shell/SoftwareCenterWindow.qml 'software.packageMutation.planRemove(software.selectedPackage)'
contains src/shell/SoftwareCenterWindow.qml 'software.packageMutation.applyPlan()'
contains src/shell/SoftwareCenterWindow.qml 'A ZFS boot environment is created before the package database changes.'
contains src/shell/SoftwareCenterWindow.qml 'No changes have been made yet'
contains src/shell/packagemutationcontroller.cpp 'QStringLiteral("-n"),'
contains src/shell/packagemutationcontroller.cpp 'QStringLiteral("-y"),'
contains src/shell/packagemutationcontroller.cpp 'QStringLiteral("REPO_AUTOUPDATE=false"),'
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

# Text Editor v2: document tabs, recent history, find/replace, and the explicit
# save/discard/cancel protection that makes closing a dirty document safe.
contains apps/text-editor/TextEditorWindow.qml 'model: editor.documentController ? editor.documentController.documents : []'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.activateDocument(index)'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.newDocument()'
contains apps/text-editor/TextEditorWindow.qml 'editor.requestCloseTab(index)'
contains apps/text-editor/TextEditorWindow.qml 'visible: modelData.dirty'
contains apps/text-editor/TextEditorWindow.qml 'id: recentPopup'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.recentFiles'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.openRecent(index)'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.clearRecentFiles()'
contains apps/text-editor/TextEditorWindow.qml 'id: openDialog'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.browseEntries'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.browseUp()'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.browseHome()'
contains apps/text-editor/TextEditorWindow.qml 'id: findBar'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.findNext()'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.findPrevious()'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.replaceCurrent()'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.replaceAll()'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.setFindCaseSensitive(checked)'
contains apps/text-editor/TextEditorWindow.qml 'function onSelectionRequested(anchor, cursor)'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.setCursorPosition(cursorPosition)'
contains apps/text-editor/TextEditorWindow.qml 'text: "Discard"'
contains apps/text-editor/TextEditorWindow.qml 'text: "Cancel"'
contains apps/text-editor/TextEditorWindow.qml 'id: externalChangeDialog'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.saveOverwritingExternalChanges()'
contains apps/text-editor/TextEditorWindow.qml 'editor.documentController.reloadDocument()'
contains apps/text-editor/TextEditorWindow.qml 'sequences: [StandardKey.Find]'
contains apps/text-editor/TextEditorWindow.qml 'sequences: [StandardKey.Save]'
contains apps/text-editor/TextEditorWindow.qml 'sequence: "Ctrl+Tab"'
contains apps/text-editor/TextEditorWindow.qml 'readonly property int fieldWidth: Math.max('
contains apps/text-editor/main.cpp 'controller.openFile(argument)'
contains apps/text-editor/texteditorcontroller.h 'Q_INVOKABLE bool saveOverwritingExternalChanges();'
contains apps/text-editor/recentfilesstore.cpp 'QFile::ReadOwner | QFile::WriteOwner'
contains apps/first-boot/FirstBootWindow.qml 'Welcome to Northstar'
contains apps/first-boot/FirstBootWindow.qml 'firstBootController.validateProfile('
contains apps/first-boot/FirstBootWindow.qml 'firstBootController.provision('
contains apps/first-boot/FirstBootWindow.qml 'echoMode: TextInput.Password'
contains apps/first-boot/FirstBootWindow.qml 'function onSecretsCleared()'
contains apps/first-boot/FirstBootWindow.qml 'import Northstar.Ui 1.0'
contains packaging/polkit/org.northstar.firstboot.policy '<allow_active>yes</allow_active>'
contains packaging/polkit/org.northstar.firstboot.policy '/usr/local/libexec/northstar-first-boot-provision'
contains packaging/polkit/org.northstar.power.policy '<allow_active>yes</allow_active>'
contains packaging/polkit/org.northstar.power.policy '/usr/local/bin/northstar-power'
contains apps/first-boot/northstar-first-boot-session 'first-boot.pending'
contains apps/first-boot/northstar-first-boot-session 'xauth -f "$candidate" list "$DISPLAY"'
contains apps/first-boot/northstar-first-boot-session 'export QT_QPA_PLATFORM=xcb'
contains apps/first-boot/northstar-first-boot-session 'exec "$WIZARD"'
contains apps/first-boot/FirstBootWindow.qml 'height: Screen.height'
contains apps/first-boot/FirstBootWindow.qml 'width: Screen.width'
contains apps/first-boot/FirstBootWindow.qml 'Layout.maximumWidth: 280'
contains apps/first-boot/FirstBootWindow.qml 'Component.onCompleted: primaryButton.forceActiveFocus()'
contains apps/installer/InstallerWindow.qml 'Select a destination'
contains apps/installer/InstallerWindow.qml 'height: Screen.height'
contains apps/installer/InstallerWindow.qml 'width: Screen.width'
contains apps/installer/InstallerWindow.qml 'Layout.fillWidth: false'
contains apps/installer/InstallerWindow.qml 'Layout.maximumWidth: 245'
contains apps/installer/InstallerWindow.qml 'Layout.minimumWidth: 600'
contains apps/installer/InstallerWindow.qml 'Type " + installerController.selectedDevice'
contains apps/installer/InstallerWindow.qml 'permanently erased'
contains apps/installer/InstallerWindow.qml 'installerController.confirmationReady'
contains apps/installer/InstallerWindow.qml 'Install Northstar'
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

# Every Lunar palette token a surface references must exist in LunarPalette.
# A missing one silently evaluates to [undefined] and the control renders with
# no colour at all, which is how the unified-search selection highlight was
# lost without any surface check noticing.
palette_definitions=$(grep -oE 'property (color|int|bool) [a-zA-Z][a-zA-Z0-9]*' \
    "$ROOT/src/ui/LunarPalette.qml" | awk '{print $3}' | sort -u)
[ -n "$palette_definitions" ] || fail 'could not read any LunarPalette token definitions'

palette_references=$(grep -rhoE '\b(lunar|lunarPalette)\.[a-zA-Z][a-zA-Z0-9]*' \
    "$ROOT/src" "$ROOT/apps" --include=*.qml | sed 's/^[a-zA-Z]*\.//' | sort -u)
[ -n "$palette_references" ] || fail 'could not read any LunarPalette token references'

for palette_token in $palette_references; do
    printf '%s\n' "$palette_definitions" | grep -qx "$palette_token" \
        || fail "LunarPalette defines no '$palette_token', but a QML surface uses it"
done

# A StandardKey often maps to several platform key sequences. Binding with
# `sequence:` registers only the first and leaves the alternates dead, so every
# StandardKey shortcut must use the list form.
if grep -rn 'sequence: StandardKey\.' "$ROOT/src" "$ROOT/apps" --include=*.qml >/dev/null 2>&1; then
    grep -rn 'sequence: StandardKey\.' "$ROOT/src" "$ROOT/apps" --include=*.qml >&2
    fail 'use sequences: [StandardKey.X] so every platform binding is registered'
fi

printf '%s\n' 'All Northstar QML surface contract checks passed.'
