import QtQuick

Rectangle {
    id: tabBar

    required property var hostWindow
    required property var palette
    readonly property var files: hostWindow
    readonly property var lunar: palette

    color: lunar.field
    border.color: lunar.borderSoft
    border.width: 1
    height: 42
    radius: lunar.radiusMedium

    Row {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 6

        ListView {
            id: tabList
            anchors.verticalCenter: parent.verticalCenter
            clip: true
            height: 34
            orientation: ListView.Horizontal
            spacing: 6
            width: parent.width - newTabButton.width - parent.spacing
            model: files.tabs

            delegate: Rectangle {
                required property var modelData
                required property int index

                color: index === files.activeTabIndex
                    ? files.surfaceAccent
                    : tabMouse.containsMouse ? files.surfaceRaised : files.surfaceBackground
                height: 34
                radius: 7
                width: Math.min(190, Math.max(112, tabTitle.implicitWidth + 52))

                Text {
                    id: tabTitle
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.right: closeTabButton.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    color: files.surfaceForeground
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: modelData.title || "Files"
                }

                MouseArea {
                    id: tabMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: files.activateTab(index)
                }

                Rectangle {
                    id: closeTabButton
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter
                    color: closeTabMouse.containsMouse ? lunar.danger : "transparent"
                    height: 22
                    radius: 11
                    width: 22

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "×"
                    }

                    MouseArea {
                        id: closeTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: function(mouse) {
                            mouse.accepted = true
                            files.closeTab(index)
                        }
                    }
                }
            }
        }

        Rectangle {
            id: newTabButton
            anchors.verticalCenter: parent.verticalCenter
            color: newTabMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
            height: 32
            radius: 7
            width: 38

            Text {
                anchors.centerIn: parent
                color: files.surfaceForeground
                font.pixelSize: 18
                text: "+"
            }

            MouseArea {
                id: newTabMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: files.newTab()
            }
        }
    }
}
