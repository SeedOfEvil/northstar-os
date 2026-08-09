import QtQuick

Item {
    id: root

    property string iconName: "northstar"
    property url iconSource: northstarIconsSource
    property url generatedIconsDirectory: typeof northstarGeneratedIconsDirectory !== "undefined"
        ? northstarGeneratedIconsDirectory : ""
    property int tileSize: 362
    property int tileIndex: iconTileIndex(root.iconName)
    property bool usesGeneratedIcon: String(root.generatedIconsDirectory).length > 0
        && (root.iconName === "welcome"
            || root.iconName === "software"
            || root.iconName === "notifications"
            || root.iconName === "power")

    function iconTileIndex(name) {
        switch (name) {
        case "files":
        case "folder":
            return 0
        case "terminal":
            return 1
        case "browser":
            return 2
        case "settings":
            return 3
        case "applications":
            return 4
        case "software":
            return 4
        case "quick-settings":
            return 5
        case "trash":
            return 6
        case "search":
            return 7
        case "desktop":
            return 8
        case "editor":
        case "file":
            return 9
        case "info":
        case "notifications":
            return 10
        case "northstar":
        default:
            return 11
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
            root.usesGeneratedIcon ? 0 : (root.tileIndex % 4) * root.tileSize,
            root.usesGeneratedIcon ? 0 : Math.floor(root.tileIndex / 4) * root.tileSize,
            root.usesGeneratedIcon ? 0 : root.tileSize,
            root.usesGeneratedIcon ? 0 : root.tileSize)
    }
}
