import QtQuick

Item {
    id: bar

    required property var window
    required property var lunarPalette
    property string title: "Northstar"
    property string subtitle: ""
    property url iconSource: ""
    property bool showIcon: String(iconSource).length > 0
    property bool maximized: false
    property bool closeDestroysWindow: false
    property bool maximizeEnabled: true
    property alias actions: actionRow.data

    signal maximizeRequested()

    height: 52

    NativeWindowMoveHandler {
        enabled: !bar.maximized
        window: bar.window
    }

    NorthstarWindowControls {
        id: controls
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        closeDestroysWindow: bar.closeDestroysWindow
        maximized: bar.maximized
        maximizeEnabled: bar.maximizeEnabled
        lunarPalette: bar.lunarPalette
        window: bar.window
        onMaximizeRequested: bar.maximizeRequested()
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Image {
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
            height: 28
            source: bar.iconSource
            visible: bar.showIcon
            width: visible ? 28 : 0
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            width: Math.min(360, Math.max(120, bar.width - controls.width - actionRow.width - 80))

            Text {
                color: bar.lunarPalette.foreground
                elide: Text.ElideRight
                font.bold: true
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
                text: bar.title
                width: parent.width
            }
            Text {
                color: bar.lunarPalette.muted
                elide: Text.ElideRight
                font.pixelSize: 10
                horizontalAlignment: Text.AlignHCenter
                text: bar.subtitle
                visible: text.length > 0
                width: parent.width
            }
        }

    }

    Row {
        id: actionRow
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
    }
}
