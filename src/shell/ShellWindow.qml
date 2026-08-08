import QtQuick
import QtQuick.Controls

Window {
    id: root

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool
    height: 44
    width: 1280
    title: "Northstar Shell"

    property color panelBackground: shellState.darkMode ? "#171a21" : "#f4f6fb"
    property color panelForeground: shellState.darkMode ? "#f5f7fb" : "#1e2430"
    property color panelMuted: shellState.darkMode ? "#a9b1c2" : "#637083"
    property color panelAccent: shellState.darkMode ? "#79b8ff" : "#1769aa"
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

    function closeTransientSurfaces() {
        if (systemMenu.visible) {
            systemMenu.closeMenu()
        } else if (applicationOverview.visible) {
            applicationOverview.hide()
        } else if (quickSettingsWindow.visible) {
            quickSettingsWindow.hide()
        } else if (notificationCenterWindow.visible) {
            notificationCenterWindow.hide()
        } else if (fileBrowserWindow.visible) {
            fileBrowserWindow.hide()
        } else if (settingsWindow.visible) {
            settingsWindow.hide()
        }
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
        enabled: systemMenu.visible || applicationOverview.visible || quickSettingsWindow.visible
            || notificationCenterWindow.visible || fileBrowserWindow.visible || settingsWindow.visible
        sequence: "Escape"
        onActivated: root.closeTransientSurfaces()
    }

    Rectangle {
        anchors.fill: parent
        color: root.panelBackground
        border.color: root.panelAccent
        border.width: 1

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
                    color: systemMouse.containsMouse ? root.panelAccent : "transparent"
                    height: 32
                    radius: 8
                    width: 30

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
                    font.pixelSize: 15
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
                    color: filesNavMouse.containsMouse ? root.panelAccent : "transparent"
                    height: 32
                    radius: 7
                    width: 42

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
                    color: appsNavMouse.containsMouse ? root.panelAccent : "transparent"
                    height: 32
                    radius: 7
                    width: 42

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
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                color: shellState.darkMode ? "#252d3c" : "#e8edf5"
                height: 30
                radius: 9
                width: Math.min(300, root.width - 500)

                TextField {
                    id: globalSearchField
                    anchors.fill: parent
                    color: root.panelForeground
                    font.pixelSize: 11
                    placeholderText: "Search Northstar apps..."
                    placeholderTextColor: root.panelMuted
                    selectByMouse: true

                    background: Rectangle {
                        color: "transparent"
                    }

                    onAccepted: {
                        applicationOverview.openWithQuery(text)
                        globalSearchField.text = ""
                    }
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.panelMuted
                    font.pixelSize: 10
                    text: "Super+K"
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 5

                Rectangle {
                    color: notificationMouse.containsMouse ? root.panelAccent : "transparent"
                    height: 30
                    radius: 7
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
                        color: "#c34f65"
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
                    color: quickSettingsMouse.containsMouse ? root.panelAccent : "transparent"
                    height: 30
                    radius: 7
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
                    text: Qt.formatDateTime(root.now, "ddd MMM d  hh:mm")
                }
            }
        }

    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 5
        color: launcher.lastLaunchSucceeded ? root.panelAccent : "#c34f65"
        height: 34
        radius: 7
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
        launcherController: launcher
        powerController: northstarPowerController
        overviewWindow: applicationOverview
        settingsSurface: settingsWindow
        filesWindow: fileBrowserWindow
        sessionController: northstarSessionController
        shortcutCatalog: northstarShortcutCatalog
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    ApplicationOverview {
        id: applicationOverview
        applicationLauncher: launcher
        targetScreen: targetScreen
        panelHeight: root.height
        surfaceBackground: root.panelBackground
        surfaceForeground: root.panelForeground
        surfaceMuted: root.panelMuted
        surfaceAccent: root.panelAccent
    }

    QuickSettings {
        id: quickSettingsWindow
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    NotificationCenterWindow {
        id: notificationCenterWindow
        center: northstarNotificationCenter
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    FileBrowserWindow {
        id: fileBrowserWindow
        fileBrowserController: northstarFileBrowserController
        applicationLauncher: launcher
        volumeController: northstarVolumeController
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    SettingsWindow {
        id: settingsWindow
        state: shellState
        launcherController: launcher
        sessionController: northstarSessionController
        targetScreen: targetScreen
        panelHeight: root.height
    }
}
