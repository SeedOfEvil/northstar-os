import QtQuick

Item {
    id: root

    property string iconName: "northstar"
    property url iconSource: typeof northstarIconsSource !== "undefined" ? northstarIconsSource : ""
    property url generatedIconsDirectory: typeof northstarGeneratedIconsDirectory !== "undefined"
        ? northstarGeneratedIconsDirectory : ""
    property real tileWidth: 1447 / 4
    property real tileHeight: 1087 / 3
    property real cropFactor: 0.76
    property int tileIndex: iconTileIndex(root.iconName)
    // Aurora keeps one coherent atlas across the shell. The older standalone
    // generated icons remain installed for rollback, but mixing their glossy
    // material with the quieter Aurora set made the dock feel inconsistent.
    property bool usesGeneratedIcon: false

    function iconTileIndex(name) {
        switch (name) {
        case "files": case "folder": return 0
        case "terminal": return 1
        case "browser": return 2
        case "settings": case "power": return 3
        case "applications": case "software": return 4
        case "quick-settings": return 5
        case "trash": return 6
        case "search": return 7
        case "desktop": return 8
        case "editor": case "file": return 9
        case "info": case "notifications": return 10
        case "northstar": default: return 11
        }
    }

    Image {
        anchors.fill: parent
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectFit
        mipmap: true
        smooth: true
        source: root.usesGeneratedIcon
            ? root.generatedIconsDirectory + "northstar-" + root.iconName + ".png"
            : root.iconSource
        sourceClipRect: Qt.rect(
            root.usesGeneratedIcon ? 0 : (root.tileIndex % 4) * root.tileWidth
                + root.tileWidth * (1 - root.cropFactor) / 2,
            root.usesGeneratedIcon ? 0 : Math.floor(root.tileIndex / 4) * root.tileHeight
                + root.tileHeight * (1 - root.cropFactor) / 2,
            root.usesGeneratedIcon ? 0 : root.tileWidth * root.cropFactor,
            root.usesGeneratedIcon ? 0 : root.tileHeight * root.cropFactor)
    }
}
