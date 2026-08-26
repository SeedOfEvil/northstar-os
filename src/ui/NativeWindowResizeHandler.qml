import QtQuick

Item {
    id: root

    required property var window
    property bool resizingEnabled: true
    property int edgeSize: 8
    property int cornerSize: 18

    anchors.fill: parent
    enabled: resizingEnabled
    visible: resizingEnabled
    z: 10000

    component ResizeZone: Item {
        required property int edges
        required property int pointerCursor

        DragHandler {
            target: null
            acceptedButtons: Qt.LeftButton
            cursorShape: parent.pointerCursor

            onActiveChanged: {
                if (active && root.resizingEnabled && root.window) {
                    root.window.startSystemResize(parent.edges)
                }
            }
        }
    }

    ResizeZone {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.edgeSize
        edges: Qt.TopEdge
        pointerCursor: Qt.SizeVerCursor
    }

    ResizeZone {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.edgeSize
        edges: Qt.BottomEdge
        pointerCursor: Qt.SizeVerCursor
    }

    ResizeZone {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.top: parent.top
        width: root.edgeSize
        edges: Qt.LeftEdge
        pointerCursor: Qt.SizeHorCursor
    }

    ResizeZone {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.top: parent.top
        width: root.edgeSize
        edges: Qt.RightEdge
        pointerCursor: Qt.SizeHorCursor
    }

    ResizeZone {
        anchors.left: parent.left
        anchors.top: parent.top
        height: root.cornerSize
        width: root.cornerSize
        edges: Qt.LeftEdge | Qt.TopEdge
        pointerCursor: Qt.SizeFDiagCursor
        z: 2
    }

    ResizeZone {
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.cornerSize
        width: root.cornerSize
        edges: Qt.RightEdge | Qt.TopEdge
        pointerCursor: Qt.SizeBDiagCursor
        z: 2
    }

    ResizeZone {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        height: root.cornerSize
        width: root.cornerSize
        edges: Qt.LeftEdge | Qt.BottomEdge
        pointerCursor: Qt.SizeBDiagCursor
        z: 2
    }

    ResizeZone {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        height: root.cornerSize
        width: root.cornerSize
        edges: Qt.RightEdge | Qt.BottomEdge
        pointerCursor: Qt.SizeFDiagCursor
        z: 2
    }

    Item {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        anchors.right: parent.right
        anchors.rightMargin: 4
        height: 9
        width: 9

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            color: root.window && root.window.active ? "#76dcff" : "#6f8299"
            height: 1
            opacity: 0.7
            rotation: -45
            transformOrigin: Item.BottomRight
            width: 9
        }
    }
}
