import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

// The desktop background picker, shared by the desktop context menu and the
// Settings appearance section so both offer the same browsing, the same fits,
// and the same refusal messages.
Dialog {
    id: picker

    // The WallpaperController this picker drives.
    property var wallpaper

    property color surfaceBackground: "#101318"
    property color surfaceForeground: "#f2f4f8"
    property color surfaceMuted: "#9aa4b2"
    property color surfaceAccent: "#3f7ad6"
    property color surfaceRaised: "#1a1f27"
    property color surfaceWarning: "#ff8a8a"

    readonly property bool hasImage: !!picker.wallpaper && picker.wallpaper.hasImage

    // Opens on the folder the current picture came from, so changing a
    // wallpaper starts where the last one was found rather than at Home.
    function openAt() {
        if (!picker.wallpaper) {
            return
        }
        const current = picker.wallpaper.imagePath
        const separator = current.lastIndexOf("/")
        if (current !== "" && separator > 0) {
            picker.wallpaper.browseTo(current.substring(0, separator))
        } else {
            picker.wallpaper.browseToPictures()
        }
        picker.open()
    }

    modal: true
    padding: 16
    standardButtons: Dialog.Close
    title: "Desktop Background"

    background: Rectangle {
        border.color: picker.surfaceAccent
        border.width: 1
        color: picker.surfaceRaised
        radius: 12
    }

    contentItem: Column {
        spacing: 10
        width: picker.availableWidth

        Row {
            spacing: 8
            width: parent.width

            AuroraButton {
                enabled: !!picker.wallpaper
                text: "Pictures"
                onClicked: picker.wallpaper.browseToPictures()
            }

            AuroraButton {
                enabled: !!picker.wallpaper
                text: "Home"
                onClicked: picker.wallpaper.browseHome()
            }

            AuroraButton {
                enabled: !!picker.wallpaper && picker.wallpaper.browseCanNavigateUp
                text: "Up"
                onClicked: picker.wallpaper.browseUp()
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: picker.surfaceMuted
                elide: Text.ElideLeft
                font.pixelSize: 12
                text: picker.wallpaper ? picker.wallpaper.browseDisplayPath : ""
                width: Math.max(0, parent.width - 240)
            }
        }

        ListView {
            id: entryList
            clip: true
            height: 280
            model: picker.wallpaper ? picker.wallpaper.browseEntries : []
            spacing: 3
            width: parent.width

            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                required property var modelData

                color: entryMouse.containsMouse ? picker.surfaceBackground : "transparent"
                height: 32
                radius: 6
                width: entryList.width

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: entryDetail.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    // A folder is always worth showing in full strength: it
                    // can be entered even when nothing in it is a picture.
                    color: modelData.selectable || modelData.isDirectory
                        ? picker.surfaceForeground
                        : picker.surfaceMuted
                    elide: Text.ElideMiddle
                    font.pixelSize: 13
                    text: modelData.isDirectory ? modelData.name + "/" : modelData.name
                }

                Text {
                    id: entryDetail
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    color: picker.surfaceMuted
                    font.pixelSize: 11
                    text: modelData.reason !== "" ? modelData.reason : modelData.size
                }

                MouseArea {
                    id: entryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (modelData.isDirectory) {
                            picker.wallpaper.browseTo(modelData.path)
                        } else if (modelData.selectable) {
                            picker.wallpaper.setImagePath(modelData.path)
                        }
                    }
                }
            }
        }

        Text {
            color: picker.surfaceMuted
            font.pixelSize: 11
            text: "Only the first part of this folder is listed."
            visible: !!picker.wallpaper && picker.wallpaper.browseTruncated
            width: parent.width
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: 8
            width: parent.width

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: picker.surfaceForeground
                font.pixelSize: 12
                text: "Fit"
            }

            Repeater {
                model: picker.wallpaper ? picker.wallpaper.availableFitModes() : []

                delegate: AuroraButton {
                    required property var modelData

                    checkable: false
                    // Fit has nothing to act on until a picture is chosen.
                    enabled: picker.hasImage
                    font.bold: picker.wallpaper && picker.wallpaper.fitMode === modelData
                    text: picker.wallpaper ? picker.wallpaper.labelForFitMode(modelData) : ""
                    onClicked: picker.wallpaper.setFitMode(modelData)
                }
            }
        }

        AuroraButton {
            enabled: picker.hasImage
            text: "Use built-in background"
            onClicked: picker.wallpaper.clearImage()
        }

        Text {
            color: picker.wallpaper && picker.wallpaper.statusIsError
                ? picker.surfaceWarning
                : picker.surfaceMuted
            font.pixelSize: 11
            text: picker.wallpaper ? picker.wallpaper.status : ""
            visible: text !== ""
            width: parent.width
            wrapMode: Text.WordWrap
        }
    }
}
