import QtQuick
import QtQuick.Controls

Window {
    id: files

    property var fileBrowserController
    property var state
    property var targetScreen
    property int panelHeight: 44
    property int desktopMargin: 24
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property int minimumSurfaceWidth: 620
    property int minimumSurfaceHeight: 460
    property bool dragging: false
    property point dragOrigin: Qt.point(0, 0)
    property point windowOrigin: Qt.point(0, 0)
    property point resizeOrigin: Qt.point(0, 0)
    property size resizeSize: Qt.size(0, 0)
    property color surfaceBackground: state && state.darkMode ? "#171a21" : "#f4f6fb"
    property color surfaceForeground: state && state.darkMode ? "#f5f7fb" : "#1e2430"
    property color surfaceMuted: state && state.darkMode ? "#a9b1c2" : "#637083"
    property color surfaceAccent: state && state.darkMode ? "#79b8ff" : "#1769aa"
    property color surfaceRaised: state && state.darkMode ? "#252b36" : "#e8edf5"
    property string selectedPath: fileList.currentItem && fileList.currentItem.modelData
        ? fileList.currentItem.modelData.path : ""
    property string selectedName: fileList.currentItem && fileList.currentItem.modelData
        ? fileList.currentItem.modelData.name : ""
    property bool hasSelection: files.selectedPath.length > 0

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.NonModal
    title: "Northstar Files"

    minimumWidth: files.minimumSurfaceWidth
    minimumHeight: files.minimumSurfaceHeight
    width: Math.min(920, Math.max(files.minimumSurfaceWidth, files.screenWidth - (files.desktopMargin * 2)))
    height: Math.min(680, Math.max(files.minimumSurfaceHeight, files.screenHeight - files.panelHeight - (files.desktopMargin * 2)))
    x: files.screenX + Math.max(files.desktopMargin, (files.screenWidth - files.width) / 2)
    y: files.screenY + files.panelHeight + Math.max(files.desktopMargin, (files.screenHeight - files.panelHeight - files.height) / 2)

    function openBrowser() {
        if (!files.fileBrowserController) {
            return
        }
        files.fileBrowserController.refresh()
        show()
        raise()
        requestActivate()
    }

    function openNameDialog(mode) {
        if (!files.fileBrowserController || (mode === "rename" && !files.hasSelection)) {
            return
        }
        nameDialog.mode = mode
        nameDialog.originalPath = mode === "rename" ? files.selectedPath : ""
        nameField.text = mode === "rename" ? files.selectedName : ""
        nameDialog.open()
    }

    function openTrashDialog() {
        if (!files.fileBrowserController || !files.hasSelection) {
            return
        }
        trashDialog.itemPath = files.selectedPath
        trashDialog.itemName = files.selectedName
        trashDialog.open()
    }

    function beginDrag(mouseX, mouseY) {
        files.dragging = true
        files.dragOrigin = Qt.point(mouseX, mouseY)
        files.windowOrigin = Qt.point(files.x, files.y)
    }

    function updateDrag(mouseX, mouseY) {
        if (!files.dragging) {
            return
        }
        const deltaX = mouseX - files.dragOrigin.x
        const deltaY = mouseY - files.dragOrigin.y
        const maxX = files.screenX + files.screenWidth - files.width
        const maxY = files.screenY + files.screenHeight - files.height
        files.x = Math.max(files.screenX, Math.min(maxX, files.windowOrigin.x + deltaX))
        files.y = Math.max(files.screenY + files.panelHeight, Math.min(maxY, files.windowOrigin.y + deltaY))
    }

    function endDrag() {
        files.dragging = false
    }

    function beginResize(mouseX, mouseY) {
        files.resizeOrigin = Qt.point(mouseX, mouseY)
        files.resizeSize = Qt.size(files.width, files.height)
    }

    function updateResize(mouseX, mouseY) {
        const deltaX = mouseX - files.resizeOrigin.x
        const deltaY = mouseY - files.resizeOrigin.y
        files.width = Math.min(files.screenX + files.screenWidth - files.x,
                               Math.max(files.minimumSurfaceWidth, files.resizeSize.width + deltaX))
        files.height = Math.min(files.screenY + files.screenHeight - files.y,
                                Math.max(files.minimumSurfaceHeight, files.resizeSize.height + deltaY))
    }

    Rectangle {
        anchors.fill: parent
        color: files.surfaceBackground
        border.color: files.surfaceAccent
        border.width: 1
        radius: 12

        Column {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Item {
                id: titleBar
                height: 44
                width: parent.width

                MouseArea {
                    anchors.fill: parent
                    onPressed: files.beginDrag(mouse.x, mouse.y)
                    onPositionChanged: files.updateDrag(mouse.x, mouse.y)
                    onReleased: files.endDrag()
                    onCanceled: files.endDrag()
                }

                Column {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        color: files.surfaceForeground
                        font.bold: true
                        font.pixelSize: 22
                        text: "Files"
                    }

                    Text {
                        color: files.surfaceMuted
                        font.pixelSize: 12
                        text: "Browse your Northstar home folder"
                    }
                }

                Rectangle {
                    id: closeButton
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    color: closeMouse.containsMouse ? "#c34f65" : files.surfaceRaised
                    height: 32
                    radius: 5
                    width: 66

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Close"
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: files.hide()
                    }
                }
            }

            Row {
                spacing: 8
                width: parent.width

                Rectangle {
                    color: backMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    radius: 5
                    width: 70

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Up"
                    }

                    MouseArea {
                        id: backMouse
                        anchors.fill: parent
                        enabled: !!files.fileBrowserController && files.fileBrowserController.currentPath !== files.fileBrowserController.homePath
                        hoverEnabled: true
                        onClicked: files.fileBrowserController.navigateUp()
                    }
                }

                Rectangle {
                    color: homeMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    radius: 5
                    width: 78

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Home"
                    }

                    MouseArea {
                        id: homeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: files.fileBrowserController.goHome()
                    }
                }

                Rectangle {
                    color: refreshMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    radius: 5
                    width: 86

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Refresh"
                    }

                    MouseArea {
                        id: refreshMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: files.fileBrowserController.refresh()
                    }
                }

                Rectangle {
                    color: newFolderMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    radius: 5
                    width: 96

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "New Folder"
                    }

                    MouseArea {
                        id: newFolderMouse
                        anchors.fill: parent
                        enabled: !!files.fileBrowserController
                        hoverEnabled: true
                        onClicked: files.openNameDialog("create")
                    }
                }

                Rectangle {
                    color: files.hasSelection && renameMouse.containsMouse
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.hasSelection ? 1 : 0.55
                    radius: 5
                    width: 76

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Rename"
                    }

                    MouseArea {
                        id: renameMouse
                        anchors.fill: parent
                        enabled: files.hasSelection
                        hoverEnabled: true
                        onClicked: files.openNameDialog("rename")
                    }
                }

                Rectangle {
                    color: files.hasSelection && trashMouse.containsMouse
                        ? "#c34f65" : files.surfaceRaised
                    height: 34
                    opacity: files.hasSelection ? 1 : 0.55
                    radius: 5
                    width: 84

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Trash"
                    }

                    MouseArea {
                        id: trashMouse
                        anchors.fill: parent
                        enabled: files.hasSelection
                        hoverEnabled: true
                        onClicked: files.openTrashDialog()
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: files.surfaceMuted
                    elide: Text.ElideMiddle
                    font.pixelSize: 13
                    text: files.fileBrowserController ? files.fileBrowserController.displayPath : "~"
                    width: Math.max(32, parent.width - 530)
                }
            }

            Rectangle {
                color: files.surfaceBackground
                border.color: files.surfaceMuted
                border.width: 1
                height: parent.height - titleBar.height - 72
                radius: 8
                width: parent.width

                ListView {
                    id: fileList
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    model: files.fileBrowserController ? files.fileBrowserController.entries : []
                    spacing: 4

                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        property bool selected: fileList.currentIndex === index

                        color: selected || fileMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                        height: 48
                        radius: 6
                        width: fileList.width

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 12

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                color: files.surfaceForeground
                                font.bold: true
                                font.pixelSize: 11
                                text: modelData.isDirectory ? "DIR" : "FILE"
                                width: 34
                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                width: parent.width - 46

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
                                    text: modelData.kind + "  ·  " + modelData.modified
                                    width: parent.width
                                }
                            }
                        }

                        MouseArea {
                            id: fileMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: fileList.currentIndex = index
                            onDoubleClicked: files.fileBrowserController.openEntry(modelData.path)
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }

                Text {
                    anchors.centerIn: parent
                    color: files.surfaceMuted
                    font.pixelSize: 13
                    text: "This folder is empty"
                    visible: fileList.count === 0 && (!files.fileBrowserController || files.fileBrowserController.errorMessage.length === 0)
                }
            }

            Text {
                color: files.fileBrowserController && files.fileBrowserController.errorMessage.length > 0
                    ? "#c34f65" : files.surfaceMuted
                elide: Text.ElideRight
                font.pixelSize: 12
                text: files.fileBrowserController && files.fileBrowserController.errorMessage.length > 0
                    ? files.fileBrowserController.errorMessage
                    : "Double-click a folder to browse it or a file to open it."
                width: parent.width
            }
        }
    }

    Dialog {
        id: nameDialog
        property string mode: "create"
        property string originalPath: ""

        title: mode === "rename" ? "Rename item" : "Create folder"
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: Math.min(420, files.width - 48)
        x: (files.width - width) / 2
        y: (files.height - height) / 2

        background: Rectangle {
            color: files.surfaceBackground
            border.color: files.surfaceAccent
            border.width: 1
            radius: 8
        }

        contentItem: Column {
            spacing: 10
            width: nameDialog.width - (2 * nameDialog.padding)

            Text {
                color: files.surfaceForeground
                text: nameDialog.mode === "rename"
                    ? "Choose a new name for the selected item."
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
                ? files.fileBrowserController.renameEntry(originalPath, nameField.text)
                : files.fileBrowserController.createFolder(nameField.text)
            if (succeeded) {
                fileList.currentIndex = -1
            } else {
                Qt.callLater(function() { nameDialog.open() })
            }
        }
    }

    Dialog {
        id: trashDialog
        property string itemPath: ""
        property string itemName: ""

        title: "Move to Trash?"
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: Math.min(420, files.width - 48)
        x: (files.width - width) / 2
        y: (files.height - height) / 2

        background: Rectangle {
            color: files.surfaceBackground
            border.color: files.surfaceAccent
            border.width: 1
            radius: 8
        }

        contentItem: Text {
            color: files.surfaceForeground
            text: "Move \"" + trashDialog.itemName + "\" to the Northstar Trash?"
            wrapMode: Text.WordWrap
            width: trashDialog.width - (2 * trashDialog.padding)
        }

        onAccepted: {
            if (files.fileBrowserController.moveToTrash(itemPath)) {
                fileList.currentIndex = -1
            } else {
                Qt.callLater(function() { trashDialog.open() })
            }
        }
    }

    Rectangle {
        id: resizeHandle
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: files.surfaceAccent
        height: 18
        opacity: 0.85
        width: 18
        z: 10

        Text {
            anchors.centerIn: parent
            color: files.surfaceBackground
            font.pixelSize: 12
            rotation: 45
            text: "..."
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            onPressed: files.beginResize(mouse.x, mouse.y)
            onPositionChanged: files.updateResize(mouse.x, mouse.y)
        }
    }
}
