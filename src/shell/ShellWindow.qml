import QtQuick
import QtQuick.Controls

Window {
    id: root

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool
    height: 96
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

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                color: root.panelForeground
                font.bold: true
                font.pixelSize: 16
                text: "Northstar"
            }

            Rectangle {
                anchors.left: parent.left
                anchors.leftMargin: 132
                anchors.verticalCenter: parent.verticalCenter
                color: "transparent"
                height: 32
                width: 170

                Text {
                    anchors.centerIn: parent
                    color: root.panelMuted
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    text: shellState.activeWindowTitle
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Rectangle {
                    color: systemMouse.containsMouse ? root.panelAccent : "transparent"
                    height: 30
                    radius: 6
                    width: 42

                    Text {
                        anchors.centerIn: parent
                        color: root.panelForeground
                        font.pixelSize: 16
                        text: "☰"
                    }

                    MouseArea {
                        id: systemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: systemMenu.openMenu()
                    }
                }

                Text {
                    color: root.panelForeground
                    font.pixelSize: 13
                    text: Qt.formatDateTime(root.now, "ddd MMM d  hh:mm")
                }
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 52
            color: root.panelBackground
            opacity: 0.98

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Text {
                    color: root.panelMuted
                    font.pixelSize: 12
                    text: "Dock"
                    verticalAlignment: Text.AlignVCenter
                }

                Rectangle {
                    color: root.panelAccent
                    height: 1
                    width: 26
                }

                Repeater {
                    model: shellState.pinnedApplications

                    delegate: Rectangle {
                        color: dockMouse.containsMouse ? root.panelAccent : (shellState.darkMode ? "#252b36" : "#e8edf5")
                        height: 34
                        radius: 7
                        width: 112

                        Text {
                            anchors.centerIn: parent
                            color: root.panelForeground
                            font.pixelSize: 12
                            text: modelData === "qterminal" ? "Terminal" : "Firefox"
                        }

                        MouseArea {
                            id: dockMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: modelData === "qterminal" ? launcher.launchTerminal() : launcher.launchBrowser()
                        }
                    }
                }
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                color: root.panelMuted
                font.pixelSize: 11
                text: "Display " + (displayIndex + 1)
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
        overviewWindow: applicationOverview
        settingsSurface: settingsWindow
        sessionController: northstarSessionController
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

    SettingsWindow {
        id: settingsWindow
        state: shellState
        launcherController: launcher
        sessionController: northstarSessionController
        targetScreen: targetScreen
        panelHeight: root.height
    }
}
