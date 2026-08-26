import QtQuick

DragHandler {
    id: handler

    required property var window
    signal moveStarted()

    target: null
    acceptedButtons: Qt.LeftButton
    cursorShape: active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
    grabPermissions: PointerHandler.CanTakeOverFromAnything
                     | PointerHandler.ApprovesTakeOverByAnything

    onActiveChanged: {
        if (active) {
            handler.moveStarted()
            handler.window.startSystemMove()
        }
    }
}
