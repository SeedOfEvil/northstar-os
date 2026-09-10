import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Dialog {
    id: restoreDialog
    objectName: "restoreDialog"
    required property var ownerWindow
    required property var theme
    property string itemPath: ""
    property string itemName: ""
    property string originalLocation: ""

    title: "Restore item?"
    modal: true
    padding: 16
    standardButtons: Dialog.Cancel | Dialog.Ok
    width: Math.min(440, ownerWindow.width - 48)
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
        text: "Restore \"" + restoreDialog.itemName + "\" to "
            + restoreDialog.originalLocation + "?"
        wrapMode: Text.WordWrap
        width: restoreDialog.width - (2 * restoreDialog.padding)
    }

    onAccepted: {
        if (ownerWindow.fileBrowserController.restoreEntry(itemPath)) {
            ownerWindow.clearSelection()
        } else {
            Qt.callLater(function() { restoreDialog.open() })
        }
    }
}
