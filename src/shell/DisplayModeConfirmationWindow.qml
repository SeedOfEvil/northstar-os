import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: confirmation

    property var controller
    property var state
    property var targetScreen
    property int panelHeight: 44
    property int desktopMargin: 24
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property string pendingAction: ""

    function readableMode(mode) {
        if (!mode) {
            return "previous mode"
        }
        return mode.replace("x", " × ").replace("@", " at ").replace("Hz", " Hz")
    }

    function syncVisibility() {
        const pending = confirmation.controller
            && confirmation.controller.displayModePending
        if (pending && !visible) {
            show()
            raise()
            requestActivate()
        } else if (!pending && visible) {
            hide()
        }
    }

    function remapIfPending() {
        if (!confirmation.controller
            || !confirmation.controller.displayModePending) {
            return
        }
        hide()
        remapTimer.restart()
    }

    LunarPalette {
        id: lunar
        darkMode: confirmation.state ? confirmation.state.darkMode : true
    }

    color: "transparent"
    flags: Qt.Dialog | Qt.FramelessWindowHint
    height: 238
    modality: Qt.ApplicationModal
    title: "Confirm display mode"
    visible: false
    width: Math.min(500, screenWidth - (desktopMargin * 2))
    x: screenX + (screenWidth - width) / 2
    y: screenY + panelHeight + Math.max(desktopMargin,
        (screenHeight - panelHeight - height) / 3)

    onVisibleChanged: {
        if (visible) {
            if (!keepTimer.running && !revertTimer.running) {
                pendingAction = ""
            }
            raise()
            requestActivate()
        }
    }

    Component.onCompleted: syncVisibility()

    Connections {
        target: confirmation.controller
        function onCapabilitiesChanged() {
            confirmation.syncVisibility()
        }
    }

    Timer {
        id: remapTimer
        interval: 100
        onTriggered: confirmation.syncVisibility()
    }

    Timer {
        id: keepTimer
        interval: 0
        onTriggered: {
            if (!confirmation.controller) {
                confirmation.pendingAction = ""
                return
            }
            if (!confirmation.controller.keepDisplayMode()) {
                confirmation.pendingAction = ""
            }
        }
    }

    Timer {
        id: revertTimer
        interval: 0
        onTriggered: {
            if (!confirmation.controller) {
                confirmation.pendingAction = ""
                return
            }
            if (!confirmation.controller.revertDisplayMode()) {
                confirmation.pendingAction = ""
            }
        }
    }

    NorthstarWindowFrame {
        anchors.fill: parent
        darkMode: lunar.darkMode

        Column {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14

            Text {
                color: lunar.foreground
                font.bold: true
                font.pixelSize: 20
                text: "Keep " + confirmation.readableMode(
                    confirmation.controller ? confirmation.controller.currentDisplayMode : "") + "?"
                width: parent.width
            }

            Text {
                color: lunar.muted
                font.pixelSize: 13
                text: confirmation.pendingAction === "keep"
                    ? "Keeping this mode..."
                    : confirmation.pendingAction === "revert"
                        ? "Restoring the previous mode..."
                        : "Northstar will restore the previous mode in "
                            + (confirmation.controller
                                ? confirmation.controller.displayModeSecondsRemaining : 0)
                            + " seconds unless you keep this one."
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Item {
                height: 4
                width: 1
            }

            Row {
                anchors.right: parent.right
                spacing: 12

                Button {
                    enabled: confirmation.pendingAction.length === 0
                    text: confirmation.pendingAction === "revert"
                        ? "Restoring..."
                        : "Go back to " + confirmation.readableMode(
                            confirmation.controller
                                ? confirmation.controller.previousDisplayMode : "")
                    onClicked: {
                        confirmation.pendingAction = "revert"
                        revertTimer.start()
                    }
                }

                Button {
                    enabled: confirmation.pendingAction.length === 0
                    highlighted: true
                    text: confirmation.pendingAction === "keep"
                        ? "Keeping..."
                        : "Keep " + confirmation.readableMode(
                            confirmation.controller
                                ? confirmation.controller.currentDisplayMode : "")
                    onClicked: {
                        confirmation.pendingAction = "keep"
                        keepTimer.start()
                    }
                }
            }
        }
    }
}
