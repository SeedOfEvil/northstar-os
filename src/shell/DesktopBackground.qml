import QtQuick

Window {
    id: desktopBackground

    property url logoSource: northstarLogoSource
    property var state: shellState
    property var fileBrowserController: northstarFileBrowserController
    property var fileBrowserWindow: null
    property var targetScreen
    property int displayIndex: 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
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

        Grid {
            id: desktopItems
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 94
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.top: parent.top
            anchors.topMargin: 62
            columns: 1
            rowSpacing: 8
            visible: desktopBackground.displayIndex === 0
            width: 112

            Repeater {
                model: desktopBackground.fileBrowserController
                    ? desktopBackground.fileBrowserController.desktopEntries : []

                delegate: Rectangle {
                    color: desktopItemMouse.containsMouse ? "#4079b8" : "transparent"
                    height: 104
                    radius: 8
                    width: 104

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
                        onDoubleClicked: desktopBackground.openDesktopEntry(modelData)
                    }
                }
            }
        }
    }
}
