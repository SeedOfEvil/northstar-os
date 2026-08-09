import QtQuick

DragHandler {
    id: moveHandler

    required property var window

    target: null
    acceptedButtons: Qt.LeftButton
    cursorShape: active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
    grabPermissions: PointerHandler.CanTakeOverFromAnything
                     | PointerHandler.ApprovesTakeOverByAnything

    onActiveChanged: {
        if (active) {
            moveHandler.window.startSystemMove()
        }
    }
}
