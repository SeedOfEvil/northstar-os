import QtQuick

Rectangle {
    id: frame

    property bool darkMode: true

    color: lunar.panelStrong
    border.color: lunar.border
    border.width: 1
    radius: lunar.radiusPanel
    clip: true

    LunarPalette {
        id: lunar
        darkMode: frame.darkMode
    }

    gradient: Gradient {
        GradientStop { position: 0.0; color: lunar.panelStrong }
        GradientStop { position: 1.0; color: lunar.panel }
    }
}
