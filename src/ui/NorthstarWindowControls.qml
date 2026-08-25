import QtQuick

Row {
    id: controls

    property var window
    property var lunarPalette
    property bool maximized: false
    property bool closeDestroysWindow: false
    property bool maximizeEnabled: true
    property int controlSize: 16

    signal minimizeRequested()
    signal maximizeRequested()
    signal closeRequested()

    spacing: 8

    Repeater {
        model: ["close", "minimize", "maximize"]

        delegate: Rectangle {
            required property string modelData

            color: modelData === "close" ? controls.lunarPalette.danger
                : modelData === "maximize" ? controls.lunarPalette.success
                : controls.lunarPalette.warning
            enabled: modelData !== "maximize" || controls.maximizeEnabled
            height: controls.controlSize
            opacity: enabled ? 1 : 0.45
            radius: controls.controlSize / 2
            width: controls.controlSize

            Text {
                anchors.centerIn: parent
                color: "#80101a27"
                font.bold: true
                font.pixelSize: 10
                opacity: controlMouse.containsMouse ? 1 : 0
                text: parent.modelData === "minimize" ? "−"
                    : parent.modelData === "maximize" ? (controls.maximized ? "❐" : "□") : "×"
            }

            MouseArea {
                id: controlMouse
                anchors.fill: parent
                enabled: parent.enabled
                hoverEnabled: true
                onClicked: {
                    if (parent.modelData === "minimize") {
                        controls.minimizeRequested()
                    } else if (parent.modelData === "maximize") {
                        controls.maximizeRequested()
                    } else {
                        controls.closeRequested()
                    }
                }
            }
        }
    }

    onMinimizeRequested: {
        if (window) window.showMinimized()
    }
    onCloseRequested: {
        if (!window) return
        if (closeDestroysWindow) window.close()
        else window.hide()
    }
}
