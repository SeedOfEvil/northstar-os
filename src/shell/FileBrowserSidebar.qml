import QtQuick
import Northstar.Ui 1.0

Rectangle {
    id: sidebar

    required property var hostWindow
    required property var palette
    readonly property var files: hostWindow
    readonly property var lunar: palette

    color: lunar.panel
    border.color: lunar.borderSoft
    border.width: 1
    radius: lunar.radiusLarge

    Column {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        Row {
            spacing: 8
            width: parent.width

            NorthstarIcon {
                anchors.verticalCenter: parent.verticalCenter
                height: 28
                width: 28
                iconName: "files"
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1

                Text {
                    color: files.surfaceForeground
                    font.bold: true
                    font.pixelSize: 15
                    text: "Northstar Files"
                }

                Text {
                    color: files.surfaceMuted
                    font.pixelSize: 10
                    text: "Favorites"
                }
            }
        }

        Rectangle {
            color: files.surfaceMuted
            height: 1
            opacity: 0.35
            width: parent.width
        }

        Text {
            color: files.surfaceMuted
            font.bold: true
            font.pixelSize: 10
            text: "FAVORITES"
            width: parent.width
        }

        ListView {
            id: sidebarFavoritesList
            clip: true
            interactive: false
            model: files.sidebarFavorites
            spacing: 4
            width: parent.width
            height: contentHeight

            delegate: Rectangle {
                required property var modelData
                property bool available: files.sidebarRefreshToken >= 0
                    && files.sidebarItemAvailable(modelData)
                property bool active: modelData.kind === "trash"
                    ? files.showingTrash
                    : modelData.kind === "home"
                        ? files.fileBrowserController && files.fileBrowserController.homeLocation
                            && !files.showingTrash
                        : files.fileBrowserController
                            && files.fileBrowserController.currentPath
                                === files.fileBrowserController.homeChildPath(modelData.relativePath)
                color: active || sidebarItemMouse.containsMouse
                    ? files.surfaceAccent : "transparent"
                height: 36
                opacity: available ? 1 : 0.4
                radius: 6
                width: sidebarFavoritesList.width

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 9

                    NorthstarIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 24
                        width: 24
                        iconName: modelData.iconName
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: files.surfaceForeground
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        text: modelData.label
                        width: parent.width - 34
                    }
                }

                MouseArea {
                    id: sidebarItemMouse
                    anchors.fill: parent
                    enabled: available
                    hoverEnabled: true
                    onClicked: files.openSidebarItem(modelData)
                }
            }
        }

        Rectangle {
            color: files.surfaceMuted
            height: 1
            opacity: 0.35
            width: parent.width
        }

        Text {
            color: files.surfaceMuted
            font.bold: true
            font.pixelSize: 10
            text: "LOCATIONS"
            width: parent.width
        }

        Text {
            color: files.surfaceMuted
            font.pixelSize: 11
            text: files.fileBrowserController && files.fileBrowserController.readOnlyLocation
                    ? "Mounted volume"
                : "Northstar"
            width: parent.width
        }

        Text {
            color: files.surfaceMuted
            elide: Text.ElideMiddle
            font.pixelSize: 10
            text: files.fileBrowserController
                ? files.fileBrowserController.displayPath : "~"
            width: parent.width
        }

        Item { height: 1; width: 1 }

        Text {
            color: files.surfaceMuted
            font.pixelSize: 10
            text: "Northstar home-scoped storage"
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }
}
