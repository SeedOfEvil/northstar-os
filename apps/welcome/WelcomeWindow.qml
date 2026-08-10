import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

ApplicationWindow {
    id: welcome

    LunarPalette { id: lunar; darkMode: northstarDarkMode }

    property color backgroundColor: lunar.background
    property color panelColor: lunar.panel
    property color raisedColor: lunar.raised
    property color foregroundColor: lunar.foreground
    property color mutedColor: lunar.muted
    property color accentColor: lunar.accent
    property string statusMessage: "Choose a starting point for your Northstar desktop."

    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    height: 680
    minimumHeight: 620
    minimumWidth: 760
    title: "Northstar Welcome"
    visible: true
    width: 820

    background: NorthstarWindowFrame {
        darkMode: lunar.darkMode
    }

    Column {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 12

        NorthstarWindowTitleBar {
            closeDestroysWindow: true
            iconSource: "qrc:/Northstar/Welcome/northstar-welcome.svg"
            maximized: welcome.visibility === Window.Maximized
            lunarPalette: lunar
            subtitle: "Start exploring your Northstar desktop"
            title: "Welcome"
            width: parent.width
            window: welcome
            onMaximizeRequested: {
                if (welcome.visibility === Window.Maximized) welcome.showNormal()
                else welcome.showMaximized()
            }
        }

        Row {
            spacing: 18
            width: parent.width

            Image {
                anchors.verticalCenter: parent.verticalCenter
                fillMode: Image.PreserveAspectFit
                height: 70
                source: "qrc:/Northstar/Welcome/northstar-welcome.svg"
                width: 70
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
                height: 116
                border.color: homeMouse.containsMouse ? lunar.accentBright : lunar.borderSoft
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
                height: 116
                border.color: desktopMouse.containsMouse ? lunar.accentBright : lunar.borderSoft
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
                height: 116
                border.color: guideMouse.containsMouse ? lunar.accentBright : lunar.borderSoft
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
            height: 76
            border.color: lunar.borderSoft
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
            height: 110
            border.color: lunar.borderSoft
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

    NativeWindowResizeHandler {
        resizingEnabled: welcome.visibility !== Window.Maximized
        window: welcome
    }
}
