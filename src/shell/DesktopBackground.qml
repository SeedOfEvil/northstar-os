import QtQuick

Window {
    id: desktopBackground

    property url logoSource: northstarLogoSource
    property var state: shellState
    property var fileBrowserController: northstarFileBrowserController
    property var layoutController: northstarDesktopLayoutController
    property var fileBrowserWindow: null
    property var targetScreen
    property int displayIndex: 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property int layoutCellWidth: 112
    property int layoutCellHeight: 112
    property color surfaceBackground: state && state.darkMode ? "#0f1218" : "#e8edf5"

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool
    title: "Northstar Desktop Background"

    function openDesktopEntry(entry) {
        if (!entry || !desktopBackground.fileBrowserWindow
                || !desktopBackground.fileBrowserWindow.openDesktopEntry) {
            return
        }
        desktopBackground.fileBrowserWindow.openDesktopEntry(entry.path)
    }

    function nearestFreePosition(icon, candidateX, candidateY) {
        const maximumX = Math.max(0, desktopItems.width - icon.width)
        const maximumY = Math.max(0, desktopItems.height - icon.height)
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

        Item {
            id: desktopItems
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 94
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.top: parent.top
            anchors.topMargin: 62
            clip: true
            visible: desktopBackground.displayIndex === 0
            width: Math.min(360, parent.width * 0.32)

            Repeater {
                id: desktopRepeater
                model: desktopBackground.fileBrowserController
                    ? desktopBackground.fileBrowserController.desktopEntries : []

                delegate: Rectangle {
                    id: desktopIcon
                    property real itemX: 0
                    property real itemY: index * 112
                    property point dragOrigin: Qt.point(0, 0)
                    property point itemOrigin: Qt.point(0, 0)

                    color: desktopItemMouse.containsMouse ? "#4079b8" : "transparent"
                    height: 104
                    radius: 8
                    width: 104
                    x: desktopIcon.itemX
                    y: desktopIcon.itemY

                    function applySavedPosition() {
                        desktopIcon.itemX = 0
                        desktopIcon.itemY = index * 112
                        if (!desktopBackground.layoutController) {
                            return
                        }
                        const saved = desktopBackground.layoutController.positionFor(modelData.path)
                        if (saved && saved.x !== undefined && saved.y !== undefined) {
                            desktopIcon.itemX = Math.max(0, Math.min(desktopItems.width - desktopIcon.width, saved.x))
                            desktopIcon.itemY = Math.max(0, Math.min(desktopItems.height - desktopIcon.height, saved.y))
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
                        anchors.topMargin: 4
                        height: 58
                        iconName: modelData.isDirectory ? "files" : "editor"
                        width: 58
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 66
                        color: "#f5f7fb"
                        elide: Text.ElideMiddle
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        maximumLineCount: 2
                        text: modelData.name || "Desktop item"
                        width: parent.width - 8
                        wrapMode: Text.Wrap
                    }

                    MouseArea {
                        id: desktopItemMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onPressed: {
                            desktopIcon.dragOrigin = Qt.point(mouse.x, mouse.y)
                            desktopIcon.itemOrigin = Qt.point(desktopIcon.itemX, desktopIcon.itemY)
                        }
                        onPositionChanged: {
                            if (!pressed) {
                                return
                            }
                            desktopIcon.itemX = Math.max(0, Math.min(
                                desktopItems.width - desktopIcon.width,
                                desktopIcon.itemOrigin.x + mouse.x - desktopIcon.dragOrigin.x))
                            desktopIcon.itemY = Math.max(0, Math.min(
                                desktopItems.height - desktopIcon.height,
                                desktopIcon.itemOrigin.y + mouse.y - desktopIcon.dragOrigin.y))
                        }
                        onReleased: {
                            const snapped = desktopBackground.nearestFreePosition(
                                desktopIcon, desktopIcon.itemX, desktopIcon.itemY)
                            desktopIcon.itemX = snapped.x
                            desktopIcon.itemY = snapped.y
                            if (desktopBackground.layoutController) {
                                desktopBackground.layoutController.setPosition(
                                    modelData.path, desktopIcon.itemX, desktopIcon.itemY)
                            }
                        }
                        onDoubleClicked: desktopBackground.openDesktopEntry(modelData)
                    }
                }
            }
        }
    }
}
