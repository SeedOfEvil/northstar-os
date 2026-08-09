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

    color: editor.backgroundColor
    height: 680
    minimumHeight: 420
    minimumWidth: 620
    title: editor.documentController && editor.documentController.filePath.length > 0
        ? "Northstar Text Editor — " + editor.documentController.filePath.split("/").pop()
        : "Northstar Text Editor"
    visible: true
    width: 920

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
                source: "qrc:/qt/qml/Northstar/TextEditor/northstar-text-editor.svg"
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
                text: "Save"
                onClicked: editor.documentController.save()
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
