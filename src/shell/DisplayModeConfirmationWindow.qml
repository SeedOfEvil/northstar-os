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

    LunarPalette {
        id: lunar
        darkMode: confirmation.state ? confirmation.state.darkMode : true
    }

    color: "transparent"
    flags: Qt.Dialog | Qt.FramelessWindowHint
    height: 238
    modality: Qt.ApplicationModal
    title: "Confirm display mode"
    visible: confirmation.controller
        && confirmation.controller.displayModePending
    width: Math.min(500, screenWidth - (desktopMargin * 2))
    x: screenX + (screenWidth - width) / 2
    y: screenY + panelHeight + Math.max(desktopMargin,
        (screenHeight - panelHeight - height) / 3)

    onVisibleChanged: {
        if (visible) {
            pendingAction = ""
            raise()
            requestActivate()
        }
    }

    Timer {
        id: actionTimer
        interval: 0
        onTriggered: {
            if (!confirmation.controller) {
                confirmation.pendingAction = ""
                return
            }
            const succeeded = confirmation.pendingAction === "keep"
                ? confirmation.controller.keepDisplayMode()
                : confirmation.controller.revertDisplayMode()
            if (!succeeded) {
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
                text: "Keep this display mode?"
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
                    text: confirmation.pendingAction === "revert" ? "Restoring..." : "Revert"
                    onClicked: {
                        confirmation.pendingAction = "revert"
                        actionTimer.start()
                    }
                }

                Button {
                    enabled: confirmation.pendingAction.length === 0
                    highlighted: true
                    text: confirmation.pendingAction === "keep" ? "Keeping..." : "Keep display"
                    onClicked: {
                        confirmation.pendingAction = "keep"
                        actionTimer.start()
                    }
                }
            }
        }
    }
}
