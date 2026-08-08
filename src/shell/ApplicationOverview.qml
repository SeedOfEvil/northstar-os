import QtQuick
import QtQuick.Controls

Window {
    id: overview

    property var applicationLauncher
    property var targetScreen
    property int panelHeight: 96
    property int desktopMargin: 24
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property color surfaceBackground: "#171a21"
    property color surfaceForeground: "#f5f7fb"
    property color surfaceMuted: "#a9b1c2"
    property color surfaceAccent: "#79b8ff"

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.ApplicationModal
    title: "Northstar Applications"

    width: Math.max(640, screenWidth - (desktopMargin * 2))
    height: Math.max(420, screenHeight - panelHeight - (desktopMargin * 2))
    x: screenX + desktopMargin
    y: screenY + panelHeight + desktopMargin

    onVisibleChanged: {
        if (visible) {
            applicationLauncher.setApplicationQuery("")
            searchField.forceActiveFocus()
            requestActivate()
        } else {
            applicationLauncher.setApplicationQuery("")
        }
    }

    Rectangle {
        anchors.fill: parent
        color: overview.surfaceBackground
        border.color: overview.surfaceAccent
        border.width: 1
        radius: 12

        Column {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Row {
                width: parent.width
                spacing: 10

                Column {
                    width: parent.width - closeButton.width - parent.spacing
                    spacing: 2

                    Text {
                        color: overview.surfaceForeground
                        font.bold: true
                        font.pixelSize: 20
                        text: "Applications"
                    }

                    Text {
                        color: overview.surfaceMuted
                        font.pixelSize: 12
                        text: "Search installed FreeBSD applications"
                    }
                }

                Button {
                    id: closeButton
                    text: "Close"
                    onClicked: overview.visible = false
                }
            }

            TextField {
                id: searchField
                width: parent.width
                placeholderText: "Search by name, category, or desktop id"
                selectByMouse: true

                onTextChanged: applicationLauncher.setApplicationQuery(text)
                onAccepted: {
                    if (applicationLauncher.matchingApplications.length > 0) {
                        applicationLauncher.launchApplication(applicationLauncher.matchingApplications[0].desktopId)
                        overview.visible = false
                    }
                }
            }

            Rectangle {
                color: overview.surfaceBackground
                border.color: overview.surfaceMuted
                border.width: 1
                height: parent.height - searchField.height - 72
                width: parent.width

                ListView {
                    id: applicationList
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    model: applicationLauncher ? applicationLauncher.matchingApplications : []
                    spacing: 4

                    delegate: ItemDelegate {
                        required property var modelData

                        width: applicationList.width
                        text: modelData.genericName ? modelData.name + "  ·  " + modelData.genericName : modelData.name
                        onClicked: {
                            applicationLauncher.launchApplication(modelData.desktopId)
                            overview.visible = false
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }

                Text {
                    anchors.centerIn: parent
                    color: overview.surfaceMuted
                    font.pixelSize: 13
                    text: "No matching applications"
                    visible: applicationList.count === 0
                }
            }
        }
    }
}
