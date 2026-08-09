import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: welcome

    property color backgroundColor: "#07172f"
    property color panelColor: "#e61a3153"
    property color raisedColor: "#d926436c"
    property color foregroundColor: "#f6f9ff"
    property color mutedColor: "#a9bdd8"
    property color accentColor: "#63adff"
    property string statusMessage: "Choose a starting point for your Northstar desktop."

    color: welcome.backgroundColor
    height: 680
    minimumHeight: 620
    minimumWidth: 760
    title: "Northstar Welcome"
    visible: true
    width: 820

    background: Rectangle {
        color: welcome.backgroundColor

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#07172f" }
            GradientStop { position: 1.0; color: "#0c2c5a" }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 18
            color: welcome.panelColor
            border.color: "#58779e"
            border.width: 1
            radius: 24

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                color: welcome.accentColor
                height: 2
                opacity: 0.9
            }
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 42
        spacing: 18

        Row {
            spacing: 18
            width: parent.width

            Image {
                anchors.verticalCenter: parent.verticalCenter
                fillMode: Image.PreserveAspectFit
                height: 82
                source: "qrc:/Northstar/Welcome/northstar-welcome.svg"
                width: 82
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 5

                Text {
                    color: welcome.foregroundColor
                    font.bold: true
                    font.pixelSize: 30
                    text: "Welcome to Northstar"
                }

                Text {
                    color: welcome.mutedColor
                    font.pixelSize: 14
                    text: "A focused FreeBSD desktop with a calm, familiar workspace."
                }
            }
        }

        Text {
            color: welcome.mutedColor
            font.pixelSize: 14
            text: "Your session is ready. Start with a place to work, find an app, or review the desktop basics."
            wrapMode: Text.WordWrap
            width: parent.width
        }

        Row {
            spacing: 14
            width: parent.width

            Rectangle {
                color: homeMouse.containsMouse ? welcome.accentColor : welcome.raisedColor
                height: 132
                border.color: homeMouse.containsMouse ? "#82c9ff" : "#355373"
                border.width: 1
                radius: 16
                width: (parent.width - 28) / 3

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8

                    Text { color: welcome.foregroundColor; font.bold: true; font.pixelSize: 16; text: "Home" }
                    Text {
                        color: welcome.mutedColor
                        font.pixelSize: 12
                        text: "Open your personal workspace and start organizing files."
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                    Text { color: welcome.accentColor; font.pixelSize: 12; text: "Open Home Folder  ›" }
                }

                MouseArea {
                    id: homeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        welcome.statusMessage = "Opening your home folder."
                        Qt.openUrlExternally("file://" + northstarHomePath)
                    }
                }
            }

            Rectangle {
                color: desktopMouse.containsMouse ? welcome.accentColor : welcome.raisedColor
                height: 132
                border.color: desktopMouse.containsMouse ? "#82c9ff" : "#355373"
                border.width: 1
                radius: 16
                width: (parent.width - 28) / 3

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8

                    Text { color: welcome.foregroundColor; font.bold: true; font.pixelSize: 16; text: "Desktop" }
                    Text {
                        color: welcome.mutedColor
                        font.pixelSize: 12
                        text: "Keep frequently used folders and files close at hand."
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                    Text { color: welcome.accentColor; font.pixelSize: 12; text: "Open Desktop  ›" }
                }

                MouseArea {
                    id: desktopMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        welcome.statusMessage = "Opening your Desktop folder."
                        Qt.openUrlExternally("file://" + northstarHomePath + "/Desktop")
                    }
                }
            }

            Rectangle {
                color: guideMouse.containsMouse ? welcome.accentColor : welcome.raisedColor
                height: 132
                border.color: guideMouse.containsMouse ? "#82c9ff" : "#355373"
                border.width: 1
                radius: 16
                width: (parent.width - 28) / 3

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8

                    Text { color: welcome.foregroundColor; font.bold: true; font.pixelSize: 16; text: "Explore" }
                    Text {
                        color: welcome.mutedColor
                        font.pixelSize: 12
                        text: "Read the quick-start guide. This card is informational; use the Northstar menu for Apps, Files, Settings, and Software."
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                    Text { color: welcome.accentColor; font.pixelSize: 12; text: "Show me around  ›" }
                }

                MouseArea {
                    id: guideMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: welcome.statusMessage = "Guide: use the Northstar logo menu for Apps, Files, Settings, and Software. This card does not launch a second shell surface."
                }
            }
        }

        Rectangle {
            color: welcome.raisedColor
            height: 92
            border.color: "#355373"
            border.width: 1
            radius: 16
            width: parent.width

            Row {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                Text {
                    color: welcome.accentColor
                    font.bold: true
                    font.pixelSize: 20
                    text: "✓"
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    width: parent.width - 38

                    Text { color: welcome.foregroundColor; font.bold: true; font.pixelSize: 14; text: "Northstar is ready" }
                    Text {
                        color: welcome.mutedColor
                        font.pixelSize: 12
                        text: welcome.statusMessage
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                }
            }
        }

        Rectangle {
            color: welcome.raisedColor
            height: 126
            border.color: "#355373"
            border.width: 1
            radius: 16
            width: parent.width

            Row {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 24

                Column {
                    spacing: 7
                    width: parent.width * 0.58

                    Text {
                        color: welcome.foregroundColor
                        font.bold: true
                        font.pixelSize: 15
                        text: "Getting Started"
                    }
                    Text {
                        color: welcome.mutedColor
                        font.pixelSize: 12
                        text: "1. Open Home or Desktop to choose a workspace."
                    }
                    Text {
                        color: welcome.mutedColor
                        font.pixelSize: 12
                        text: "2. Use the Northstar menu to launch Apps and Settings."
                    }
                    Text {
                        color: welcome.mutedColor
                        font.pixelSize: 12
                        text: "3. Create a file in Files and watch it appear on Desktop."
                    }
                }

                Column {
                    spacing: 7
                    width: parent.width * 0.42 - 24

                    Text {
                        color: welcome.foregroundColor
                        font.bold: true
                        font.pixelSize: 15
                        text: "Session"
                    }
                    Text {
                        color: welcome.mutedColor
                        font.pixelSize: 12
                        text: northstarSessionStatus
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                    Text {
                        color: welcome.mutedColor
                        font.pixelSize: 12
                        text: northstarPlatform
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                }
            }
        }

        Item { height: 1; width: 1 }

        Row {
            spacing: 10
            width: parent.width

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: welcome.mutedColor
                font.pixelSize: 12
                text: "Northstar " + northstarVersion + " · " + northstarBuild
            }

            Item { height: 1; width: parent.width - 220 }

            Button {
                text: "Done"
                onClicked: welcome.close()
            }
        }
    }
}
