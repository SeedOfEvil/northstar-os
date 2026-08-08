import QtQuick

Item {
    id: root

    property string iconName: "northstar"
    property url iconSource: northstarIconsSource
    property int tileSize: 362
    property int tileIndex: iconTileIndex(root.iconName)

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
        source: root.iconSource
        sourceClipRect: Qt.rect(
            (root.tileIndex % 4) * root.tileSize,
            Math.floor(root.tileIndex / 4) * root.tileSize,
            root.tileSize,
            root.tileSize)
    }
}
