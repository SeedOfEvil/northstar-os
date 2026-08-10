import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: quickSettings

    LunarPalette {
        id: lunar
        darkMode: quickSettings.state ? quickSettings.state.darkMode : true
    }

    property var state
    property var targetScreen
    property int panelHeight: 44
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property color surfaceBackground: lunar.panelStrong
    property color surfaceForeground: lunar.foreground
    property color surfaceMuted: lunar.muted
    property color surfaceAccent: lunar.accent
    property color surfaceRaised: lunar.raised
    property bool wifiEnabled: true
    property bool bluetoothEnabled: true
    property bool nightLightEnabled: false
    property bool doNotDisturbEnabled: false

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.Tool
    modality: Qt.NonModal
    title: "Northstar Quick Settings"

    width: 354
    height: 456
    x: screenX + screenWidth - width - 18
    y: screenY + panelHeight + 8

    WindowDragController {
        id: quickSettingsDrag
        window: quickSettings
        screenX: quickSettings.screenX
        screenY: quickSettings.screenY
        screenWidth: quickSettings.screenWidth
        screenHeight: quickSettings.screenHeight
        topInset: quickSettings.panelHeight
        bottomInset: 12
        defaultX: quickSettings.screenX + quickSettings.screenWidth - quickSettings.width - 18
        defaultY: quickSettings.screenY + quickSettings.panelHeight + 8
    }

    function openPanel() {
        quickSettingsDrag.prepareForOpen()
        show()
        raise()
        requestActivate()
    }

    function togglePanel() {
        if (visible) {
            hide()
        } else {
            openPanel()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: quickSettings.surfaceBackground
        border.color: lunar.border
        border.width: 1
        radius: lunar.radiusPanel

        gradient: Gradient {
            GradientStop { position: 0.0; color: lunar.panelStrong }
            GradientStop { position: 1.0; color: lunar.panel }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Row {
                width: parent.width

                Item {
                    id: quickSettingsDragHandle
                    height: 34
                    width: parent.width - closeButton.width - 8

                    Column {
                        anchors.fill: parent
                        spacing: 2

                        Text {
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 18
                            text: "Quick settings"
                        }

                        Text {
                            color: quickSettings.surfaceMuted
                            font.pixelSize: 11
                            text: "Northstar controls"
                        }
                    }

                    NativeWindowMoveHandler {
                        window: quickSettings
                    }
                }

                Rectangle {
                    id: closeButton
                    color: closeMouse.containsMouse ? quickSettings.surfaceAccent : quickSettings.surfaceRaised
                    height: 30
                    radius: lunar.radiusSmall
                    width: 62

                    Text {
                        anchors.centerIn: parent
                        color: quickSettings.surfaceForeground
                        font.pixelSize: 11
                        text: "Close"
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: quickSettings.hide()
                    }
                }
            }

            Row {
                spacing: 8
                width: parent.width

                Rectangle {
                    color: quickSettings.wifiEnabled ? lunar.accentSoft : quickSettings.surfaceRaised
                    border.color: quickSettings.wifiEnabled ? lunar.accentBright : lunar.borderSoft
                    border.width: 1
                    height: 62
                    radius: lunar.radiusMedium
                    width: (parent.width - parent.spacing) / 2

                    Column {
                        anchors.centerIn: parent
                        spacing: 3

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 12
                            text: "Wi-Fi"
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.surfaceForeground
                            font.pixelSize: 10
                            text: quickSettings.wifiEnabled ? "Connected" : "Off"
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: quickSettings.wifiEnabled = !quickSettings.wifiEnabled
                    }
                }

                Rectangle {
                    color: quickSettings.bluetoothEnabled ? lunar.accentSoft : quickSettings.surfaceRaised
                    border.color: quickSettings.bluetoothEnabled ? lunar.accentBright : lunar.borderSoft
                    border.width: 1
                    height: 62
                    radius: lunar.radiusMedium
                    width: (parent.width - parent.spacing) / 2

                    Column {
                        anchors.centerIn: parent
                        spacing: 3

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 12
                            text: "Bluetooth"
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.surfaceForeground
                            font.pixelSize: 10
                            text: quickSettings.bluetoothEnabled ? "On" : "Off"
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: quickSettings.bluetoothEnabled = !quickSettings.bluetoothEnabled
                    }
                }
            }

            Row {
                spacing: 8
                width: parent.width

                Rectangle {
                    color: quickSettings.nightLightEnabled ? lunar.accentSoft : quickSettings.surfaceRaised
                    height: 56
                    radius: lunar.radiusMedium
                    width: (parent.width - parent.spacing) / 2

                    Text {
                        anchors.centerIn: parent
                        color: quickSettings.surfaceForeground
                        font.pixelSize: 11
                        text: "Night Light  " + (quickSettings.nightLightEnabled ? "On" : "Off")
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: quickSettings.nightLightEnabled = !quickSettings.nightLightEnabled
                    }
                }

                Rectangle {
                    color: quickSettings.doNotDisturbEnabled ? lunar.accentSoft : quickSettings.surfaceRaised
                    height: 56
                    radius: lunar.radiusMedium
                    width: (parent.width - parent.spacing) / 2

                    Text {
                        anchors.centerIn: parent
                        color: quickSettings.surfaceForeground
                        font.pixelSize: 11
                        text: "Do Not Disturb  "
                            + (quickSettings.doNotDisturbEnabled ? "On" : "Off")
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: quickSettings.doNotDisturbEnabled = !quickSettings.doNotDisturbEnabled
                    }
                }
            }

            Rectangle {
                color: quickSettings.surfaceRaised
                height: 82
                radius: lunar.radiusMedium
                border.color: lunar.borderSoft
                border.width: 1
                width: parent.width

                Column {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    anchors.topMargin: 10
                    spacing: 3

                    Item {
                        height: 18
                        width: parent.width

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: quickSettings.surfaceForeground
                            font.pixelSize: 11
                            text: "Display"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            color: quickSettings.surfaceMuted
                            font.pixelSize: 10
                            text: "Preview"
                        }
                    }

                    Slider {
                        id: displaySlider
                        from: 0
                        to: 1
                        value: 0.7
                        width: parent.width
                    }

                    Item {
                        height: 18
                        width: parent.width

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: quickSettings.surfaceForeground
                            font.pixelSize: 11
                            text: "Sound"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            color: quickSettings.surfaceMuted
                            font.pixelSize: 10
                            text: Math.round(soundSlider.value * 100) + "%"
                        }
                    }

                    Slider {
                        id: soundSlider
                        from: 0
                        to: 1
                        value: 0.65
                        width: parent.width
                    }
                }
            }

            Rectangle {
                color: quickSettings.surfaceRaised
                height: 66
                radius: lunar.radiusMedium
                border.color: lunar.borderSoft
                border.width: 1
                width: parent.width

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 12

                    Rectangle {
                        color: quickSettings.surfaceAccent
                        height: 34
                        radius: 8
                        width: 34
                        y: (parent.height - height) / 2

                        Text {
                            anchors.centerIn: parent
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 14
                            text: "♪"
                        }
                    }

                    Column {
                        spacing: 2
                        y: (parent.height - height) / 2

                        Text {
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 11
                            text: "Media"
                        }

                        Text {
                            color: quickSettings.surfaceMuted
                            font.pixelSize: 10
                            text: "No media playing"
                        }
                    }
                }
            }
        }
    }
}
