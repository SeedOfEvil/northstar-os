import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

    function applicationIconName(applicationId) {
        const descriptor = String(applicationId || "").toLowerCase()
        if (descriptor.indexOf("terminal") >= 0 || descriptor.indexOf("qterminal") >= 0
                || descriptor.indexOf("shell") >= 0 || descriptor.indexOf("console") >= 0) {
            return "terminal"
        }
        if (descriptor.indexOf("firefox") >= 0 || descriptor.indexOf("browser") >= 0) {
            return "browser"
        }
        if (descriptor.indexOf("file") >= 0 || descriptor.indexOf("manager") >= 0) {
            return "files"
        }
        return "northstar"
    }

    function applicationIsRunning(applicationId) {
        if (!northstarWindowController || !northstarWindowController.windows) {
            return false
        }

        const descriptor = String(applicationId || "").toLowerCase()
        for (let index = 0; index < northstarWindowController.windows.length; ++index) {
            const window = northstarWindowController.windows[index]
            const windowDescriptor = ((window.appId || "") + " " + (window.title || "")).toLowerCase()
            if ((descriptor === "qterminal"
                    && (windowDescriptor.indexOf("qterminal") >= 0
                        || windowDescriptor.indexOf("terminal") >= 0
                        || windowDescriptor.indexOf("console") >= 0))
                    || (descriptor === "firefox"
                        && (windowDescriptor.indexOf("firefox") >= 0
                            || windowDescriptor.indexOf("browser") >= 0))) {
                return true
            }
        }
        return false
    }

    function launchPinned(applicationId) {
        if (applicationId === "qterminal") {
            launcher.launchTerminal()
        } else if (applicationId === "firefox") {
            launcher.launchBrowser()
        }
        refreshTimer.restart()
    }

    function activateOrToggle(window) {
        if (!northstarWindowController || !window) {
            return
        }
        if (window.minimized || window.active) {
            northstarWindowController.toggleMinimize(window.viewId)
        } else {
            northstarWindowController.activateWindow(window.viewId)
        }
        refreshTimer.restart()
    }

    Timer {
        id: refreshTimer
        interval: 1200
        repeat: true
        running: true
        onTriggered: northstarWindowController.refresh()
    }

    Component.onCompleted: northstarWindowController.refresh()

    Rectangle {
        id: dockSurface
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        color: dock.dockBackground
        height: 58
        opacity: 0.98
        radius: 18
        border.color: dock.dockAccent
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: 28
                Layout.preferredWidth: 28
                fillMode: Image.PreserveAspectFit
                mipmap: true
                smooth: true
                source: northstarLogoSource
                sourceClipRect: Qt.rect(270, 245, 485, 335)
            }

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: 26
                Layout.preferredWidth: 1
                color: dock.dockAccent
                opacity: 0.7
            }

            Repeater {
                model: shellState.pinnedApplications

                delegate: Rectangle {
                    required property string modelData
                    readonly property bool running: dock.applicationIsRunning(modelData)

                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredHeight: 38
                    Layout.preferredWidth: 88
                    color: shortcutMouse.containsMouse ? dock.dockAccent : dock.dockButton
                    radius: 8
                    border.color: running ? dock.dockAccent : "transparent"
                    border.width: running ? 1 : 0

                    Row {
                        anchors.centerIn: parent
                        spacing: 5

                        NorthstarIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            height: 24
                            width: 24
                            iconName: dock.applicationIconName(modelData)
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: dock.dockForeground
                            elide: Text.ElideRight
                            font.pixelSize: 12
                            text: modelData === "qterminal" ? "Terminal" : "Firefox"
                            width: 48
                        }
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: dock.dockAccent
                        height: 3
                        radius: 2
                        visible: running
                        width: 24
                    }

                    MouseArea {
                        id: shortcutMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: dock.launchPinned(modelData)
                    }
                }
            }

            Rectangle {
                id: filesShortcut
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: 38
                Layout.preferredWidth: 78
                color: filesShortcutMouse.containsMouse ? dock.dockAccent : dock.dockButton
                radius: 8

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
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: 38
                Layout.preferredWidth: 78
                color: trashShortcutMouse.containsMouse ? "#c34f65" : dock.dockButton
                radius: 8

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
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: 36
                Layout.preferredWidth: 1
                color: dock.dockMuted
                opacity: 0.35
            }

            Text {
                Layout.alignment: Qt.AlignVCenter
                Layout.minimumWidth: 0
                Layout.preferredWidth: 62
                color: dock.dockMuted
                elide: Text.ElideRight
                font.pixelSize: 12
                text: "Open apps"
            }

            Flickable {
                id: appStrip
                Layout.alignment: Qt.AlignVCenter
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 84
                clip: true
                contentWidth: appRow.width
                flickableDirection: Flickable.HorizontalFlick
                interactive: appRow.width > width

                Row {
                    id: appRow
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Repeater {
                        model: northstarWindowController.windows

                        delegate: Rectangle {
                            required property var modelData

                            color: modelData.active
                                ? dock.dockAccent
                                : appMouse.containsMouse ? dock.dockAccent : dock.dockButton
                            height: 38
                            radius: 8
                            width: 172
                            border.color: modelData.active ? dock.dockForeground : "transparent"
                            border.width: modelData.active ? 1 : 0

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 6
                                spacing: 6

                                NorthstarIcon {
                                    anchors.verticalCenter: parent.verticalCenter
                                    height: 22
                                    width: 22
                                    iconName: dock.applicationIconName(modelData.appId || modelData.title)
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: dock.dockForeground
                                    elide: Text.ElideRight
                                    font.pixelSize: 12
                                    text: modelData.title
                                    width: parent.width - 62
                                }

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: modelData.active ? dock.dockForeground : dock.dockAccent
                                    height: 7
                                    radius: 4
                                    visible: !modelData.minimized
                                    width: 7
                                }
                            }

                            MouseArea {
                                id: appMouse
                                anchors.left: parent.left
                                anchors.right: minimizeButton.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                hoverEnabled: true
                                onClicked: dock.activateOrToggle(modelData)
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

                ScrollBar.horizontal: ScrollBar {}
            }

            Rectangle {
                id: refreshButton
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: 38
                Layout.preferredWidth: 38
                color: refreshMouse.containsMouse ? dock.dockAccent : dock.dockButton
                radius: 8

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
        volumeController: northstarVolumeController
        state: shellState
        targetScreen: targetScreen
        panelHeight: 44
    }
}
