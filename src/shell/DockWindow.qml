import QtQuick
import QtQuick.Controls

Window {
    id: dock

    LunarPalette {
        id: lunar
        darkMode: shellState.darkMode
    }

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool
    height: 72
    width: 1280
    title: "Northstar Dock"

    property color dockBackground: lunar.dockGlass
    property color dockForeground: lunar.foreground
    property color dockMuted: lunar.muted
    property color dockAccent: lunar.accent
    property color dockButton: lunar.raised

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
        if (descriptor.indexOf("setting") >= 0 || descriptor.indexOf("preference") >= 0) {
            return "settings"
        }
        if (descriptor.indexOf("software") >= 0 || descriptor.indexOf("package") >= 0) {
            return "software"
        }
        if (descriptor.indexOf("text") >= 0 || descriptor.indexOf("editor") >= 0
                || descriptor.indexOf("note") >= 0) {
            return "editor"
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
            const candidate = ((window.appId || "") + " " + (window.title || "")).toLowerCase()
            if ((descriptor === "qterminal"
                    && (candidate.indexOf("terminal") >= 0 || candidate.indexOf("console") >= 0))
                    || (descriptor === "firefox"
                        && (candidate.indexOf("firefox") >= 0 || candidate.indexOf("browser") >= 0))) {
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
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        anchors.horizontalCenter: parent.horizontalCenter
        color: lunar.shadow
        height: 64
        radius: 24
        width: Math.min(parent.width - 20, 760)
        y: 8
    }

    Rectangle {
        id: dockSurface
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.horizontalCenter: parent.horizontalCenter
        color: dock.dockBackground
        height: 62
        radius: 22
        border.color: lunar.dockGlassEdge
        border.width: 1
        width: Math.min(parent.width - 24, 748)

        gradient: Gradient {
            GradientStop { position: 0.0; color: lunar.dockGlass }
            GradientStop { position: 1.0; color: Qt.rgba(lunar.panel.r, lunar.panel.g, lunar.panel.b, 0.34) }
        }

        Row {
            id: dockContent
            anchors.centerIn: parent
            height: 50
            spacing: 8

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: logoMouse.containsMouse ? lunar.raisedHover : lunar.accentSoft
                height: 48
                radius: 15
                scale: logoMouse.containsMouse ? 1.08 : 1.0
                width: 48

                Behavior on scale { NumberAnimation { duration: 140 } }

                Image {
                    anchors.centerIn: parent
                    fillMode: Image.PreserveAspectFit
                    height: 34
                    mipmap: true
                    smooth: true
                    source: northstarLogoSource
                    sourceClipRect: Qt.rect(270, 245, 485, 335)
                    width: 34
                }

                MouseArea {
                    id: logoMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: launcher.refreshApplications()
                }

                ToolTip.visible: logoMouse.containsMouse
                ToolTip.text: "Northstar"
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: lunar.borderSoft
                height: 34
                opacity: 0.8
                width: 1
            }

            Repeater {
                model: shellState.pinnedApplications

                delegate: Rectangle {
                    required property string modelData
                    readonly property bool running: dock.applicationIsRunning(modelData)

                    anchors.verticalCenter: parent.verticalCenter
                    color: pinnedMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: 48
                    radius: 14
                    scale: pinnedMouse.containsMouse ? 1.08 : 1.0
                    width: 48

                    Behavior on scale { NumberAnimation { duration: 140 } }

                    NorthstarIcon {
                        anchors.centerIn: parent
                        height: 34
                        iconName: dock.applicationIconName(modelData)
                        width: 34
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: dock.dockAccent
                        height: 4
                        radius: 2
                        visible: parent.running
                        width: 18
                    }

                    MouseArea {
                        id: pinnedMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: dock.launchPinned(modelData)
                    }

                    ToolTip.visible: pinnedMouse.containsMouse
                    ToolTip.text: modelData === "qterminal" ? "Terminal" : "Firefox"
                }
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: filesMouse.containsMouse ? lunar.raisedHover : "transparent"
                height: 48
                radius: 14
                scale: filesMouse.containsMouse ? 1.08 : 1.0
                width: 48

                Behavior on scale { NumberAnimation { duration: 140 } }

                NorthstarIcon {
                    anchors.centerIn: parent
                    height: 34
                    iconName: "files"
                    width: 34
                }

                MouseArea {
                    id: filesMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: filesWindow.openBrowser()
                }

                ToolTip.visible: filesMouse.containsMouse
                ToolTip.text: "Files"
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: lunar.borderSoft
                height: 34
                opacity: 0.8
                width: 1
            }

            Flickable {
                anchors.verticalCenter: parent.verticalCenter
                clip: true
                contentWidth: runningRow.width
                height: 50
                interactive: runningRow.width > width
                width: 250

                Row {
                    id: runningRow
                    height: parent.height
                    spacing: 6

                    Repeater {
                        model: northstarWindowController.windows

                        delegate: Rectangle {
                            required property var modelData

                            anchors.verticalCenter: parent.verticalCenter
                            color: modelData.active ? lunar.accentSoft
                                : runningMouse.containsMouse ? lunar.raisedHover : "transparent"
                            border.color: modelData.active ? lunar.accentBright : "transparent"
                            border.width: modelData.active ? 1 : 0
                            height: 48
                            radius: 14
                            scale: runningMouse.containsMouse ? 1.06 : 1.0
                            width: 48

                            Behavior on scale { NumberAnimation { duration: 140 } }

                            NorthstarIcon {
                                anchors.centerIn: parent
                                height: 32
                                iconName: dock.applicationIconName(modelData.appId || modelData.title)
                                opacity: modelData.minimized ? 0.62 : 1.0
                                width: 32
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 2
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: modelData.minimized ? lunar.muted : lunar.accentBright
                                height: 4
                                radius: 2
                                width: modelData.active ? 22 : 12
                            }

                            MouseArea {
                                id: runningMouse
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                hoverEnabled: true
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        northstarWindowController.toggleMinimize(modelData.viewId)
                                    } else {
                                        dock.activateOrToggle(modelData)
                                    }
                                }
                            }

                            ToolTip.visible: runningMouse.containsMouse
                            ToolTip.text: modelData.title + (modelData.minimized ? " (minimized)" : "")
                                + "\nRight-click to minimize or restore"
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: dock.dockMuted
                        font.pixelSize: 11
                        text: northstarWindowController.windows.length === 0 ? "No open apps" : ""
                        visible: text.length > 0
                    }
                }
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: lunar.borderSoft
                height: 34
                opacity: 0.8
                width: 1
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: trashMouse.containsMouse ? "#664052" : "transparent"
                height: 48
                radius: 14
                scale: trashMouse.containsMouse ? 1.08 : 1.0
                width: 48

                Behavior on scale { NumberAnimation { duration: 140 } }

                NorthstarIcon {
                    anchors.centerIn: parent
                    height: 34
                    iconName: "trash"
                    width: 34
                }

                MouseArea {
                    id: trashMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: filesWindow.openTrash()
                }

                ToolTip.visible: trashMouse.containsMouse
                ToolTip.text: "Trash"
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
