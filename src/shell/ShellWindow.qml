import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: root

    LunarPalette {
        id: lunar
        darkMode: shellState.darkMode
    }

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool
    height: 44
    width: 1280
    title: "Northstar Shell"

    property color panelBackground: lunar.panelStrong
    property color panelForeground: lunar.foreground
    property color panelMuted: lunar.muted
    property color panelAccent: lunar.accent
    property date now: new Date()

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: root.now = new Date()
    }

    Timer {
        id: launchNotificationTimer
        interval: 3500
        onTriggered: launcher.clearLaunchMessage()
    }

    Connections {
        target: launcher

        function onLaunchStatusChanged() {
            if (launcher.launchMessage.length > 0) {
                launchNotificationTimer.restart()
            }
        }
    }

    Connections {
        target: northstarDesktopItemsController

        function onOpenPathRequested(path, isDirectory, isLaunchable) {
            fileBrowserWindow.openPath(path, isDirectory, isLaunchable)
        }

        function onOpenWithRequested(path) {
            fileBrowserWindow.openAssociationForPath(path)
        }
    }

    // The desktop background and dock are keyboard-inert and the panel is
    // KeyboardInteractivityOnDemand, so the panel is the only shell surface that
    // can hold keyboard focus. A transient surface such as unified search is a
    // normal toplevel: when it closes, the compositor has nothing to hand focus
    // back to, and every Qt.ApplicationShortcut stops receiving keys until the
    // user clicks the panel. Ask for focus back explicitly instead.
    function restoreShellFocus() {
        if (displayIndex !== 0) {
            return
        }
        // requestActivate() alone is ignored for an on-demand layer surface,
        // so the panel asks the compositor for focus through layer-shell.
        if (typeof northstarShellFocus !== "undefined" && northstarShellFocus) {
            northstarShellFocus.restore()
            return
        }
        root.requestActivate()
    }

    function closeTransientSurfaces() {
        if (searchOverlay.visible) {
            searchOverlay.closeSearch()
        } else if (systemMenu.visible) {
            systemMenu.closeMenu()
        } else if (applicationOverview.visible) {
            applicationOverview.hide()
        } else if (quickSettingsWindow.visible) {
            quickSettingsWindow.hide()
        } else if (notificationCenterWindow.visible) {
            notificationCenterWindow.hide()
        } else if (quickLookWindow.visible) {
            quickLookWindow.hide()
        } else if (softwareCenterWindow.visible) {
            softwareCenterWindow.hide()
        } else if (fileBrowserWindow.visible) {
            fileBrowserWindow.hide()
        } else if (settingsWindow.visible) {
            settingsWindow.hide()
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0
        sequence: "Ctrl+K"
        onActivated: searchOverlay.openSearch("")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("applications") : ""
        onActivated: systemMenu.triggerAction("applications")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("files") : ""
        onActivated: systemMenu.triggerAction("files")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("software") : ""
        onActivated: systemMenu.triggerAction("software")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("settings") : ""
        onActivated: systemMenu.triggerAction("settings")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("terminal") : ""
        onActivated: systemMenu.triggerAction("terminal")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("browser") : ""
        onActivated: systemMenu.triggerAction("browser")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("refresh") : ""
        onActivated: systemMenu.triggerAction("refresh")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: searchOverlay.visible || systemMenu.visible || applicationOverview.visible || quickSettingsWindow.visible
            || notificationCenterWindow.visible || softwareCenterWindow.visible
            || fileBrowserWindow.visible || settingsWindow.visible
        sequence: "Escape"
        onActivated: root.closeTransientSurfaces()
    }

    Rectangle {
        anchors.fill: parent
        color: root.panelBackground
        border.color: lunar.borderSoft
        border.width: 1

        gradient: Gradient {
            GradientStop { position: 0.0; color: lunar.panelStrong }
            GradientStop { position: 1.0; color: lunar.panel }
        }

        Rectangle {
            id: topBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 44
            color: "transparent"

            Row {
                id: brandNavigation
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 7

                Rectangle {
                    color: systemMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: 32
                    radius: 10
                    width: 34

                    Image {
                        anchors.centerIn: parent
                        fillMode: Image.PreserveAspectFit
                        height: 24
                        mipmap: true
                        smooth: true
                        source: northstarLogoSource
                        sourceClipRect: Qt.rect(270, 245, 485, 335)
                        width: 24
                    }

                    MouseArea {
                        id: systemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: systemMenu.openMenu()
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.panelForeground
                    font.bold: true
                    font.pixelSize: 14
                    text: "NorthStar"
                }

                Rectangle {
                    color: "transparent"
                    height: 32
                    width: 56

                    Text {
                        anchors.centerIn: parent
                        color: root.panelMuted
                        font.pixelSize: 11
                        text: "Desktop"
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: systemMenu.closeMenu()
                    }
                }

                Rectangle {
                    color: filesNavMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: 32
                    radius: 9
                    width: 58

                    Row {
                        anchors.centerIn: parent
                        spacing: 3

                        NorthstarIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            height: 20
                            width: 20
                            iconName: "files"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: root.panelMuted
                            font.pixelSize: 11
                            text: "Files"
                        }
                    }

                    MouseArea {
                        id: filesNavMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: fileBrowserWindow.openBrowser()
                    }
                }

                Rectangle {
                    color: appsNavMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: 32
                    radius: 9
                    width: 58

                    Row {
                        anchors.centerIn: parent
                        spacing: 3

                        NorthstarIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            height: 20
                            width: 20
                            iconName: "applications"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: root.panelMuted
                            font.pixelSize: 11
                            text: "Apps"
                        }
                    }

                    MouseArea {
                        id: appsNavMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: applicationOverview.openWithQuery("")
                    }
                }
            }

            Rectangle {
                id: globalSearchSurface
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                color: lunar.field
                border.color: globalSearchMouse.containsMouse ? lunar.accent : lunar.borderSoft
                border.width: 1
                height: 32
                radius: 12
                width: Math.min(360, root.width - 560)

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 34
                    anchors.right: parent.right
                    anchors.rightMargin: 58
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.panelMuted
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: "Search apps, files, and actions"
                }

                NorthstarIcon {
                    anchors.left: parent.left
                    anchors.leftMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                    height: 18
                    iconName: "search"
                    width: 18
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.panelMuted
                    font.pixelSize: 10
                    text: "⌘K"
                }

                MouseArea {
                    id: globalSearchMouse
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: searchOverlay.openSearch("")
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 5

                Rectangle {
                    color: notificationMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: 30
                    radius: 9
                    width: 34

                    NorthstarIcon {
                        anchors.centerIn: parent
                        height: 22
                        width: 22
                        iconName: "notifications"
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                    color: lunar.danger
                        height: 12
                        radius: 6
                        visible: northstarNotificationCenter && northstarNotificationCenter.unreadCount > 0
                        width: 12
                    }

                    MouseArea {
                        id: notificationMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: notificationCenterWindow.togglePanel()
                    }
                }

                Rectangle {
                    color: quickSettingsMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: 30
                    radius: 9
                    width: 34

                    NorthstarIcon {
                        anchors.centerIn: parent
                        height: 22
                        width: 22
                        iconName: "quick-settings"
                    }

                    MouseArea {
                        id: quickSettingsMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: quickSettingsWindow.togglePanel()
                    }
                }

                Text {
                    color: root.panelForeground
                    font.pixelSize: 11
                    text: Qt.formatDateTime(root.now, "ddd MMM d   hh:mm")
                }
            }
        }

    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 5
        color: launcher.lastLaunchSucceeded ? lunar.accentSoft : lunar.danger
        height: 34
        radius: 12
        visible: launcher.launchMessage.length > 0
        width: Math.min(360, root.width - 40)
        z: 20

        Text {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            color: root.panelForeground
            elide: Text.ElideRight
            font.pixelSize: 12
            text: launcher.launchMessage
            verticalAlignment: Text.AlignVCenter
        }
    }

    SystemMenu {
        id: systemMenu
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        launcherController: launcher
        powerController: northstarPowerController
        overviewWindow: applicationOverview
        settingsSurface: settingsWindow
        filesWindow: fileBrowserWindow
        softwareWindow: softwareCenterWindow
        sessionController: northstarSessionController
        shortcutCatalog: northstarShortcutCatalog
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    Connections {
        target: northstarSearchController
        enabled: displayIndex === 0

        function onActionRequested(actionId) {
            if (actionId === "applications") {
                applicationOverview.openWithQuery("")
            } else if (actionId === "files") {
                fileBrowserWindow.openBrowser()
            } else if (actionId === "settings") {
                settingsWindow.openSettings()
            } else if (actionId === "software") {
                softwareCenterWindow.openSoftware()
            } else if (actionId === "terminal") {
                launcher.launchTerminal()
            } else if (actionId === "browser") {
                launcher.launchBrowser()
            }
        }

        function onApplicationRequested(desktopId) {
            launcher.launchApplication(desktopId)
        }

        function onFileRequested(path, isDirectory) {
            fileBrowserWindow.openPath(path, isDirectory, false)
        }
    }

    SearchOverlay {
        id: searchOverlay
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        controller: northstarSearchController
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    ApplicationOverview {
        id: applicationOverview
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        applicationLauncher: launcher
        pinnedApplications: northstarPinnedApplicationModel
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
        surfaceBackground: root.panelBackground
        surfaceForeground: root.panelForeground
        surfaceMuted: root.panelMuted
        surfaceAccent: root.panelAccent
    }

    QuickSettings {
        id: quickSettingsWindow
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        controller: northstarQuickSettingsController
        state: shellState
        settingsWindow: settingsWindow
        targetScreen: targetScreen
        panelHeight: root.height
    }

    NotificationCenterWindow {
        id: notificationCenterWindow
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        center: northstarNotificationCenter
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    SoftwareCenterWindow {
        id: softwareCenterWindow
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        packageCatalog: northstarPackageCatalog
        applicationLauncher: launcher
        packageTrust: northstarPackageTrustController
        updatePlan: northstarUpdatePlanController
        updateAuthorization: northstarUpdateAuthorizationController
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    FileBrowserWindow {
        id: fileBrowserWindow
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        fileBrowserController: northstarFileBrowserController
        applicationLauncher: launcher
        volumeController: northstarVolumeController
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
        previewWindow: quickLookWindow
    }

    QuickLookWindow {
        id: quickLookWindow
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        previewController: northstarPreviewController
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    SettingsWindow {
        id: settingsWindow
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        state: shellState
        desktopLayoutController: northstarDesktopLayoutController
        launcherController: launcher
        sessionController: northstarSessionController
        settingsCatalog: northstarSettingsCatalog
        targetScreen: targetScreen
        panelHeight: root.height
    }
}
