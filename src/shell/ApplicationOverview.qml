import QtQuick
import QtQuick.Controls

Popup {
    id: overview

    property var applicationLauncher
    property color surfaceBackground: "#171a21"
    property color surfaceForeground: "#f5f7fb"
    property color surfaceMuted: "#a9b1c2"
    property color surfaceAccent: "#79b8ff"

    width: 720
    height: 520
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 18

    x: parent ? Math.max(12, (parent.width - width) / 2) : 12
    y: parent ? parent.height + 12 : 108

    background: Rectangle {
        color: overview.surfaceBackground
        border.color: overview.surfaceAccent
        border.width: 1
        radius: 12
    }

    onOpened: {
        applicationLauncher.setApplicationQuery("")
        searchField.forceActiveFocus()
    }

    onClosed: applicationLauncher.setApplicationQuery("")

    Column {
        anchors.fill: parent
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
                onClicked: overview.close()
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
                    overview.close()
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
                        overview.close()
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
