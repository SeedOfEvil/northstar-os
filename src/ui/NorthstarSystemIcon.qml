import QtQuick
import QtQuick.Effects

Item {
    id: icon

    property string iconName: "settings"
    property bool darkMode: true
    property bool monochrome: false
    property color monochromeColor: "#f4f8ff"
    property url source: darkMode
        ? "qrc:/Northstar/Ui/northstar-system-icons-aurora.png"
        : "qrc:/Northstar/Ui/northstar-system-icons-aurora-light.png"
    property real atlasWidth: 1402
    property real atlasHeight: 1122
    property real cropFactor: 0.62
    readonly property real tileWidth: atlasWidth / 5
    readonly property real tileHeight: atlasHeight / 4
    readonly property real cropWidth: tileWidth * cropFactor
    readonly property real cropHeight: tileHeight * cropFactor

    function tileForName(name) {
        const tiles = {
            "wifi": [0, 0], "network": [1, 0], "bluetooth": [2, 0],
            "display": [3, 0], "sound": [4, 0], "power": [0, 1],
            "mouse": [1, 1], "keyboard": [2, 1], "appearance": [3, 1],
            "notifications": [4, 1], "privacy": [0, 2], "users": [1, 2],
            "settings": [2, 2], "brightness": [3, 2], "battery": [4, 2],
            "lock": [0, 3], "terminal": [1, 3], "files": [2, 3],
            "browser": [3, 3], "mail": [4, 3]
        }
        return tiles[name] || tiles.settings
    }

    Image {
        id: glyphImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        mipmap: true
        smooth: true
        source: icon.source
        readonly property var tile: icon.tileForName(icon.iconName)
        sourceClipRect: Qt.rect(
            tile[0] * icon.tileWidth + (icon.tileWidth - icon.cropWidth) / 2,
            tile[1] * icon.tileHeight + (icon.tileHeight - icon.cropHeight) / 2,
            icon.cropWidth,
            icon.cropHeight)

        layer.enabled: icon.monochrome
        layer.effect: MultiEffect {
            colorization: 1
            colorizationColor: icon.monochromeColor
            saturation: 0
        }
    }
}
