import QtQuick
import QtQuick.Controls

Window {
    id: overview

    property var applicationLauncher
    property var targetScreen
    property int panelHeight: 44
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
    modality: Qt.NonModal
    title: "Northstar Applications"

    width: Math.min(780, screenWidth - 80)
    height: Math.min(500, screenHeight - panelHeight - 80)
    x: screenX + (screenWidth - width) / 2
    y: screenY + panelHeight + (screenHeight - panelHeight - height) / 2

    onVisibleChanged: {
        if (visible) {
            applicationLauncher.setApplicationQuery("")
            searchField.forceActiveFocus()
            requestActivate()
        } else {
            applicationLauncher.setApplicationQuery("")
        }
    }

    function openWithQuery(query) {
        show()
        raise()
        requestActivate()
        searchField.text = query || ""
        applicationLauncher.setApplicationQuery(searchField.text)
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    Rectangle {
        anchors.fill: parent
        color: overview.surfaceBackground
        border.color: overview.surfaceAccent
        border.width: 1
        radius: 16

        Column {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            Row {
                width: parent.width
                spacing: 10

                Column {
                    width: parent.width - closeButton.width - parent.spacing
                    spacing: 2

                    Text {
                        color: overview.surfaceForeground
                        font.bold: true
                        font.pixelSize: 18
                        text: "Apps"
                    }

                    Text {
                        color: overview.surfaceMuted
                        font.pixelSize: 12
                        text: "Launch your Northstar applications"
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
                background: Rectangle {
                    color: overview.surfaceBackground
                    border.color: overview.surfaceAccent
                    border.width: 1
                    radius: 8
                }
                color: overview.surfaceForeground
                height: 38
                width: parent.width
                placeholderText: "Search by name, category, or desktop id"
                placeholderTextColor: overview.surfaceMuted
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

                GridView {
                    id: applicationList
                    anchors.fill: parent
                    anchors.margins: 6
                    cellHeight: 112
                    cellWidth: 124
                    clip: true
                    model: applicationLauncher ? applicationLauncher.matchingApplications : []

                    delegate: Rectangle {
                        required property var modelData

                        color: applicationMouse.containsMouse ? overview.surfaceAccent : overview.surfaceBackground
                        height: 102
                        radius: 10
                        width: 112
                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 5

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: overview.surfaceAccent
                                height: 44
                                radius: 10
                                width: 44

                                Text {
                                    anchors.centerIn: parent
                                    color: overview.surfaceForeground
                                    font.bold: true
                                    font.pixelSize: 20
                                    text: modelData.name && modelData.name.length > 0
                                        ? modelData.name.charAt(0).toUpperCase() : "?"
                                }
                            }

                            Text {
                                color: overview.surfaceForeground
                                elide: Text.ElideRight
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                text: modelData.name
                                width: parent.width
                            }

                            Text {
                                color: overview.surfaceMuted
                                elide: Text.ElideRight
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                text: modelData.genericName || "Application"
                                width: parent.width
                            }
                        }

                        MouseArea {
                            id: applicationMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                applicationLauncher.launchApplication(modelData.desktopId)
                                overview.visible = false
                            }
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
