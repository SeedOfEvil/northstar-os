import QtQuick

QtObject {
    id: controller

    required property var window
    property int screenX: 0
    property int screenY: 0
    property int screenWidth: 1280
    property int screenHeight: 800
    property int topInset: 44
    property int bottomInset: 12
    property int edgeMargin: 8
    property real defaultX: screenX + edgeMargin
    property real defaultY: screenY + topInset + edgeMargin
    property bool dragging: false
    property bool hasCustomPosition: false
    property point pointerOrigin: Qt.point(0, 0)
    property point windowOrigin: Qt.point(0, 0)

    function minimumX() {
        return controller.screenX + controller.edgeMargin
    }

    function maximumX() {
        return Math.max(minimumX(), controller.screenX + controller.screenWidth
                        - controller.window.width - controller.edgeMargin)
    }

    function minimumY() {
        return controller.screenY + controller.topInset + controller.edgeMargin
    }

    function maximumY() {
        return Math.max(minimumY(), controller.screenY + controller.screenHeight
                        - controller.window.height - controller.bottomInset)
    }

    function clampedX(candidate) {
        return Math.max(minimumX(), Math.min(maximumX(), candidate))
    }

    function clampedY(candidate) {
        return Math.max(minimumY(), Math.min(maximumY(), candidate))
    }

    function resetPosition() {
        controller.window.x = clampedX(controller.defaultX)
        controller.window.y = clampedY(controller.defaultY)
        controller.hasCustomPosition = false
    }

    function keepVisible() {
        if (!controller.window) {
            return
        }
        if (!controller.hasCustomPosition) {
            resetPosition()
            return
        }
        controller.window.x = clampedX(controller.window.x)
        controller.window.y = clampedY(controller.window.y)
    }

    function prepareForOpen() {
        keepVisible()
    }

    function begin(globalX, globalY) {
        controller.dragging = true
        controller.hasCustomPosition = true
        controller.pointerOrigin = Qt.point(globalX, globalY)
        controller.windowOrigin = Qt.point(controller.window.x, controller.window.y)
    }

    function update(globalX, globalY) {
        if (!controller.dragging) {
            return
        }
        controller.window.x = clampedX(controller.windowOrigin.x
                                       + globalX - controller.pointerOrigin.x)
        controller.window.y = clampedY(controller.windowOrigin.y
                                       + globalY - controller.pointerOrigin.y)
    }

    function end() {
        controller.dragging = false
        keepVisible()
    }

    onScreenXChanged: keepVisible()
    onScreenYChanged: keepVisible()
    onScreenWidthChanged: keepVisible()
    onScreenHeightChanged: keepVisible()
    onDefaultXChanged: {
        if (!hasCustomPosition) {
            resetPosition()
        }
    }
    onDefaultYChanged: {
        if (!hasCustomPosition) {
            resetPosition()
        }
    }
}
