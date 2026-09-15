import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Column {
    id: storageSidebar
    required property var hostWindow
    required property var theme
    readonly property var controller: hostWindow.volumeController
    readonly property var partitions: controller ? controller.storagePartitions.filter(function(row) { return row.eligible }) : []
    spacing: 6

    Text {
        width: parent.width
        text: "REMOVABLE"
        color: hostWindow.surfaceMuted
        font.pixelSize: 10
        font.bold: true
    }
    Text {
        width: parent.width
        visible: storageSidebar.partitions.length === 0
        text: controller && controller.scanning ? "Checking devices…" : "No supported USB volumes"
        color: hostWindow.surfaceMuted
        font.pixelSize: 10
        wrapMode: Text.WordWrap
    }
    Repeater {
        model: storageSidebar.partitions
        delegate: Column {
            id: volumeRow
            required property var modelData
            width: storageSidebar.width
            spacing: 3
            AuroraButton {
                objectName: "sidebarRemovableOpen"
                width: parent.width
                enabled: !storageSidebar.controller.scanning && !storageSidebar.controller.operationBusy
                    && (!volumeRow.modelData.mounted || !!volumeRow.modelData.browseReady)
                text: volumeRow.modelData.name || "USB volume"
                contentItem: Text {
                    text: parent.text
                    color: storageSidebar.hostWindow.surfaceForeground
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    verticalAlignment: Text.AlignVCenter
                }
                Accessible.name: text + (volumeRow.modelData.browseReady ? ", open read-only" : ", mount and open")
                onClicked: storageSidebar.hostWindow.openRemovablePartition(volumeRow.modelData)
            }
            Text {
                width: parent.width
                color: storageSidebar.hostWindow.surfaceMuted
                font.pixelSize: 10
                text: volumeRow.modelData.browseReady ? "Mounted · read-only"
                    : volumeRow.modelData.mounted ? "Mount needs verification" : "Click to mount and open"
                wrapMode: Text.WordWrap
            }
            AuroraButton {
                objectName: "sidebarRemovableUnmount"
                width: parent.width
                visible: !!volumeRow.modelData.mounted
                enabled: !storageSidebar.controller.scanning && !storageSidebar.controller.operationBusy
                    && !!storageSidebar.hostWindow.fileBrowserController
                    && !storageSidebar.hostWindow.fileBrowserController.transferActive
                text: "Eject"
                onClicked: storageSidebar.hostWindow.unmountRemovablePartition(volumeRow.modelData)
            }
        }
    }
}
