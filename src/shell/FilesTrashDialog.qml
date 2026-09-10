import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Dialog {
    id: trashDialog
    objectName: "trashDialog"
    required property var ownerWindow
    required property var theme
    property string itemPath: ""
    property string itemName: ""

    title: "Delete item?"
    modal: true
    padding: 16
    standardButtons: Dialog.Cancel | Dialog.Ok
    width: Math.min(420, ownerWindow.width - 48)
    x: (ownerWindow.width - width) / 2
    y: (ownerWindow.height - height) / 2

    background: Rectangle {
        color: ownerWindow.surfaceBackground
        border.color: ownerWindow.surfaceAccent
        border.width: 1
        radius: 8
    }

    contentItem: Text {
        color: ownerWindow.surfaceForeground
        text: "Move \"" + trashDialog.itemName
            + "\" to the Northstar Trash? You can restore it later."
        wrapMode: Text.WordWrap
        width: trashDialog.width - (2 * trashDialog.padding)
    }

    onAccepted: {
        if (ownerWindow.fileBrowserController.moveToTrash(itemPath)) {
            ownerWindow.clearSelection()
        } else {
            Qt.callLater(function() { trashDialog.open() })
        }
    }
}
