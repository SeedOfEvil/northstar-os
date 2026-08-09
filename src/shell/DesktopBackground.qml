import QtQuick
import QtQuick.Controls

Window {
    id: desktopBackground

    property url logoSource: northstarLogoSource
    property var state: shellState
    property var desktopItems: northstarDesktopItemsController
    property var fileBrowserController: northstarFileBrowserController
    property var targetScreen
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property int panelHeight: 44
    property int dockHeight: 72
    property int desktopMargin: 18
    property string selectedPath: ""
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

    function openEntry(entry) {
        if (desktopBackground.desktopItems && entry) {
            desktopBackground.desktopItems.requestOpen(entry.path)
        }
    }

    function openWithEntry(entry) {
        if (desktopBackground.desktopItems && entry) {
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
        target: desktopBackground.desktopItems

        function onEntriesChanged() {
            if (!desktopBackground.desktopItems) {
                desktopBackground.selectedPath = ""
                return
            }
            const entries = desktopBackground.desktopItems.entries
            const stillExists = entries.some(function(entry) {
                return entry.path === desktopBackground.selectedPath
            })
            if (!stillExists) {
                desktopBackground.selectedPath = ""
            }
        }
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
            acceptedButtons: Qt.LeftButton
            z: 1
            onClicked: desktopBackground.selectedPath = ""
        }

        GridView {
            id: desktopGrid
            anchors.bottomMargin: desktopBackground.dockHeight + desktopBackground.desktopMargin
            anchors.fill: parent
            anchors.leftMargin: desktopBackground.desktopMargin
            anchors.rightMargin: desktopBackground.desktopMargin
            anchors.topMargin: desktopBackground.panelHeight + desktopBackground.desktopMargin
            cellHeight: 102
            cellWidth: 104
            clip: true
            flow: GridView.FlowTopToBottom
            interactive: contentHeight > height || contentWidth > width
            layoutDirection: Qt.LeftToRight
            model: desktopBackground.desktopItems ? desktopBackground.desktopItems.entries : []
            z: 2

            delegate: Item {
                required property var modelData

                height: desktopGrid.cellHeight
                width: desktopGrid.cellWidth

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4
                    color: desktopBackground.selectedPath === modelData.path
                        ? (desktopBackground.state && desktopBackground.state.darkMode
                            ? "#3b5f89" : "#c9e1ff") : "transparent"
                    radius: 10
                    border.color: desktopBackground.selectedPath === modelData.path
                        ? desktopBackground.surfaceAccent : "transparent"
                    border.width: 1
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
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true

                    onClicked: function(mouse) {
                        desktopBackground.selectEntry(modelData)
                        if (mouse.button === Qt.RightButton) {
                            desktopMenu.item = modelData
                            desktopMenu.popup()
                        }
                    }

                    onDoubleClicked: desktopBackground.openEntry(modelData)
                }
            }
        }

        Text {
            anchors.centerIn: desktopGrid
            color: desktopBackground.surfaceMuted
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            text: desktopBackground.desktopItems
                && desktopBackground.desktopItems.errorMessage.length > 0
                ? desktopBackground.desktopItems.errorMessage
                : "Your Desktop is empty"
            visible: desktopBackground.desktopItems
                && desktopBackground.desktopItems.entries.length === 0
            width: Math.min(360, desktopGrid.width)
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
                desktopBackground.desktopItems.refresh()
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
                desktopBackground.desktopItems.refresh()
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
