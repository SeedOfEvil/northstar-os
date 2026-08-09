import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: editor

    property var documentController: northstarTextEditorController
    property color backgroundColor: "#101724"
    property color panelColor: "#192235"
    property color raisedColor: "#222f45"
    property color foregroundColor: "#f5f7fb"
    property color mutedColor: "#a9b8cf"
    property color accentColor: "#79b8ff"
    property bool syncingText: false
    property bool closeConfirmed: false
    property bool closeAfterSave: false

    color: editor.backgroundColor
    height: 680
    minimumHeight: 420
    minimumWidth: 620
    title: editor.documentController && editor.documentController.filePath.length > 0
        ? "Northstar Text Editor — " + editor.documentController.filePath.split("/").pop()
        : "Northstar Text Editor"
    visible: true
    width: 920

    onClosing: function(close) {
        if (editor.closeConfirmed || !editor.documentController || !editor.documentController.dirty) {
            close.accepted = true
            return
        }
        close.accepted = false
        unsavedDialog.open()
    }

    function saveDocument() {
        if (!editor.documentController || !editor.documentController.dirty) {
            return
        }
        if (editor.documentController.filePath.length > 0) {
            editor.documentController.save()
        } else {
            openSaveAsDialog()
        }
    }

    function openSaveAsDialog() {
        saveAsDialog.validationMessage = ""
        saveAsDialog.open()
    }

    Dialog {
        id: saveAsDialog
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel | Dialog.Ok
        title: "Save new document"
        width: 420

        property string validationMessage: ""

        contentItem: Column {
            spacing: 12
            width: saveAsDialog.width - (2 * saveAsDialog.padding)

            Text {
                color: editor.foregroundColor
                text: "New documents are saved in ~/Documents."
                wrapMode: Text.WordWrap
                width: parent.width
            }

            TextField {
                id: saveAsNameField
                placeholderText: "File name"
                selectByMouse: true
                width: parent.width
                onAccepted: saveAsDialog.accept()
            }

            Text {
                color: "#ff9b9b"
                text: saveAsDialog.validationMessage
                visible: text.length > 0
                wrapMode: Text.WordWrap
                width: parent.width
            }
        }

        onOpened: {
            saveAsNameField.forceActiveFocus()
            saveAsNameField.selectAll()
        }

        onAccepted: {
            const fileName = saveAsNameField.text.trim()
            if (fileName.length === 0 || fileName.indexOf("/") >= 0
                || fileName.indexOf("\\") >= 0) {
                validationMessage = "Enter a file name without folder separators."
                Qt.callLater(function() { saveAsDialog.open() })
                return
            }

            const directory = editor.documentController.defaultSaveDirectory
            const separator = directory.endsWith("/") ? "" : "/"
            const saved = editor.documentController.saveAs(directory + separator + fileName)
            if (saved) {
                saveAsDialog.close()
                if (editor.closeAfterSave) {
                    editor.closeAfterSave = false
                    editor.closeConfirmed = true
                    editor.close()
                }
            } else {
                validationMessage = editor.documentController.statusMessage
                Qt.callLater(function() { saveAsDialog.open() })
            }
        }

        onRejected: editor.closeAfterSave = false
    }

    Dialog {
        id: unsavedDialog
        modal: true
        title: "Save changes?"
        standardButtons: Dialog.NoButton
        padding: 16
        width: 420

        contentItem: Column {
            spacing: 14
            width: unsavedDialog.width - (2 * unsavedDialog.padding)

            Text {
                color: editor.foregroundColor
                text: "This document has unsaved changes."
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Row {
                spacing: 8

                Button {
                    text: editor.documentController && editor.documentController.filePath.length > 0
                        ? "Save" : "Save As..."
                    onClicked: {
                        if (editor.documentController.filePath.length > 0
                            && editor.documentController.save()) {
                            editor.closeConfirmed = true
                            unsavedDialog.close()
                            editor.close()
                } else if (editor.documentController.filePath.length === 0) {
                    editor.closeAfterSave = true
                    unsavedDialog.close()
                    editor.openSaveAsDialog()
                }
                    }
                }

                Button {
                    text: "Discard"
                    onClicked: {
                        editor.closeConfirmed = true
                        unsavedDialog.close()
                        editor.close()
                    }
                }

                Button {
                    text: "Cancel"
                    onClicked: unsavedDialog.close()
                }
            }
        }
    }

    Connections {
        target: editor.documentController

        function onStateChanged() {
            if (editor.documentController && editor.textArea.text !== editor.documentController.text) {
                editor.syncingText = true
                editor.textArea.text = editor.documentController.text
                editor.syncingText = false
            }
        }
    }

    background: Rectangle {
        color: editor.backgroundColor

        Rectangle {
            anchors.fill: parent
            anchors.margins: 18
            color: editor.panelColor
            radius: 16
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 14

        Row {
            width: parent.width
            spacing: 12

            Image {
                anchors.verticalCenter: parent.verticalCenter
                height: 42
                source: "qrc:/Northstar/TextEditor/northstar-text-editor.svg"
                width: 42
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                width: parent.width - saveButton.width - 60

                Text {
                    color: editor.foregroundColor
                    elide: Text.ElideMiddle
                    font.bold: true
                    font.pixelSize: 22
                    text: editor.documentController && editor.documentController.filePath.length > 0
                        ? editor.documentController.filePath.split("/").pop() : "Untitled document"
                    width: parent.width
                }

                Text {
                    color: editor.mutedColor
                    font.pixelSize: 12
                    text: editor.documentController && editor.documentController.dirty
                        ? "Unsaved changes" : "Northstar Text Editor"
                }
            }

            Button {
                id: saveButton
                enabled: editor.documentController && editor.documentController.canSave
                text: editor.documentController && editor.documentController.filePath.length > 0
                    ? "Save" : "Save As..."
                onClicked: editor.saveDocument()
            }
        }

        Rectangle {
            color: editor.raisedColor
            height: parent.height - 120
            radius: 10
            width: parent.width

            TextArea {
                id: textArea
                anchors.fill: parent
                anchors.margins: 14
                color: editor.foregroundColor
                font.family: "monospace"
                font.pixelSize: 14
                placeholderText: "Start typing..."
                placeholderTextColor: editor.mutedColor
                selectByMouse: true
                text: editor.documentController ? editor.documentController.text : ""
                wrapMode: TextArea.Wrap

                onTextChanged: {
                    if (!editor.syncingText && editor.documentController) {
                        editor.documentController.setText(text)
                    }
                }
            }
        }

        Row {
            spacing: 12
            width: parent.width

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: editor.mutedColor
                elide: Text.ElideRight
                font.pixelSize: 12
                text: editor.documentController ? editor.documentController.statusMessage : ""
                width: parent.width - closeButton.width - 12
            }

            Button {
                id: closeButton
                text: "Close"
                onClicked: editor.close()
            }
        }
    }
}
