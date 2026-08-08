import QtQuick
import QtQuick.Controls

Window {
    id: files

    property var fileBrowserController
    property var applicationLauncher
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
    property bool gridView: true
    property int selectedIndex: -1
    property var selectedEntry: files.fileBrowserController
        && files.selectedIndex >= 0
        && files.selectedIndex < files.fileBrowserController.entries.length
        ? files.fileBrowserController.entries[files.selectedIndex] : null
    property string selectedPath: files.selectedEntry ? files.selectedEntry.path : ""
    property string selectedName: files.selectedEntry ? files.selectedEntry.name : ""
    property bool hasSelection: !!files.selectedEntry && files.selectedPath.length > 0
    property bool showingTrash: files.fileBrowserController && files.fileBrowserController.showingTrash

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.NonModal
    title: "Northstar Files"

    Connections {
        target: files.fileBrowserController

        function onSearchQueryChanged() {
            if (searchField.text !== files.fileBrowserController.searchQuery) {
                searchField.text = files.fileBrowserController.searchQuery
            }
        }
    }

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
        files.clearSelection()
        files.fileBrowserController.setSearchQuery("")
        searchField.text = ""
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

    function openSelectedEntry() {
        if (!files.fileBrowserController || files.showingTrash || !files.hasSelection) {
            return
        }
        const entry = files.selectedEntry
        if (entry.isDirectory) {
            if (files.fileBrowserController.openEntry(files.selectedPath)) {
                files.clearSelection()
            }
            return
        }
        files.openAssociationDialog()
    }

    function openAssociationDialog() {
        if (!files.hasSelection || files.selectedEntry.isDirectory) {
            return
        }
        associationDialog.itemPath = files.selectedPath
        associationDialog.itemName = files.selectedName
        if (files.applicationLauncher) {
            files.applicationLauncher.setApplicationQuery("")
        }
        associationDialog.open()
    }

    function launchSelectedFile(desktopId) {
        if (!files.applicationLauncher || !associationDialog.itemPath) {
            return
        }
        if (files.applicationLauncher.launchApplicationWithFile(desktopId, associationDialog.itemPath)) {
            associationDialog.close()
        }
    }

    function clearSelection() {
        files.selectedIndex = -1
    }

    function openTrashDialog() {
        if (!files.fileBrowserController || files.showingTrash || !files.hasSelection) {
            return
        }
        trashDialog.itemPath = files.selectedPath
        trashDialog.itemName = files.selectedName
        trashDialog.open()
    }

    function openTrash() {
        if (!files.fileBrowserController || !files.fileBrowserController.showTrash()) {
            return
        }
        files.clearSelection()
        show()
        raise()
        requestActivate()
    }

    function openRestoreDialog() {
        if (!files.fileBrowserController || !files.showingTrash || !files.hasSelection) {
            return
        }
        restoreDialog.itemPath = files.selectedPath
        restoreDialog.itemName = files.selectedName
        restoreDialog.originalLocation = files.fileListOriginalLocation()
        restoreDialog.open()
    }

    function fileListOriginalLocation() {
        if (!files.selectedEntry) {
            return ""
        }
        return files.selectedEntry.originalLocation || "the original location"
    }

    function entrySummary(entry) {
        if (!entry) {
            return ""
        }
        return files.showingTrash
            ? entry.kind + " - " + (entry.originalLocation || "Original location unavailable")
            : files.fileBrowserController && files.fileBrowserController.searching
                ? entry.kind + " - " + (entry.searchLocation || "Home folder")
            : entry.kind + " - " + entry.modified
    }

    function applicationIconName(application) {
        if (!application) {
            return "northstar"
        }
        const descriptor = ((application.name || "") + " "
            + (application.genericName || "") + " "
            + (application.desktopId || "")).toLowerCase()
        if (descriptor.indexOf("terminal") >= 0 || descriptor.indexOf("shell") >= 0
                || descriptor.indexOf("console") >= 0) {
            return "terminal"
        }
        if (descriptor.indexOf("firefox") >= 0 || descriptor.indexOf("browser") >= 0
                || descriptor.indexOf("web") >= 0) {
            return "browser"
        }
        if (descriptor.indexOf("setting") >= 0 || descriptor.indexOf("preference") >= 0
                || descriptor.indexOf("config") >= 0) {
            return "settings"
        }
        if (descriptor.indexOf("file") >= 0 || descriptor.indexOf("folder") >= 0
                || descriptor.indexOf("manager") >= 0) {
            return "files"
        }
        if (descriptor.indexOf("text") >= 0 || descriptor.indexOf("editor") >= 0
                || descriptor.indexOf("note") >= 0) {
            return "editor"
        }
        return "northstar"
    }

    function openEmptyTrashDialog() {
        if (!files.fileBrowserController || !files.showingTrash) {
            return
        }
        emptyTrashDialog.open()
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
                        text: files.showingTrash
                            ? "Review and restore deleted items"
                            : files.fileBrowserController && files.fileBrowserController.searching
                                ? "Search results from your Northstar home folder"
                            : "Browse your Northstar home folder"
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
                id: navigationRow
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
                        enabled: !!files.fileBrowserController
                            && !files.showingTrash
                            && !files.fileBrowserController.searching
                            && files.fileBrowserController.currentPath !== files.fileBrowserController.homePath
                        hoverEnabled: true
                        onClicked: {
                            if (files.fileBrowserController.navigateUp()) {
                                files.clearSelection()
                            }
                        }
                    }
                }

                Rectangle {
                    color: homeMouse.containsMouse || !files.showingTrash && files.fileBrowserController
                        && files.fileBrowserController.currentPath === files.fileBrowserController.homePath
                        ? files.surfaceAccent : files.surfaceRaised
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
                        onClicked: {
                            if (files.fileBrowserController.goHome()) {
                                files.clearSelection()
                            }
                        }
                    }
                }

                Rectangle {
                    color: trashLocationMouse.containsMouse || files.showingTrash
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    radius: 5
                    width: 72

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Trash"
                    }

                    MouseArea {
                        id: trashLocationMouse
                        anchors.fill: parent
                        enabled: !!files.fileBrowserController
                        hoverEnabled: true
                        onClicked: files.openTrash()
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
                        onClicked: {
                            files.clearSelection()
                            files.fileBrowserController.refresh()
                        }
                    }
                }

                Rectangle {
                    color: files.gridView || tilesMouse.containsMouse
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    radius: 5
                    width: 60

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.bold: files.gridView
                        font.pixelSize: 12
                        text: "Tiles"
                    }

                    MouseArea {
                        id: tilesMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: files.gridView = true
                    }
                }

                Rectangle {
                    color: !files.gridView || listViewMouse.containsMouse
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    radius: 5
                    width: 58

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.bold: !files.gridView
                        font.pixelSize: 12
                        text: "List"
                    }

                    MouseArea {
                        id: listViewMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: files.gridView = false
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: files.surfaceMuted
                    elide: Text.ElideMiddle
                    font.pixelSize: 13
                    text: files.fileBrowserController ? files.fileBrowserController.displayPath : "~"
                    width: Math.max(80, parent.width - 464)
                }
            }

            Row {
                id: searchRow
                spacing: 8
                width: parent.width

                TextField {
                    id: searchField
                    background: Rectangle {
                        color: files.surfaceBackground
                        border.color: files.surfaceAccent
                        border.width: 1
                        radius: 6
                    }
                    color: files.surfaceForeground
                    enabled: !!files.fileBrowserController && !files.showingTrash
                    height: 36
                    placeholderText: "Search the Northstar home folder"
                    placeholderTextColor: files.surfaceMuted
                    selectByMouse: true
                    width: parent.width - clearSearchButton.width - searchHint.implicitWidth - (2 * parent.spacing)

                    onTextChanged: {
                        if (files.fileBrowserController
                                && text !== files.fileBrowserController.searchQuery) {
                            files.fileBrowserController.setSearchQuery(text)
                        }
                    }
                }

                Rectangle {
                    id: clearSearchButton
                    color: clearSearchMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 36
                    opacity: searchField.text.length > 0 ? 1 : 0.55
                    radius: 6
                    width: 62

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Clear"
                    }

                    MouseArea {
                        id: clearSearchMouse
                        anchors.fill: parent
                        enabled: searchField.text.length > 0
                        hoverEnabled: true
                        onClicked: searchField.text = ""
                    }
                }

                Text {
                    id: searchHint
                    anchors.verticalCenter: parent.verticalCenter
                    color: files.surfaceMuted
                    font.pixelSize: 11
                    text: files.fileBrowserController && files.fileBrowserController.searching
                        ? files.fileBrowserController.entries.length + " result(s)"
                        : "Home search"
                }
            }

            Row {
                id: actionRow
                spacing: 8
                width: parent.width

                Rectangle {
                    color: files.hasSelection && !files.showingTrash && openMouse.containsMouse
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.hasSelection && !files.showingTrash ? 1 : 0.55
                    radius: 5
                    width: 70

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Open"
                    }

                    MouseArea {
                        id: openMouse
                        anchors.fill: parent
                        enabled: files.hasSelection && !files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openSelectedEntry()
                    }
                }

                Rectangle {
                    color: newFileMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.showingTrash ? 0.55 : 1
                    radius: 5
                    width: 82

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "New File"
                    }

                    MouseArea {
                        id: newFileMouse
                        anchors.fill: parent
                        enabled: !!files.fileBrowserController && !files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openNameDialog("file")
                    }
                }

                Rectangle {
                    color: newFolderMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.showingTrash ? 0.55 : 1
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
                        enabled: !!files.fileBrowserController && !files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openNameDialog("create")
                    }
                }

                Rectangle {
                    color: files.hasSelection && !files.showingTrash && renameMouse.containsMouse
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.hasSelection && !files.showingTrash ? 1 : 0.55
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
                        enabled: files.hasSelection && !files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openNameDialog("rename")
                    }
                }

                Rectangle {
                    color: files.hasSelection && !files.showingTrash && deleteMouse.containsMouse
                        ? "#c34f65" : files.surfaceRaised
                    height: 34
                    opacity: files.hasSelection && !files.showingTrash ? 1 : 0.55
                    radius: 5
                    width: 78

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Delete"
                    }

                    MouseArea {
                        id: deleteMouse
                        anchors.fill: parent
                        enabled: files.hasSelection && !files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openTrashDialog()
                    }
                }

                Rectangle {
                    color: files.hasSelection && files.showingTrash && restoreMouse.containsMouse
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.hasSelection && files.showingTrash ? 1 : 0.55
                    radius: 5
                    visible: files.showingTrash
                    width: 86

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Restore"
                    }

                    MouseArea {
                        id: restoreMouse
                        anchors.fill: parent
                        enabled: files.hasSelection && files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openRestoreDialog()
                    }
                }

                Rectangle {
                    color: files.showingTrash && emptyTrashMouse.containsMouse ? "#c34f65" : files.surfaceRaised
                    height: 34
                    opacity: files.showingTrash ? 1 : 0.55
                    radius: 5
                    visible: files.showingTrash
                    width: 100

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Empty Trash"
                    }

                    MouseArea {
                        id: emptyTrashMouse
                        anchors.fill: parent
                        enabled: files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openEmptyTrashDialog()
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: files.surfaceMuted
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    text: files.showingTrash
                        ? "Select an item to restore it."
                        : "Select an item to open, rename, or delete."
                    width: Math.max(80, parent.width - (files.showingTrash ? 214 : 434))
                }
            }

            Rectangle {
                color: files.surfaceBackground
                border.color: files.surfaceMuted
                border.width: 1
                height: Math.max(160, parent.height - titleBar.height - navigationRow.height
                    - searchRow.height - actionRow.height - footerText.implicitHeight - 48)
                radius: 8
                width: parent.width

                GridView {
                    id: fileList
                    anchors.fill: parent
                    anchors.margins: 8
                    cellHeight: files.gridView ? 116 : 58
                    cellWidth: files.gridView ? 176 : width
                    clip: true
                    model: files.fileBrowserController ? files.fileBrowserController.entries : []

                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        property bool selected: files.selectedIndex === index

                        color: selected || fileMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                        height: files.gridView ? 104 : 50
                        radius: 6
                        width: files.gridView ? 160 : fileList.cellWidth - 16

                        Column {
                            id: tileContent
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
                            id: listContent
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
                            onClicked: files.selectedIndex = index
                            onDoubleClicked: {
                                files.selectedIndex = index
                                files.openSelectedEntry()
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
                    visible: fileList.count === 0 && (!files.fileBrowserController || files.fileBrowserController.errorMessage.length === 0)
                }
            }

            Text {
                id: footerText
                color: files.fileBrowserController && files.fileBrowserController.errorMessage.length > 0
                    ? "#c34f65" : files.surfaceMuted
                elide: Text.ElideRight
                font.pixelSize: 12
                text: files.fileBrowserController && files.fileBrowserController.errorMessage.length > 0
                    ? files.fileBrowserController.errorMessage
                    : files.fileBrowserController && files.fileBrowserController.searching
                        ? "Search results are scoped to the Northstar home folder."
                    : "Select an item and choose Open, or double-click it."
                width: parent.width
            }
        }
    }

    Dialog {
        id: associationDialog
        property string itemPath: ""
        property string itemName: ""

        title: "Open with an application"
        modal: true
        padding: 16
        standardButtons: Dialog.NoButton
        width: Math.min(520, files.width - 48)
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
            width: associationDialog.width - (2 * associationDialog.padding)

            Text {
                color: files.surfaceForeground
                text: "Choose how Northstar should open \"" + associationDialog.itemName + "\"."
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Rectangle {
                color: files.surfaceBackground
                border.color: files.surfaceMuted
                border.width: 1
                height: 220
                width: parent.width

                ListView {
                    id: associationList
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    model: files.applicationLauncher ? files.applicationLauncher.applications : []

                    delegate: Rectangle {
                        required property var modelData

                        color: associationMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                        height: 52
                        radius: 6
                        width: associationList.width - 12

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            NorthstarIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                height: 34
                                width: 34
                                iconName: files.applicationIconName(modelData)
                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                width: parent.width - 44

                                Text {
                                    color: files.surfaceForeground
                                    elide: Text.ElideRight
                                    font.pixelSize: 13
                                    text: modelData.name
                                    width: parent.width
                                }

                                Text {
                                    color: files.surfaceMuted
                                    elide: Text.ElideRight
                                    font.pixelSize: 10
                                    text: modelData.genericName || modelData.desktopId
                                    width: parent.width
                                }
                            }
                        }

                        MouseArea {
                            id: associationMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: files.launchSelectedFile(modelData.desktopId)
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }

                Text {
                    anchors.centerIn: parent
                    color: files.surfaceMuted
                    text: "No registered applications were found."
                    visible: associationList.count === 0
                }
            }

            Row {
                spacing: 8
                width: parent.width

                Button {
                    text: "Use System Default"
                    enabled: !!files.fileBrowserController
                    onClicked: {
                        if (files.fileBrowserController.openEntry(associationDialog.itemPath)) {
                            associationDialog.close()
                        }
                    }
                }

                Button {
                    text: "Cancel"
                    onClicked: associationDialog.close()
                }
            }
        }
    }

    Dialog {
        id: nameDialog
        property string mode: "create"
        property string originalPath: ""

        title: mode === "rename" ? "Rename item"
            : mode === "file" ? "Create file" : "Create folder"
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
                ? files.fileBrowserController.renameEntry(originalPath, nameField.text)
                : mode === "file"
                    ? files.fileBrowserController.createFile(nameField.text)
                    : files.fileBrowserController.createFolder(nameField.text)
            if (succeeded) {
                files.clearSelection()
            } else {
                Qt.callLater(function() { nameDialog.open() })
            }
        }
    }

    Dialog {
        id: trashDialog
        property string itemPath: ""
        property string itemName: ""

        title: "Delete item?"
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
            text: "Move \"" + trashDialog.itemName
                + "\" to the Northstar Trash? You can restore it later."
            wrapMode: Text.WordWrap
            width: trashDialog.width - (2 * trashDialog.padding)
        }

        onAccepted: {
            if (files.fileBrowserController.moveToTrash(itemPath)) {
                files.clearSelection()
            } else {
                Qt.callLater(function() { trashDialog.open() })
            }
        }
    }

    Dialog {
        id: restoreDialog
        property string itemPath: ""
        property string itemName: ""
        property string originalLocation: ""

        title: "Restore item?"
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: Math.min(440, files.width - 48)
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
            text: "Restore \"" + restoreDialog.itemName + "\" to "
                + restoreDialog.originalLocation + "?"
            wrapMode: Text.WordWrap
            width: restoreDialog.width - (2 * restoreDialog.padding)
        }

        onAccepted: {
            if (files.fileBrowserController.restoreEntry(itemPath)) {
                files.clearSelection()
            } else {
                Qt.callLater(function() { restoreDialog.open() })
            }
        }
    }

    Dialog {
        id: emptyTrashDialog

        title: "Empty Trash?"
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: Math.min(420, files.width - 48)
        x: (files.width - width) / 2
        y: (files.height - height) / 2

        background: Rectangle {
            color: files.surfaceBackground
            border.color: "#c34f65"
            border.width: 1
            radius: 8
        }

        contentItem: Text {
            color: files.surfaceForeground
            text: "This permanently removes every item currently in the Northstar Trash."
            wrapMode: Text.WordWrap
            width: emptyTrashDialog.width - (2 * emptyTrashDialog.padding)
        }

        onAccepted: {
            if (files.fileBrowserController.emptyTrash()) {
                files.clearSelection()
            } else {
                Qt.callLater(function() { emptyTrashDialog.open() })
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
