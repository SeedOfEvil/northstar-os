import QtQuick

Item {
    id: icon

    property string iconName: "settings"
    property url source: "qrc:/Northstar/Ui/northstar-system-icons-aurora.png"
    property real atlasWidth: 1402
    property real atlasHeight: 1122
    readonly property real tileWidth: atlasWidth / 5
    readonly property real tileHeight: atlasHeight / 4

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
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        mipmap: true
        smooth: true
        source: icon.source
        readonly property var tile: icon.tileForName(icon.iconName)
        sourceClipRect: Qt.rect(tile[0] * icon.tileWidth,
                                tile[1] * icon.tileHeight,
                                icon.tileWidth,
                                icon.tileHeight)
    }
}
