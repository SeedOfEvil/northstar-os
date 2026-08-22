import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Northstar.Ui 1.0

ApplicationWindow {
    id: root
    width: 760
    height: 620
    minimumWidth: 640
    minimumHeight: 520
    visible: true
    title: "Bluetooth"
    color: lunar.background

    property string selectedAddress: ""
    property string selectedName: ""

    LunarPalette { id: lunar; darkMode: northstarDarkMode }

    Connections {
        target: bluetoothController
        function onSetupWizardLaunched() { root.hide() }
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
            text: "Put a keyboard, mouse, or other Classic Bluetooth device in pairing mode, then choose Refresh."
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

                    Label {
                        anchors.fill: parent
                        anchors.margins: 12
                        verticalAlignment: Text.AlignVCenter
                        text: modelData.name
                        color: lunar.foreground
                        font.bold: root.selectedAddress === modelData.addressHex
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        id: mouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: !bluetoothController.busy
                        onClicked: {
                            root.selectedAddress = modelData.addressHex
                            root.selectedName = modelData.name
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
            text: "Set up " + root.selectedName
            color: lunar.foreground
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            visible: root.selectedAddress.length > 0
            text: "Northstar opens FreeBSD's foreground pairing wizard. It can configure Classic Bluetooth and HID keyboards or mice. Audio setup is separate."
            color: lunar.muted
            wrapMode: Text.WordWrap
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
                text: "Open Pairing Wizard"
                enabled: !bluetoothController.busy && root.selectedAddress.length > 0
                onClicked: bluetoothController.openSetupWizard(root.selectedAddress)
            }
        }
    }

    Component.onCompleted: if (!bluetoothSelfTest) bluetoothController.refreshDevices()
}
