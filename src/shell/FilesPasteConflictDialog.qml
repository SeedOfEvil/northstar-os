import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Dialog {
    id: pasteConflictDialog
    objectName: "pasteConflictDialog"
    required property var ownerWindow
    required property var theme

    title: "An item already exists"
    modal: true
    padding: 16
    standardButtons: Dialog.Cancel
    width: Math.min(460, ownerWindow.width - 48)
    x: (ownerWindow.width - width) / 2
    y: (ownerWindow.height - height) / 2

    background: Rectangle {
        color: ownerWindow.surfaceBackground
        border.color: theme.warning
        border.width: 1
        radius: 8
    }

    contentItem: Column {
        spacing: 12
        width: pasteConflictDialog.width - (2 * pasteConflictDialog.padding)

        Text {
            color: ownerWindow.surfaceForeground
            text: "A file or folder named \""
                + (ownerWindow.fileBrowserController ? ownerWindow.fileBrowserController.conflictName : "item")
                + "\" is already in this location. Keep both items with a safe copy name?"
            wrapMode: Text.WordWrap
            width: parent.width
        }

        AuroraButton {
            text: "Keep Both"
            onClicked: {
                if (ownerWindow.fileBrowserController.pasteClipboard("keepBoth")) {
                    pasteConflictDialog.close()
                    ownerWindow.clearSelection()
                }
            }
        }
    }

    onRejected: {
        if (ownerWindow.fileBrowserController) {
            ownerWindow.fileBrowserController.cancelConflict()
        }
    }
}
