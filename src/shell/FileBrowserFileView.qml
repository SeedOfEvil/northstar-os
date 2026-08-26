import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Rectangle {
    id: fileView

    required property var hostWindow
    readonly property var files: hostWindow

    color: files.surfaceBackground
    border.color: files.surfaceMuted
    border.width: 1
    radius: 8

    function forceListFocus() {
        fileList.forceActiveFocus()
    }

    GridView {
        id: fileList
        anchors.fill: parent
        anchors.margins: 8
        cellHeight: files.gridView ? 116 : 58
        cellWidth: files.gridView ? 176 : width
        clip: true
        model: files.fileBrowserController ? files.fileBrowserController.entries : []
        activeFocusOnTab: true
        focus: true

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Space && files.hasSelection) {
                files.previewSelectedEntry()
                event.accepted = true
            }
        }

        delegate: Rectangle {
            required property var modelData
            required property int index
            property bool selected: files.selectedIndex === index
            property string dragUrl: files.localFileUrl(modelData.path)

            Drag.active: fileDragHandler.active
            Drag.hotSpot.x: width / 2
            Drag.hotSpot.y: height / 2
            Drag.mimeData: ({ "text/uri-list": dragUrl })
            Drag.supportedActions: Qt.CopyAction

            color: selected || fileMouse.containsMouse
                ? files.surfaceAccent : files.surfaceRaised
            height: files.gridView ? 104 : 50
            radius: 6
            width: files.gridView ? 160 : fileList.cellWidth - 16

            Column {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6
                visible: files.gridView

                NorthstarIcon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    height: 38
                    width: 52
                    iconName: modelData.isDirectory ? "files" : "editor"
                }

                Text {
                    color: files.surfaceForeground
                    elide: Text.ElideRight
                    font.bold: true
                    font.pixelSize: 13
                    text: modelData.name
                    width: parent.width
                }

                Text {
                    color: files.surfaceMuted
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: files.entrySummary(modelData)
                    width: parent.width
                }
            }

            Row {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 12
                visible: !files.gridView

                NorthstarIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 32
                    width: 32
                    iconName: modelData.isDirectory ? "files" : "editor"
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    width: parent.width - 58

                    Text {
                        color: files.surfaceForeground
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        text: modelData.name
                        width: parent.width
                    }

                    Text {
                        color: files.surfaceMuted
                        font.pixelSize: 11
                        text: files.showingTrash
                            ? modelData.kind + "  ·  "
                                + (modelData.originalLocation || "Original location unavailable")
                            : modelData.kind + "  ·  " + modelData.modified
                        width: parent.width
                    }
                }
            }

            MouseArea {
                id: fileMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    files.selectedIndex = index
                    fileList.forceActiveFocus()
                }
                onDoubleClicked: {
                    files.selectedIndex = index
                    files.openSelectedEntry()
                }
            }

            DragHandler {
                id: fileDragHandler
                acceptedButtons: Qt.LeftButton
                enabled: !files.showingTrash && !modelData.isDirectory
                    && dragUrl.length > 7

                onActiveChanged: {
                    if (active) {
                        files.selectedIndex = index
                    }
                }
            }
        }

        ScrollBar.vertical: ScrollBar {}
    }

    Text {
        anchors.centerIn: parent
        color: files.surfaceMuted
        font.pixelSize: 13
        text: "This folder is empty"
        visible: fileList.count === 0
            && (!files.fileBrowserController
                || files.fileBrowserController.errorMessage.length === 0)
    }
}
