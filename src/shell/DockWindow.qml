import QtQuick
import QtQuick.Controls

Window {
    id: dock

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool
    height: 72
    width: 1280
    title: "Northstar Dock"

    property color dockBackground: shellState.darkMode ? "#171a21" : "#f4f6fb"
    property color dockForeground: shellState.darkMode ? "#f5f7fb" : "#1e2430"
    property color dockMuted: shellState.darkMode ? "#a9b1c2" : "#637083"
    property color dockAccent: shellState.darkMode ? "#79b8ff" : "#1769aa"
    property color dockButton: shellState.darkMode ? "#252b36" : "#e8edf5"

    Timer {
        id: refreshTimer
        interval: 1200
        repeat: true
        running: true
        onTriggered: northstarWindowController.refresh()
    }

    Component.onCompleted: northstarWindowController.refresh()

    Rectangle {
        anchors.fill: parent
        color: dock.dockBackground
        border.color: dock.dockAccent
        border.width: 1
        opacity: 0.98

        Row {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 10

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: dock.dockMuted
                font.pixelSize: 12
                text: "Shortcuts"
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: dock.dockAccent
                height: 1
                width: 26
            }

            Repeater {
                model: shellState.pinnedApplications

                delegate: Rectangle {
                    required property string modelData

                    color: shortcutMouse.containsMouse ? dock.dockAccent : dock.dockButton
                    height: 38
                    radius: 7
                    width: 112

                    Text {
                        anchors.centerIn: parent
                        color: dock.dockForeground
                        font.pixelSize: 12
                        text: modelData === "qterminal" ? "Terminal" : "Firefox"
                    }

                    MouseArea {
                        id: shortcutMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (modelData === "qterminal") {
                                launcher.launchTerminal()
                            } else {
                                launcher.launchBrowser()
                            }
                            refreshTimer.restart()
                        }
                    }
                }
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: dock.dockMuted
                height: 36
                opacity: 0.35
                width: 1
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: dock.dockMuted
                font.pixelSize: 12
                text: "Open apps"
            }

            Flickable {
                id: appStrip
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                contentWidth: appRow.width
                clip: true
                flickableDirection: Flickable.HorizontalFlick
                interactive: appRow.width > width
                width: Math.max(0, parent.width - x - refreshButton.width - 12)

                Row {
                    id: appRow
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Repeater {
                        model: northstarWindowController.windows

                        delegate: Rectangle {
                            required property var modelData

                            color: appMouse.containsMouse ? dock.dockAccent : dock.dockButton
                            height: 38
                            radius: 7
                            width: 188

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.right: minimizeButton.left
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                color: dock.dockForeground
                                elide: Text.ElideRight
                                font.pixelSize: 12
                                text: modelData.title
                            }

                            MouseArea {
                                id: appMouse
                                anchors.left: parent.left
                                anchors.right: minimizeButton.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                hoverEnabled: true
                                onClicked: northstarWindowController.activateWindow(modelData.viewId)
                            }

                            Rectangle {
                                id: minimizeButton
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                color: minimizeMouse.containsMouse ? dock.dockAccent : "transparent"
                                height: 28
                                radius: 5
                                width: 28

                                Text {
                                    anchors.centerIn: parent
                                    color: dock.dockForeground
                                    font.bold: true
                                    font.pixelSize: 14
                                    text: modelData.minimized ? "+" : "-"
                                }

                                MouseArea {
                                    id: minimizeMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: northstarWindowController.toggleMinimize(modelData.viewId)
                                }
                            }
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: dock.dockMuted
                        font.pixelSize: 12
                        text: northstarWindowController.windows.length === 0
                            ? (northstarWindowController.available ? "No open apps" : "Window controls unavailable")
                            : ""
                    }
                }
            }

            Rectangle {
                id: refreshButton
                anchors.verticalCenter: parent.verticalCenter
                color: refreshMouse.containsMouse ? dock.dockAccent : dock.dockButton
                height: 38
                radius: 7
                width: 82

                Text {
                    anchors.centerIn: parent
                    color: dock.dockForeground
                    font.pixelSize: 12
                    text: "Refresh"
                }

                MouseArea {
                    id: refreshMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: northstarWindowController.refresh()
                }
            }
        }
    }
}
