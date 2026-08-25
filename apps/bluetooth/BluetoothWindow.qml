import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Northstar.Ui 1.0

ApplicationWindow {
    id: root
    width: 800
    height: 720
    minimumWidth: 680
    minimumHeight: 600
    visible: true
    title: "Bluetooth"
    color: lunar.background

    property string selectedAddress: ""
    property string selectedName: ""
    property bool selectedRemembered: false
    property bool selectedPaired: false
    property bool selectedConnected: false

    function clearSelection() {
        selectedAddress = ""
        selectedName = ""
        selectedRemembered = false
        selectedPaired = false
        selectedConnected = false
    }

    LunarPalette { id: lunar; darkMode: northstarDarkMode }

    Connections {
        target: bluetoothController
        function onAuthorizationPromptExpected() { root.hide() }
        function onAuthorizationCompleted() {
            root.show()
            root.raise()
            root.requestActivate()
        }
        function onPairingConfirmationRequested() {
            root.show()
            root.raise()
            root.requestActivate()
            confirmationDialog.open()
        }
        function onPairingFinished(success) {
            confirmationDialog.close()
            if (success) {
                root.clearSelection()
                bluetoothController.refreshDevices()
            }
        }
        function onForgetFinished(success) {
            if (success) {
                root.clearSelection()
                bluetoothController.refreshDevices()
            }
        }
    }

    Dialog {
        id: confirmationDialog
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose
        title: "Confirm Bluetooth pairing"
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: bluetoothController.respondToPairing(true)
        onRejected: bluetoothController.respondToPairing(false)

        ColumnLayout {
            width: 440
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: "Does this number also appear on " + root.selectedName + "?"
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: bluetoothController.confirmationCode
                font.bold: true
                font.pixelSize: 38
                font.letterSpacing: 4
            }
            Label {
                Layout.fillWidth: true
                text: "Choose OK only when both devices show exactly the same number. Otherwise choose Cancel."
                wrapMode: Text.WordWrap
            }
        }
    }

    FileDialog {
        id: sendFileDialog
        title: "Send a file to " + root.selectedName
        fileMode: FileDialog.OpenFile
        onAccepted: bluetoothController.sendFile(
            root.selectedAddress, selectedFile.toString())
    }

    Dialog {
        id: forgetDialog
        anchors.centerIn: parent
        modal: true
        title: "Forget " + root.selectedName + "?"
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: bluetoothController.forgetDevice(root.selectedAddress)

        Label {
            width: 420
            text: "Northstar will remove this device's local pairing and remembered state. You must also choose Forget or Unpair on the other device."
            wrapMode: Text.WordWrap
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: "Bluetooth devices"
                color: lunar.foreground
                font.bold: true
                font.pixelSize: 28
            }
            AuroraActivityIcon {
                Layout.preferredHeight: 28
                Layout.preferredWidth: 28
                running: bluetoothController.busy
            }
            AuroraButton {
                text: "Refresh"
                enabled: !bluetoothController.busy
                onClicked: bluetoothController.refreshDevices()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: visibilityLayout.implicitHeight + 24
            radius: 12
            color: lunar.panel
            border.color: lunar.borderSoft

            RowLayout {
                id: visibilityLayout
                anchors.fill: parent
                anchors.margins: 12
                Label {
                    Layout.fillWidth: true
                    text: bluetoothController.discoverable
                        ? "This computer is discoverable and connectable as northstar-image."
                        : "This computer is connectable but hidden from new devices."
                    color: lunar.foreground
                    wrapMode: Text.WordWrap
                }
                Button {
                    text: bluetoothController.discoverable ? "Hide This Computer" : "Make Discoverable"
                    enabled: !bluetoothController.busy
                    onClicked: bluetoothController.setDiscoverable(
                        !bluetoothController.discoverable)
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: "For a phone, leave its Bluetooth settings screen open while refreshing here. After choosing Pair and approving administrator access, choose Northstar on the phone."
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
                            visible: modelData.paired && !modelData.connected
                            text: "Paired"
                            color: lunar.accent
                            font.bold: true
                        }
                        Label {
                            visible: modelData.remembered && !modelData.paired
                                && !modelData.connected
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
                            root.selectedPaired = modelData.paired
                            root.selectedConnected = modelData.connected
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: deviceList.count === 0 && !bluetoothController.busy
                    text: "No devices listed. Choose Refresh while the other device is discoverable."
                    color: lunar.muted
                }
            }
        }

        Label {
            visible: root.selectedAddress.length > 0
            text: root.selectedConnected ? root.selectedName + " is connected"
                : (root.selectedPaired ? root.selectedName + " is paired"
                    : (root.selectedRemembered ? "Finish pairing " + root.selectedName
                                               : "Pair with " + root.selectedName))
            color: lunar.foreground
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            visible: root.selectedAddress.length > 0 && !root.selectedConnected
                && !root.selectedPaired
            text: "Choose Pair, approve administrator access, choose Northstar on the other device, then confirm the six-digit number shown on both devices."
            color: lunar.muted
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            visible: bluetoothController.fileTransferAvailable
            Label {
                Layout.fillWidth: true
                text: bluetoothController.receivingFiles
                    ? "Receiving Bluetooth files into Downloads"
                    : "Bluetooth file transfer"
                color: lunar.foreground
                font.bold: true
            }
            Button {
                text: bluetoothController.receivingFiles
                    ? "Stop Receiving" : "Receive Files"
                onClicked: bluetoothController.setReceivingFiles(
                    !bluetoothController.receivingFiles)
            }
            Button {
                text: "Send File"
                visible: root.selectedPaired
                enabled: !bluetoothController.busy
                onClicked: sendFileDialog.open()
            }
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
                text: "Forget"
                visible: root.selectedRemembered || root.selectedPaired
                enabled: !bluetoothController.busy
                onClicked: forgetDialog.open()
            }
            Button {
                id: pairButton
                text: "Pair"
                visible: !root.selectedConnected && !root.selectedPaired
                enabled: !bluetoothController.busy && root.selectedAddress.length > 0
                onClicked: bluetoothController.pairDevice(
                    root.selectedAddress, root.selectedName)
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Current alpha baseline: discovery, numeric-confirmation pairing, persisted paired state, live baseband state, and on-demand file transfer. HID and Bluetooth audio require separate profile services and are not claimed yet."
            color: lunar.muted
            wrapMode: Text.WordWrap
        }
    }

    Component.onCompleted: if (!bluetoothSelfTest) bluetoothController.refreshDevices()
}
