import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Dialog {
    id: nameDialog
    objectName: "nameDialog"
    required property var ownerWindow
    required property var theme
    property alias nameText: nameField.text
    property string mode: "create"
    property string originalPath: ""

    title: mode === "rename" ? "Rename item"
        : mode === "file" ? "Create file" : "Create folder"
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

    contentItem: Column {
        spacing: 10
        width: nameDialog.width - (2 * nameDialog.padding)

        Text {
            color: ownerWindow.surfaceForeground
            text: nameDialog.mode === "rename"
                ? "Choose a new name for the selected item."
                : nameDialog.mode === "file"
                    ? "Choose a name for the new empty file."
                    : "Choose a name for the new folder."
            wrapMode: Text.WordWrap
            width: parent.width
        }

        TextField {
            id: nameField
            width: parent.width
            placeholderText: "Name"
            selectByMouse: true
            onAccepted: nameDialog.accept()
        }
    }

    onOpened: {
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    onAccepted: {
        const succeeded = mode === "rename"
            ? ownerWindow.fileBrowserController.renameEntry(originalPath, nameField.text)
            : mode === "file"
                ? ownerWindow.fileBrowserController.createFile(nameField.text)
                : ownerWindow.fileBrowserController.createFolder(nameField.text)
        if (succeeded) {
            ownerWindow.clearSelection()
        } else {
            Qt.callLater(function() { nameDialog.open() })
        }
    }
}
