import QtQuick
import QtQuick.Controls

Window {
    id: quickSettings

    property var state
    property var targetScreen
    property int panelHeight: 44
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property color surfaceBackground: state && state.darkMode ? "#172131" : "#f4f6fb"
    property color surfaceForeground: state && state.darkMode ? "#f5f7fb" : "#1e2430"
    property color surfaceMuted: state && state.darkMode ? "#a9b1c2" : "#637083"
    property color surfaceAccent: state && state.darkMode ? "#79b8ff" : "#1769aa"
    property color surfaceRaised: state && state.darkMode ? "#243149" : "#e8edf5"
    property bool wifiEnabled: true
    property bool bluetoothEnabled: true
    property bool nightLightEnabled: false
    property bool doNotDisturbEnabled: false

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.Tool
    modality: Qt.NonModal
    title: "Northstar Quick Settings"

    width: 342
    height: 440
    x: screenX + screenWidth - width - 18
    y: screenY + panelHeight + 8

    function openPanel() {
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
        border.color: quickSettings.surfaceAccent
        border.width: 1
        radius: 16

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Row {
                width: parent.width

                Column {
                    spacing: 2
                    width: parent.width - closeButton.width - 8

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

                Rectangle {
                    id: closeButton
                    color: closeMouse.containsMouse ? quickSettings.surfaceAccent : quickSettings.surfaceRaised
                    height: 30
                    radius: 7
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
                    color: quickSettings.wifiEnabled ? quickSettings.surfaceAccent : quickSettings.surfaceRaised
                    height: 62
                    radius: 10
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
                    color: quickSettings.bluetoothEnabled ? quickSettings.surfaceAccent : quickSettings.surfaceRaised
                    height: 62
                    radius: 10
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
                    color: quickSettings.nightLightEnabled ? quickSettings.surfaceAccent : quickSettings.surfaceRaised
                    height: 56
                    radius: 10
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
                    color: quickSettings.doNotDisturbEnabled ? quickSettings.surfaceAccent : quickSettings.surfaceRaised
                    height: 56
                    radius: 10
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
                radius: 10
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
                radius: 10
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
