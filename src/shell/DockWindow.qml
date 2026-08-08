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
        color: "transparent"

        Rectangle {
            id: dockSurface
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            color: dock.dockBackground
            height: 58
            opacity: 0.98
            radius: 18
            width: Math.min(parent.width - 32, 980)
            border.color: dock.dockAccent
            border.width: 1
        }

        Row {
            anchors.bottom: dockSurface.bottom
            anchors.left: dockSurface.left
            anchors.margins: 10
            anchors.right: dockSurface.right
            anchors.top: dockSurface.top
            spacing: 10

            Image {
                anchors.verticalCenter: parent.verticalCenter
                fillMode: Image.PreserveAspectFit
                height: 28
                mipmap: true
                smooth: true
                source: northstarLogoSource
                sourceClipRect: Qt.rect(270, 245, 485, 335)
                width: 28
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

                    anchors.verticalCenter: parent.verticalCenter
                    color: shortcutMouse.containsMouse ? dock.dockAccent : dock.dockButton
                    height: 38
                    radius: 7
                    width: 84

                    Row {
                        anchors.centerIn: parent
                        spacing: 5

                        NorthstarIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            height: 24
                            width: 24
                            iconName: modelData === "qterminal" ? "terminal" : "browser"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: dock.dockForeground
                            font.pixelSize: 12
                            text: modelData === "qterminal" ? "Terminal" : "Firefox"
                        }
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
                id: filesShortcut
                anchors.verticalCenter: parent.verticalCenter
                color: filesShortcutMouse.containsMouse ? dock.dockAccent : dock.dockButton
                height: 38
                radius: 7
                width: 72

                Row {
                    anchors.centerIn: parent
                    spacing: 4

                    NorthstarIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 24
                        width: 24
                        iconName: "files"
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: dock.dockForeground
                        font.pixelSize: 12
                        text: "Files"
                    }
                }

                MouseArea {
                    id: filesShortcutMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: filesWindow.openBrowser()
                }
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: trashShortcutMouse.containsMouse ? "#c34f65" : dock.dockButton
                height: 38
                radius: 7
                width: 72

                Row {
                    anchors.centerIn: parent
                    spacing: 4

                    NorthstarIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 24
                        width: 24
                        iconName: "trash"
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: dock.dockForeground
                        font.pixelSize: 12
                        text: "Trash"
                    }
                }

                MouseArea {
                    id: trashShortcutMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: filesWindow.openTrash()
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
                            width: 156

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
                width: 38

                NorthstarIcon {
                    anchors.centerIn: parent
                    height: 24
                    width: 24
                    iconName: "quick-settings"
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

    FileBrowserWindow {
        id: filesWindow
        fileBrowserController: northstarFileBrowserController
        applicationLauncher: launcher
        state: shellState
        targetScreen: targetScreen
        panelHeight: 44
    }
}
