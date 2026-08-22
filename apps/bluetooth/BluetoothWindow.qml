import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Northstar.Ui 1.0

ApplicationWindow {
    id: root
    width: 780
    height: 680
    minimumWidth: 660
    minimumHeight: 560
    visible: true
    title: "Bluetooth"
    color: lunar.background

    property string selectedAddress: ""
    property string selectedName: ""
    property bool selectedRemembered: false
    property bool selectedConnected: false

    LunarPalette { id: lunar; darkMode: northstarDarkMode }

    Connections {
        target: bluetoothController
        function onSecretsCleared() { pinField.text = "" }
        function onAuthorizationPromptExpected() { root.hide() }
        function onAuthorizationCompleted() {
            root.show()
            root.raise()
            root.requestActivate()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: "Bluetooth devices"
                color: lunar.foreground
                font.bold: true
                font.pixelSize: 28
            }
            Button {
                text: bluetoothController.busy ? "Working..." : "Refresh"
                enabled: !bluetoothController.busy
                onClicked: bluetoothController.refreshDevices()
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Put a device in pairing mode, then choose Refresh. Remembered devices stay listed even when they are not discoverable."
            color: lunar.muted
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 14
            color: lunar.panel
            border.color: lunar.borderSoft

            ListView {
                id: deviceList
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6
                clip: true
                model: bluetoothController.devices

                delegate: Rectangle {
                    required property var modelData
                    width: deviceList.width
                    height: 62
                    radius: 10
                    color: root.selectedAddress === modelData.addressHex
                        ? lunar.accentSoft : mouse.containsMouse ? lunar.raised : "transparent"
                    border.color: root.selectedAddress === modelData.addressHex ? lunar.accent : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        Label {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: lunar.foreground
                            font.bold: root.selectedAddress === modelData.addressHex
                            elide: Text.ElideRight
                        }
                        Label {
                            visible: modelData.connected
                            text: "Connected"
                            color: lunar.accent
                            font.bold: true
                        }
                        Label {
                            visible: modelData.remembered && !modelData.connected
                            text: "Remembered"
                            color: lunar.muted
                        }
                    }
                    MouseArea {
                        id: mouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: !bluetoothController.busy
                        onClicked: {
                            root.selectedAddress = modelData.addressHex
                            root.selectedName = modelData.name
                            root.selectedRemembered = modelData.remembered
                            root.selectedConnected = modelData.connected
                            pinField.text = ""
                            if (!modelData.connected) pinField.forceActiveFocus()
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: deviceList.count === 0 && !bluetoothController.busy
                    text: "No devices listed. Choose Refresh while the device is discoverable."
                    color: lunar.muted
                }
            }
        }

        Label {
            visible: root.selectedAddress.length > 0
            text: root.selectedConnected ? root.selectedName + " is connected"
                : (root.selectedRemembered ? "Pair " + root.selectedName + " again"
                                           : "Pair with " + root.selectedName)
            color: lunar.foreground
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            visible: root.selectedAddress.length > 0 && !root.selectedConnected
            text: "Choose a 4 to 16 digit PIN. If the other device asks for a code, enter exactly the same PIN there."
            color: lunar.muted
            wrapMode: Text.WordWrap
        }

        TextField {
            id: pinField
            Layout.fillWidth: true
            visible: root.selectedAddress.length > 0 && !root.selectedConnected
            enabled: !bluetoothController.busy
            echoMode: TextInput.Normal
            inputMethodHints: Qt.ImhDigitsOnly
            validator: RegularExpressionValidator { regularExpression: /^[0-9]{0,16}$/ }
            placeholderText: "Pairing PIN (4 to 16 digits)"
            maximumLength: 16
            onAccepted: pairButton.clicked()
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: bluetoothController.statusMessage
                color: bluetoothController.statusIsError ? lunar.warning : lunar.muted
                wrapMode: Text.WordWrap
            }
            Button {
                id: pairButton
                text: root.selectedRemembered ? "Pair Again" : "Pair"
                visible: !root.selectedConnected
                enabled: !bluetoothController.busy && root.selectedAddress.length > 0
                    && pinField.text.length >= 4
                onClicked: bluetoothController.pairDevice(
                    root.selectedAddress, root.selectedName, pinField.text)
            }
        }
    }

    Component.onCompleted: if (!bluetoothSelfTest) bluetoothController.refreshDevices()
}
