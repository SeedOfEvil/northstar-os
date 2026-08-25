import QtQuick

Item {
    id: activity

    property bool running: true
    property bool darkMode: true
    property string mode: "work"
    property url source: darkMode
        ? "qrc:/Northstar/Ui/northstar-activity-aurora.png"
        : "qrc:/Northstar/Ui/northstar-activity-aurora-light.png"
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
        visible: activity.mode === "work" || activity.mode === "sync"
        sourceClipRect: Qt.rect(activity.frame * activity.frameWidth, 0,
                                activity.frameWidth, activity.atlasHeight)
    }

    NorthstarSystemIcon {
        id: pulseIcon
        anchors.centerIn: parent
        darkMode: activity.darkMode
        height: parent.height * 0.82
        iconName: activity.mode === "bluetooth" ? "bluetooth" : "wifi"
        visible: activity.mode === "wifi" || activity.mode === "bluetooth"
        width: height
    }

    SequentialAnimation {
        loops: Animation.Infinite
        running: activity.running && pulseIcon.visible
        NumberAnimation { target: pulseIcon; property: "scale"; from: 0.88; to: 1.08; duration: 420 }
        NumberAnimation { target: pulseIcon; property: "scale"; from: 1.08; to: 0.88; duration: 420 }
    }

    SequentialAnimation {
        loops: Animation.Infinite
        running: activity.running && pulseIcon.visible
        NumberAnimation { target: pulseIcon; property: "opacity"; from: 0.58; to: 1.0; duration: 420 }
        NumberAnimation { target: pulseIcon; property: "opacity"; from: 1.0; to: 0.58; duration: 420 }
    }

    Timer {
        interval: 90
        repeat: true
        running: activity.running && activity.visible
            && (activity.mode === "work" || activity.mode === "sync")
        onTriggered: activity.frame = (activity.frame + 1) % 8
    }
}
