import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Dialog {
    id: emptyTrashDialog
    objectName: "emptyTrashDialog"
    required property var ownerWindow
    required property var theme

    title: "Empty Trash?"
    modal: true
    padding: 16
    standardButtons: Dialog.Cancel | Dialog.Ok
    width: Math.min(420, ownerWindow.width - 48)
    x: (ownerWindow.width - width) / 2
    y: (ownerWindow.height - height) / 2

    background: Rectangle {
        color: ownerWindow.surfaceBackground
        border.color: "#c34f65"
        border.width: 1
        radius: 8
    }

    contentItem: Text {
        color: ownerWindow.surfaceForeground
        text: "This permanently removes every item currently in the Northstar Trash."
        wrapMode: Text.WordWrap
        width: emptyTrashDialog.width - (2 * emptyTrashDialog.padding)
    }

    onAccepted: {
        if (ownerWindow.fileBrowserController.emptyTrash()) {
            ownerWindow.clearSelection()
        } else {
            Qt.callLater(function() { emptyTrashDialog.open() })
        }
    }
}
