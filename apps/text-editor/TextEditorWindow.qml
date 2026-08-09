import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

ApplicationWindow {
    id: editor

    property var documentController: northstarTextEditorController
    LunarPalette { id: lunar; darkMode: northstarDarkMode }

    property color backgroundColor: lunar.background
    property color panelColor: lunar.panel
    property color raisedColor: lunar.raised
    property color foregroundColor: lunar.foreground
    property color mutedColor: lunar.muted
    property color accentColor: lunar.accent
    property bool syncingText: false
    property bool closeConfirmed: false
    property bool closeAfterSave: false

    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    height: 680
    minimumHeight: 420
    minimumWidth: 620
    title: editor.documentController && editor.documentController.filePath.length > 0
        ? "Northstar Text Editor — " + editor.documentController.filePath.split("/").pop()
        : "Northstar Text Editor"
    visible: true
    width: 920

    Component.onCompleted: {
        raise()
        requestActivate()
    }

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

    background: NorthstarWindowFrame {
        darkMode: lunar.darkMode
    }

    Column {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        NorthstarWindowTitleBar {
            closeDestroysWindow: true
            iconSource: "qrc:/Northstar/TextEditor/northstar-text-editor.svg"
            maximized: editor.visibility === Window.Maximized
            lunarPalette: lunar
            subtitle: editor.documentController && editor.documentController.dirty
                ? "Unsaved changes" : "Northstar Text Editor"
            title: editor.documentController && editor.documentController.filePath.length > 0
                ? editor.documentController.filePath.split("/").pop() : "Untitled document"
            width: parent.width
            window: editor
            onMaximizeRequested: {
                if (editor.visibility === Window.Maximized) editor.showNormal()
                else editor.showMaximized()
            }
        }

        Item {
            width: parent.width
            height: saveButton.implicitHeight

            Button {
                id: saveButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                enabled: editor.documentController && editor.documentController.canSave
                text: editor.documentController && editor.documentController.filePath.length > 0
                    ? "Save" : "Save As..."
                onClicked: editor.saveDocument()
            }
        }

        Rectangle {
            color: editor.raisedColor
                height: parent.height - 188
            border.color: lunar.borderSoft
            border.width: 1
            radius: 14
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

    NativeWindowResizeHandler {
        resizingEnabled: editor.visibility !== Window.Maximized
        window: editor
    }
}
