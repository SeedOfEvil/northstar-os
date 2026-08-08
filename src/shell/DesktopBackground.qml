import QtQuick

Window {
    id: desktopBackground

    property url logoSource: northstarLogoSource
    property var state: shellState
    property var targetScreen
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property color surfaceBackground: state && state.darkMode ? "#0f1218" : "#e8edf5"

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool
    title: "Northstar Desktop Background"

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
    }
}
