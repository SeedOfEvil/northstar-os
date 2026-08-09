import QtQuick

Item {
    id: root

    required property var window
    property bool resizingEnabled: true
    property int edgeSize: 16

    anchors.bottom: parent.bottom
    anchors.right: parent.right
    height: edgeSize
    visible: resizingEnabled
    width: edgeSize
    z: 1000

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        anchors.right: parent.right
        anchors.rightMargin: 4
        color: "transparent"
        border.color: root.window && root.window.active ? "#7bbcff" : "#6f8299"
        border.width: 1
        height: 7
        opacity: 0.8
        rotation: 45
        width: 7
    }

    DragHandler {
        target: null
        acceptedButtons: Qt.LeftButton
        cursorShape: Qt.SizeFDiagCursor

        onActiveChanged: {
            if (active && root.resizingEnabled) {
                root.window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
            }
        }
    }
}
