import QtQuick
import QtQuick.Controls

Button {
    id: control

    property bool accentButton: false
    property bool dangerButton: false

    LunarPalette {
        id: lunar
        darkMode: true
    }

    implicitHeight: 36
    leftPadding: 14
    rightPadding: 14

    contentItem: Text {
        color: control.enabled ? lunar.foreground : lunar.subtle
        elide: Text.ElideRight
        font.bold: control.accentButton
        font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter
        text: control.text
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        border.color: control.activeFocus ? lunar.accent
            : control.dangerButton ? "#8cff6f82"
            : control.hovered ? lunar.border : lunar.borderSoft
        border.width: 1
        color: control.down
                ? (control.dangerButton ? Qt.darker(lunar.danger, 1.45) : lunar.accentSoft)
            : control.hovered
                ? (control.dangerButton ? "#52ff6f82" : lunar.raisedHover)
                : control.accentButton ? lunar.accentSoft : lunar.raised
        opacity: control.enabled ? 1 : 0.55
        radius: lunar.radiusSmall

        Behavior on color { ColorAnimation { duration: 110 } }
    }
}
