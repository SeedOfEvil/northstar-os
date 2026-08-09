import QtQuick
import QtQuick.Controls

Window {
    id: desktopBackground

    property url logoSource: northstarLogoSource
    property var state: shellState
    property var desktopItems: northstarDesktopItemsController
    property var fileBrowserController: northstarFileBrowserController
    property var layoutController: northstarDesktopLayoutController
    property var fileBrowserWindow: null
    property var targetScreen
    property int displayIndex: 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property int panelHeight: 44
    property int dockHeight: 72
    property int desktopMargin: 18
    property string selectedPath: ""
    property int layoutCellWidth: 112
    property int layoutCellHeight: 112
    property color surfaceBackground: state && state.darkMode ? "#0f1218" : "#e8edf5"
    property color surfaceForeground: state && state.darkMode ? "#f5f7fb" : "#1e2430"
    property color surfaceMuted: state && state.darkMode ? "#a9b1c2" : "#637083"
    property color surfaceAccent: state && state.darkMode ? "#79b8ff" : "#1769aa"
    property color surfaceRaised: state && state.darkMode ? "#252b36" : "#f4f6fb"

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool
    title: "Northstar Desktop Background"

    function iconNameFor(entry) {
        if (!entry) {
            return "file"
        }
        if (entry.isLaunchable) {
            return "applications"
        }
        return entry.isDirectory ? "folder" : "file"
    }

    function selectEntry(entry) {
        desktopBackground.selectedPath = entry ? entry.path : ""
    }

    function refreshDesktop() {
        if (desktopBackground.fileBrowserController) {
            desktopBackground.fileBrowserController.refresh()
        }
        if (desktopBackground.desktopItems) {
            desktopBackground.desktopItems.refresh()
        }
    }

    function openSelectedEntry() {
        if (!desktopBackground.fileBrowserController
                || desktopBackground.selectedPath.length === 0) {
            return
        }
        const entries = desktopBackground.fileBrowserController.desktopEntries
        for (let entryIndex = 0; entryIndex < entries.length; ++entryIndex) {
            if (entries[entryIndex].path === desktopBackground.selectedPath) {
                desktopBackground.openDesktopEntry(entries[entryIndex])
                return
            }
        }
    }

    function openEntry(entry) {
        if (!entry) {
            return
        }
        if (desktopBackground.fileBrowserWindow
                && desktopBackground.fileBrowserWindow.openDesktopEntry) {
            desktopBackground.fileBrowserWindow.openDesktopEntry(
                entry.path, entry.isDirectory, entry.isLaunchable)
            return
        }
        if (desktopBackground.desktopItems) {
            desktopBackground.desktopItems.requestOpen(entry.path)
        }
    }

    function openWithEntry(entry) {
        if (!entry) {
            return
        }
        if (desktopBackground.fileBrowserWindow
                && desktopBackground.fileBrowserWindow.openAssociationForPath) {
            desktopBackground.fileBrowserWindow.openAssociationForPath(entry.path)
            return
        }
        if (desktopBackground.desktopItems) {
            desktopBackground.desktopItems.requestOpenWith(entry.path)
        }
    }

    function beginRename(entry) {
        if (!entry || !desktopBackground.fileBrowserController) {
            return
        }
        renameDialog.itemPath = entry.path
        renameDialog.itemName = entry.name
        renameField.text = entry.name
        renameDialog.open()
    }

    function beginDelete(entry) {
        if (!entry || !desktopBackground.fileBrowserController) {
            return
        }
        deleteDialog.itemPath = entry.path
        deleteDialog.itemName = entry.name
        deleteDialog.open()
    }

    function showProperties(entry) {
        if (!entry) {
            return
        }
        propertiesDialog.item = entry
        propertiesDialog.open()
    }

    function formattedSize(bytes) {
        if (!bytes || bytes < 1024) {
            return (bytes || 0) + " B"
        }
        if (bytes < 1024 * 1024) {
            return (bytes / 1024).toFixed(1) + " KB"
        }
        return (bytes / (1024 * 1024)).toFixed(1) + " MB"
    }

    Connections {
        target: desktopBackground.fileBrowserController

        function onDesktopEntriesChanged() {
            if (!desktopBackground.fileBrowserController) {
                desktopBackground.selectedPath = ""
                return
            }
            const entries = desktopBackground.fileBrowserController.desktopEntries
            const stillExists = entries.some(function(entry) {
                return entry.path === desktopBackground.selectedPath
            })
            if (!stillExists) {
                desktopBackground.selectedPath = ""
            }
        }
    }
    function openDesktopEntry(entry) {
        if (!entry || !desktopBackground.fileBrowserWindow
                || !desktopBackground.fileBrowserWindow.openDesktopEntry) {
            return
        }
        desktopBackground.fileBrowserWindow.openDesktopEntry(
            entry.path, entry.isDirectory, entry.isLaunchable)
    }

    function nearestFreePosition(icon, candidateX, candidateY) {
        const maximumX = Math.max(0, desktopIconSurface.width - icon.width)
        const maximumY = Math.max(0, desktopIconSurface.height - icon.height)
        const maximumColumn = Math.max(0, Math.floor(maximumX / desktopBackground.layoutCellWidth))
        const maximumRow = Math.max(0, Math.floor(maximumY / desktopBackground.layoutCellHeight))
        const requestedColumn = Math.max(0, Math.min(
            maximumColumn, Math.round(candidateX / desktopBackground.layoutCellWidth)))
        const requestedRow = Math.max(0, Math.min(
            maximumRow, Math.round(candidateY / desktopBackground.layoutCellHeight)))
        const occupied = function(x, y) {
            for (let itemIndex = 0; itemIndex < desktopRepeater.count; ++itemIndex) {
                const other = desktopRepeater.itemAt(itemIndex)
                if (!other || other === icon) {
                    continue
                }
                if (other.itemX < x + icon.width && x < other.itemX + other.width
                        && other.itemY < y + icon.height && y < other.itemY + other.height) {
                    return true
                }
            }
            return false
        }

        let bestX = Math.max(0, Math.min(maximumX, candidateX))
        let bestY = Math.max(0, Math.min(maximumY, candidateY))
        let bestDistance = Number.MAX_VALUE
        for (let row = 0; row <= maximumRow; ++row) {
            for (let column = 0; column <= maximumColumn; ++column) {
                const x = Math.min(maximumX, column * desktopBackground.layoutCellWidth)
                const y = Math.min(maximumY, row * desktopBackground.layoutCellHeight)
                if (occupied(x, y)) {
                    continue
                }
                const distance = Math.pow(column - requestedColumn, 2)
                    + Math.pow(row - requestedRow, 2)
                if (distance < bestDistance) {
                    bestDistance = distance
                    bestX = x
                    bestY = y
                }
            }
        }
        return Qt.point(bestX, bestY)
    }

    Rectangle {
        anchors.fill: parent
        color: desktopBackground.surfaceBackground

        Image {
            anchors.centerIn: parent
            fillMode: Image.PreserveAspectFit
            height: width
            mipmap: true
            opacity: desktopBackground.state && desktopBackground.state.darkMode ? 0.28 : 0.18
            smooth: true
            source: desktopBackground.logoSource
            width: Math.min(460, desktopBackground.screenWidth * 0.42)
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            z: 1
            onClicked: function(mouse) {
                desktopKeyboardSurface.forceActiveFocus()
                desktopBackground.selectedPath = ""
                if (mouse.button === Qt.RightButton) {
                    desktopMenu.item = null
                    desktopMenu.popup()
                }
            }
        }

        Item {
            id: desktopKeyboardSurface
            anchors.fill: parent
            activeFocusOnTab: true
            focus: true
            z: 0

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    desktopBackground.openSelectedEntry()
                    event.accepted = true
                }
            }

            Component.onCompleted: forceActiveFocus()
        }
        Item {
            id: desktopIconSurface
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 94
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.top: parent.top
            anchors.topMargin: 62
            clip: true
            visible: desktopBackground.displayIndex === 0
            z: 2

            Repeater {
                id: desktopRepeater
                model: desktopBackground.fileBrowserController
                    ? desktopBackground.fileBrowserController.desktopEntries : []
                delegate: Rectangle {
                    id: desktopIcon
                    property bool dragging: false
                    property real itemX: 0
                    property real itemY: index * desktopBackground.layoutCellHeight
                    property point dragOrigin: Qt.point(0, 0)
                    property point itemOrigin: Qt.point(0, 0)

                    color: desktopBackground.selectedPath === modelData.path
                        ? (desktopBackground.state && desktopBackground.state.darkMode
                            ? "#3b5f89" : "#c9e1ff")
                        : (desktopItemMouse.containsMouse ? "#4079b8" : "transparent")
                    border.color: desktopBackground.selectedPath === modelData.path
                        ? desktopBackground.surfaceAccent : "transparent"
                    border.width: 1
                    height: 104
                    radius: 8
                    width: 104
                    x: desktopIcon.itemX
                    y: desktopIcon.itemY

                    function applySavedPosition() {
                        desktopIcon.itemX = 0
                        desktopIcon.itemY = index * desktopBackground.layoutCellHeight
                        if (!desktopBackground.layoutController) {
                            return
                        }
                        const saved = desktopBackground.layoutController.positionFor(modelData.path)
                        if (saved && saved.x !== undefined && saved.y !== undefined) {
                            desktopIcon.itemX = Math.max(0, Math.min(desktopIconSurface.width - desktopIcon.width, saved.x))
                            desktopIcon.itemY = Math.max(0, Math.min(desktopIconSurface.height - desktopIcon.height, saved.y))
                        }
                    }

                    Component.onCompleted: applySavedPosition()

                    Connections {
                        target: desktopBackground.layoutController
                        function onPositionsChanged() {
                            desktopIcon.applySavedPosition()
                        }
                    }

                NorthstarIcon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 10
                    height: 52
                    width: 52
                    iconName: desktopBackground.iconNameFor(modelData)
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 66
                    color: desktopBackground.surfaceForeground
                    elide: Text.ElideMiddle
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    maximumLineCount: 2
                    text: modelData.name
                    width: parent.width - 12
                    wrapMode: Text.Wrap
                }

                MouseArea {
                    id: desktopItemMouse
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true

                    onPressed: function(mouse) {
                        desktopIcon.dragging = mouse.button === Qt.LeftButton
                        if (!desktopIcon.dragging) {
                            return
                        }
                        desktopIcon.dragOrigin = Qt.point(mouse.x, mouse.y)
                        desktopIcon.itemOrigin = Qt.point(desktopIcon.itemX, desktopIcon.itemY)
                    }

                    onPositionChanged: function(mouse) {
                        if (!desktopIcon.dragging) {
                            return
                        }
                        const pointer = desktopItemMouse.mapToItem(
                            desktopIconSurface, mouse.x, mouse.y)
                        desktopIcon.itemX = Math.max(0, Math.min(
                            desktopIconSurface.width - desktopIcon.width,
                            pointer.x - desktopIcon.dragOrigin.x))
                        desktopIcon.itemY = Math.max(0, Math.min(
                            desktopIconSurface.height - desktopIcon.height,
                            pointer.y - desktopIcon.dragOrigin.y))
                    }

                    onReleased: function(mouse) {
                        if (!desktopIcon.dragging) {
                            return
                        }
                        desktopIcon.dragging = false
                        const snapped = desktopBackground.nearestFreePosition(
                            desktopIcon, desktopIcon.itemX, desktopIcon.itemY)
                        desktopIcon.itemX = snapped.x
                        desktopIcon.itemY = snapped.y
                        if (desktopBackground.layoutController) {
                            desktopBackground.layoutController.setPosition(
                                modelData.path, desktopIcon.itemX, desktopIcon.itemY)
                        }
                    }

                    onClicked: function(mouse) {
                        desktopKeyboardSurface.forceActiveFocus()
                        desktopBackground.selectEntry(modelData)
                        if (mouse.button === Qt.RightButton) {
                            desktopMenu.item = modelData
                            desktopMenu.popup()
                        }
                    }

                    onDoubleClicked: desktopBackground.openDesktopEntry(modelData)
                }
            }
        }
        }

        Text {
            anchors.centerIn: desktopIconSurface
            color: desktopBackground.surfaceMuted
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            text: desktopBackground.fileBrowserController
                && desktopBackground.fileBrowserController.errorMessage.length > 0
                ? desktopBackground.fileBrowserController.errorMessage
                : "Your Desktop is empty"
            visible: desktopBackground.fileBrowserController
                && desktopBackground.fileBrowserController.desktopEntries.length === 0
            width: Math.min(360, desktopIconSurface.width)
            wrapMode: Text.WordWrap
        }
    }

    Menu {
        id: desktopMenu
        property var item: null

        MenuItem {
            text: "Open"
            enabled: !!desktopMenu.item
            onTriggered: desktopBackground.openEntry(desktopMenu.item)
        }

        MenuItem {
            text: "Open With"
            enabled: !!desktopMenu.item && !desktopMenu.item.isDirectory
            onTriggered: desktopBackground.openWithEntry(desktopMenu.item)
        }

        MenuSeparator {}

        MenuItem {
            text: "Rename"
            enabled: !!desktopMenu.item && !!desktopBackground.fileBrowserController
            onTriggered: desktopBackground.beginRename(desktopMenu.item)
        }

        MenuItem {
            text: "Move to Trash"
            enabled: !!desktopMenu.item && !!desktopBackground.fileBrowserController
            onTriggered: desktopBackground.beginDelete(desktopMenu.item)
        }

        MenuSeparator {}

        MenuItem {
            text: "Properties"
            enabled: !!desktopMenu.item
            onTriggered: desktopBackground.showProperties(desktopMenu.item)
        }

        MenuSeparator {}

        MenuItem {
            text: "Refresh Desktop"
            enabled: !!desktopBackground.fileBrowserController || !!desktopBackground.desktopItems
            onTriggered: desktopBackground.refreshDesktop()
        }
    }

    Dialog {
        id: renameDialog
        property string itemPath: ""
        property string itemName: ""

        title: "Rename Desktop item"
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: Math.min(420, desktopBackground.screenWidth - 48)
        x: (desktopBackground.width - width) / 2
        y: (desktopBackground.height - height) / 2

        background: Rectangle {
            color: desktopBackground.surfaceRaised
            border.color: desktopBackground.surfaceAccent
            border.width: 1
            radius: 8
        }

        contentItem: Column {
            spacing: 10
            width: renameDialog.width - (2 * renameDialog.padding)

            Text {
                color: desktopBackground.surfaceForeground
                text: "Choose a new name for the Desktop item."
                wrapMode: Text.WordWrap
                width: parent.width
            }

            TextField {
                id: renameField
                width: parent.width
                selectByMouse: true
                onAccepted: renameDialog.accept()
            }
        }

        onOpened: {
            renameField.forceActiveFocus()
            renameField.selectAll()
        }

        onAccepted: {
            if (desktopBackground.fileBrowserController.renameEntry(itemPath, renameField.text)) {
                desktopBackground.selectedPath = ""
                desktopBackground.fileBrowserController.refresh()
            } else {
                Qt.callLater(function() { renameDialog.open() })
            }
        }
    }

    Dialog {
        id: deleteDialog
        property string itemPath: ""
        property string itemName: ""

        title: "Move Desktop item to Trash?"
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: Math.min(440, desktopBackground.screenWidth - 48)
        x: (desktopBackground.width - width) / 2
        y: (desktopBackground.height - height) / 2

        background: Rectangle {
            color: desktopBackground.surfaceRaised
            border.color: desktopBackground.surfaceAccent
            border.width: 1
            radius: 8
        }

        contentItem: Text {
            color: desktopBackground.surfaceForeground
            text: "Move \"" + deleteDialog.itemName
                + "\" to Northstar Trash? You can restore it later."
            wrapMode: Text.WordWrap
            width: deleteDialog.width - (2 * deleteDialog.padding)
        }

        onAccepted: {
            if (desktopBackground.fileBrowserController.moveToTrash(itemPath)) {
                desktopBackground.selectedPath = ""
                desktopBackground.fileBrowserController.refresh()
            } else {
                Qt.callLater(function() { deleteDialog.open() })
            }
        }
    }

    Dialog {
        id: propertiesDialog
        property var item: null

        title: item ? item.name : "Properties"
        modal: true
        padding: 16
        standardButtons: Dialog.Ok
        width: Math.min(460, desktopBackground.screenWidth - 48)
        x: (desktopBackground.width - width) / 2
        y: (desktopBackground.height - height) / 2

        background: Rectangle {
            color: desktopBackground.surfaceRaised
            border.color: desktopBackground.surfaceAccent
            border.width: 1
            radius: 8
        }

        contentItem: Column {
            spacing: 8
            width: propertiesDialog.width - (2 * propertiesDialog.padding)

            Text {
                color: desktopBackground.surfaceForeground
                text: propertiesDialog.item
                    ? "Kind: " + propertiesDialog.item.kind : ""
            }

            Text {
                color: desktopBackground.surfaceForeground
                text: propertiesDialog.item
                    ? "Size: " + desktopBackground.formattedSize(propertiesDialog.item.size) : ""
            }

            Text {
                color: desktopBackground.surfaceMuted
                text: propertiesDialog.item
                    ? "Modified: " + propertiesDialog.item.modified : ""
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Text {
                color: desktopBackground.surfaceMuted
                text: propertiesDialog.item ? propertiesDialog.item.path : ""
                wrapMode: Text.WrapAnywhere
                width: parent.width
            }
        }
    }
}
