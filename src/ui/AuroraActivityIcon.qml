import QtQuick

Item {
    id: activity

    property bool running: true
    property url source: "qrc:/Northstar/Ui/northstar-activity-aurora.png"
    property int frame: 0
    readonly property real atlasWidth: 2172
    readonly property real atlasHeight: 724
    readonly property real frameWidth: atlasWidth / 8

    visible: running

    Image {
        anchors.fill: parent
        fillMode: Image.Stretch
        mipmap: true
        smooth: true
        source: activity.source
        sourceClipRect: Qt.rect(activity.frame * activity.frameWidth, 0,
                                activity.frameWidth, activity.atlasHeight)
    }

    Timer {
        interval: 90
        repeat: true
        running: activity.running && activity.visible
        onTriggered: activity.frame = (activity.frame + 1) % 8
    }
}
