import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Dialog {
    id: devicesDialog
    objectName: "removableDevicesDialog"
    required property var ownerWindow
    required property var theme
    property bool waitingForOperation: false
    Connections {
        target: ownerWindow.volumeController
        function onStorageOperationFinished() {
            if (devicesDialog.waitingForOperation) {
                devicesDialog.waitingForOperation = false
                devicesDialog.open()
            }
        }
    }
    title: "Removable devices"
    modal: true
    padding: 16
    standardButtons: Dialog.Close
    width: Math.min(520, ownerWindow.width - 48)
    height: Math.min(420, ownerWindow.height - 48)
    x: (ownerWindow.width - width) / 2
    y: (ownerWindow.height - height) / 2
    onOpened: if (ownerWindow.volumeController) ownerWindow.volumeController.scanRemovable()
    background: Rectangle {
        color: ownerWindow.surfaceBackground
        border.color: theme.borderSoft
        radius: theme.radiusMedium
    }
    contentItem: Column {
        spacing: 12
        AuroraButton {
            text: ownerWindow.volumeController && ownerWindow.volumeController.scanning ? "Scanning..." : "Refresh"
            enabled: !!ownerWindow.volumeController && !ownerWindow.volumeController.scanning && !ownerWindow.volumeController.operationBusy
            onClicked: ownerWindow.volumeController.scanRemovable()
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            color: ownerWindow.surfaceMuted
            text: ownerWindow.volumeController ? ownerWindow.volumeController.removableStatus : "Device detection unavailable."
        }
        Text {
            width: parent.width
            wrapMode: Text.WrapAnywhere
            textFormat: Text.PlainText
            color: ownerWindow.surfaceForeground
            text: ownerWindow.volumeController ? ownerWindow.volumeController.operationStatus : ""
            visible: text.length > 0
        }
        ListView {
            width: parent.width
            height: Math.max(60, parent.height - y)
            clip: true
            spacing: 8
            model: ownerWindow.volumeController ? (ownerWindow.volumeController.storagePartitions.length > 0
                ? ownerWindow.volumeController.storagePartitions : ownerWindow.volumeController.removableDevices) : []
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: deviceText.implicitHeight + 24 + (modelData.eligible ? 44 : 0)
                radius: 8
                color: theme.raised
                Text {
                    id: deviceText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 12
                    anchors.top: parent.top
                    color: ownerWindow.surfaceForeground
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                    text: (modelData.name || "Removable media") + "\n"
                        + modelData.device + " • " + (modelData.totalBytes / 1073741824).toFixed(1)
                        + " GiB\n" + (modelData.eligible ? (modelData.mounted ? "Mounted read-only" : "NTFS • Read-only mount available")
                            : (modelData.reason || "Detected only — filesystem and mount eligibility unverified"))
                }
                Row {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 8
                    spacing: 8
                    visible: !!modelData.eligible
                    AuroraButton {
                        objectName: "removableMountAction"
                        text: modelData.mounted ? "Unmount" : "Mount read-only"
                        enabled: !!modelData.eligible && !ownerWindow.volumeController.operationBusy && !ownerWindow.volumeController.scanning
                        onClicked: {
                            const device = modelData.device
                            const identity = modelData.identity
                            const mount = !modelData.mounted
                            devicesDialog.waitingForOperation = true
                            devicesDialog.close()
                            Qt.callLater(function() {
                                ownerWindow.volumeController.storageAction(device, identity, mount)
                            })
                        }
                    }
                    AuroraButton {
                        text: "Browse"
                        visible: !!modelData.mounted
                        onClicked: {
                            ownerWindow.openVolume(modelData.mountPath, modelData.name)
                            devicesDialog.close()
                        }
                    }
                }
            }
        }
    }
}
