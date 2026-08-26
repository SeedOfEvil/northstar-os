import QtQuick

QtObject {
    property bool darkMode: true

    readonly property color desktopTop: darkMode ? "#061329" : "#dcecff"
    readonly property color desktopBottom: darkMode ? "#0a2a59" : "#8bbbea"
    readonly property color background: darkMode ? "#081426" : "#f1f6fc"
    readonly property color backgroundDeep: darkMode ? "#040d1b" : "#dce8f5"
    readonly property color panel: darkMode ? "#df122238" : "#ccecf5fb"
    readonly property color panelStrong: darkMode ? "#eb0b1728" : "#e6f4f9fd"
    readonly property color dockGlass: darkMode ? "#c0122238" : "#bfe9f2fa"
    readonly property color dockGlassEdge: darkMode ? "#53647f9d" : "#789ab4cf"
    readonly property color raised: darkMode ? "#d61a2b42" : "#cce1ebf5"
    readonly property color raisedHover: darkMode ? "#ee243c59" : "#d8d7e7f7"
    readonly property color field: darkMode ? "#c00d1b2d" : "#bfe9f1f9"
    readonly property color foreground: darkMode ? "#f5f8fc" : "#142238"
    readonly property color muted: darkMode ? "#a7b5c8" : "#53677f"
    readonly property color subtle: darkMode ? "#74869c" : "#7a8ca1"
    readonly property color accent: darkMode ? "#24c7ef" : "#147bd1"
    readonly property color accentBright: darkMode ? "#67d9f5" : "#2998e7"
    readonly property color accentSoft: darkMode ? "#2a6380" : "#b7dcf7"
    readonly property color border: darkMode ? "#53657c" : "#a8bed4"
    readonly property color borderSoft: darkMode ? "#2b3d53" : "#ccd9e6"
    readonly property color success: "#50d890"
    readonly property color warning: "#f4bd65"
    readonly property color danger: "#ff6f82"
    readonly property color shadow: darkMode ? "#99030a14" : "#553c5875"

    readonly property int radiusSmall: 9
    readonly property int radiusMedium: 13
    readonly property int radiusLarge: 17
    readonly property int radiusPanel: 18
    readonly property int spacingSmall: 6
    readonly property int spacing: 12
    readonly property int spacingLarge: 18
}
