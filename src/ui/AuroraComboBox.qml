import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    LunarPalette { id: lunar; darkMode: true }

    leftPadding: 12
    rightPadding: 32

    contentItem: Text {
        color: control.enabled ? lunar.foreground : lunar.subtle
        elide: Text.ElideRight
        font.pixelSize: 12
        text: control.displayText
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        border.color: control.activeFocus ? lunar.accent
            : control.hovered ? lunar.border : lunar.borderSoft
        border.width: 1
        color: control.hovered ? lunar.raisedHover : lunar.field
        radius: lunar.radiusSmall
    }

    indicator: Text {
        anchors.right: parent.right
        anchors.rightMargin: 11
        anchors.verticalCenter: parent.verticalCenter
        color: lunar.muted
        font.pixelSize: 13
        text: "⌄"
    }
}
