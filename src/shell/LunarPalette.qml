import QtQuick

QtObject {
    id: palette

    property bool darkMode: true

    readonly property color desktopTop: darkMode ? "#07172f" : "#d9e9ff"
    readonly property color desktopBottom: darkMode ? "#0c2c5a" : "#86b8ee"
    readonly property color background: darkMode ? "#0a172b" : "#eef5ff"
    readonly property color backgroundDeep: darkMode ? "#061020" : "#dbe9fa"
    readonly property color panel: darkMode ? "#e61a3153" : "#eef8fcff"
    readonly property color panelStrong: darkMode ? "#f01a2b48" : "#f7fbffff"
    readonly property color dockGlass: darkMode ? "#901a3153" : "#86f8fcff"
    readonly property color dockGlassEdge: darkMode ? "#706b91bd" : "#8f9bb9da"
    readonly property color raised: darkMode ? "#d926436c" : "#dceaf8"
    readonly property color raisedHover: darkMode ? "#ef315786" : "#c9e1fa"
    readonly property color field: darkMode ? "#b80b1b33" : "#e7f1fc"
    readonly property color foreground: darkMode ? "#f6f9ff" : "#14233a"
    readonly property color muted: darkMode ? "#a9bdd8" : "#546a85"
    readonly property color subtle: darkMode ? "#718aa9" : "#7a8da4"
    readonly property color accent: darkMode ? "#63adff" : "#1976d2"
    readonly property color accentBright: darkMode ? "#82c9ff" : "#3294eb"
    readonly property color accentSoft: darkMode ? "#3f6fa5" : "#a9d2fa"
    readonly property color border: darkMode ? "#58779e" : "#9bb9da"
    readonly property color borderSoft: darkMode ? "#355373" : "#c2d6eb"
    readonly property color success: "#50d890"
    readonly property color warning: "#f4bd65"
    readonly property color danger: "#ff6f82"
    readonly property color shadow: darkMode ? "#99030a14" : "#553c5875"

    readonly property int radiusSmall: 8
    readonly property int radiusMedium: 12
    readonly property int radiusLarge: 18
    readonly property int radiusPanel: 22
    readonly property int spacingSmall: 6
    readonly property int spacing: 10
    readonly property int spacingLarge: 16
}
