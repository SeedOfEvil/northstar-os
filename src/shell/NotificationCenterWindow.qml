import QtQuick
import QtQuick.Controls

Window {
    id: notifications

    property var center
    property var state
    property var targetScreen
    property int panelHeight: 44
    property int desktopMargin: 24
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property color surfaceBackground: state && state.darkMode ? "#171a21" : "#f4f6fb"
    property color surfaceForeground: state && state.darkMode ? "#f5f7fb" : "#1e2430"
    property color surfaceMuted: state && state.darkMode ? "#a9b1c2" : "#637083"
    property color surfaceAccent: state && state.darkMode ? "#79b8ff" : "#1769aa"
    property color surfaceRaised: state && state.darkMode ? "#252b36" : "#e8edf5"

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.Tool
    modality: Qt.NonModal
    title: "Northstar Notifications"
    width: Math.min(420, screenWidth - (desktopMargin * 2))
    height: Math.min(540, Math.max(360, screenHeight - panelHeight - (desktopMargin * 2)))
    x: screenX + screenWidth - width - desktopMargin
    y: screenY + panelHeight + desktopMargin

    function openPanel() {
        if (center) {
            center.markAllRead()
        }
        show()
        raise()
        requestActivate()
    }

    function togglePanel() {
        if (visible) {
            hide()
        } else {
            openPanel()
        }
    }

    function closePanel() {
        hide()
    }

    Rectangle {
        anchors.fill: parent
        color: notifications.surfaceBackground
        border.color: notifications.surfaceAccent
        border.width: 1
        radius: 12

        Column {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Item {
                height: 42
                width: parent.width

                Column {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        color: notifications.surfaceForeground
                        font.bold: true
                        font.pixelSize: 20
                        text: "Notifications"
                    }

                    Text {
                        color: notifications.surfaceMuted
                        font.pixelSize: 11
                        text: notifications.center && notifications.center.notifications.length > 0
                            ? notifications.center.notifications.length + " recent event(s)"
                            : "You're all caught up"
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    Button {
                        text: "Clear"
                        enabled: !!notifications.center && notifications.center.notifications.length > 0
                        onClicked: notifications.center.clearNotifications()
                    }

                    Button {
                        text: "Close"
                        onClicked: notifications.closePanel()
                    }
                }
            }

            Rectangle {
                color: notifications.surfaceBackground
                border.color: notifications.surfaceMuted
                border.width: 1
                height: parent.height - 54
                radius: 8
                width: parent.width

                ListView {
                    id: notificationList
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    model: notifications.center ? notifications.center.notifications : []
                    spacing: 8

                    delegate: Rectangle {
                        required property var modelData

                        color: notificationMouse.containsMouse ? notifications.surfaceAccent : notifications.surfaceRaised
                        height: 82
                        radius: 8
                        width: notificationList.width - 8

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            color: modelData.kind === "error" ? "#c34f65"
                                : modelData.kind === "warning" ? "#d39a4a" : notifications.surfaceAccent
                            radius: 8
                            width: 4
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.right: dismissButton.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4

                            Text {
                                color: notifications.surfaceForeground
                                elide: Text.ElideRight
                                font.bold: true
                                font.pixelSize: 13
                                text: modelData.title
                                width: parent.width
                            }

                            Text {
                                color: notifications.surfaceMuted
                                elide: Text.ElideRight
                                font.pixelSize: 11
                                maximumLineCount: 2
                                text: modelData.body
                                width: parent.width
                                wrapMode: Text.WordWrap
                            }
                        }

                        Text {
                            anchors.right: dismissButton.left
                            anchors.rightMargin: 8
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 7
                            color: notifications.surfaceMuted
                            font.pixelSize: 9
                            text: modelData.timestamp
                            width: Math.min(150, parent.width * 0.4)
                            elide: Text.ElideLeft
                            horizontalAlignment: Text.AlignRight
                        }

                        Rectangle {
                            id: dismissButton
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            color: dismissMouse.containsMouse ? notifications.surfaceAccent : "transparent"
                            height: 26
                            radius: 5
                            width: 26

                            Text {
                                anchors.centerIn: parent
                                color: notifications.surfaceForeground
                                font.bold: true
                                font.pixelSize: 14
                                text: "×"
                            }

                            MouseArea {
                                id: dismissMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: notifications.center.dismissNotification(modelData.id)
                            }
                        }

                        MouseArea {
                            id: notificationMouse
                            anchors.fill: parent
                            anchors.rightMargin: dismissButton.width + 10
                            hoverEnabled: true
                            onClicked: notifications.center.markRead(modelData.id)
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }

                Text {
                    anchors.centerIn: parent
                    color: notifications.surfaceMuted
                    font.pixelSize: 13
                    text: "No notifications yet"
                    visible: notificationList.count === 0
                }
            }
        }
    }
}
