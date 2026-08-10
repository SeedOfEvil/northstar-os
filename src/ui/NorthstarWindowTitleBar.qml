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

    Row {
        anchors.left: parent.left
        anchors.right: controls.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        Image {
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
            height: 38
            source: bar.iconSource
            visible: bar.showIcon
            width: visible ? 38 : 0
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            width: Math.max(80, parent.width - actionRow.width - (bar.showIcon ? 50 : 0) - 12)

            Text {
                color: bar.lunarPalette.foreground
                elide: Text.ElideRight
                font.bold: true
                font.pixelSize: 22
                text: bar.title
                width: parent.width
            }
            Text {
                color: bar.lunarPalette.muted
                elide: Text.ElideRight
                font.pixelSize: 12
                text: bar.subtitle
                visible: text.length > 0
                width: parent.width
            }
        }

        Row {
            id: actionRow
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
        }
    }

    NorthstarWindowControls {
        id: controls
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        closeDestroysWindow: bar.closeDestroysWindow
        maximized: bar.maximized
        maximizeEnabled: bar.maximizeEnabled
        lunarPalette: bar.lunarPalette
        window: bar.window
        onMaximizeRequested: bar.maximizeRequested()
    }
}
