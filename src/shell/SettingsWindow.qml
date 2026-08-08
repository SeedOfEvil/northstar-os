import QtQuick
import QtQuick.Controls

Window {
    id: settings

    property var state
    property var launcherController
    property var sessionController
    property bool hasSessionController: sessionController !== null && sessionController !== undefined
    property var targetScreen
    property string shellApplicationName: "northstar-shell"
    property string shellApplicationVersion: "0.1.0"
    property int panelHeight: 96
    property int desktopMargin: 24
    property string selectedSection: "appearance"
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property color surfaceBackground: state && state.darkMode ? "#171a21" : "#f4f6fb"
    property color surfaceForeground: state && state.darkMode ? "#f5f7fb" : "#1e2430"
    property color surfaceMuted: state && state.darkMode ? "#a9b1c2" : "#637083"
    property color surfaceAccent: state && state.darkMode ? "#79b8ff" : "#1769aa"
    property color surfaceRaised: state && state.darkMode ? "#252b36" : "#e8edf5"
    property string catalogStatus: ""

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.ApplicationModal
    title: "Northstar Settings"

    width: Math.min(900, screenWidth - (desktopMargin * 2))
    height: Math.max(520, screenHeight - panelHeight - (desktopMargin * 2))
    x: screenX + Math.max(desktopMargin, (screenWidth - width) / 2)
    y: screenY + panelHeight + desktopMargin

    function openSettings() {
        show()
        raise()
        requestActivate()
        if (sessionController) {
            sessionController.refresh()
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: settings.visible && settings.hasSessionController
        onTriggered: settings.sessionController.refresh()
    }

    Timer {
        id: catalogStatusTimer
        interval: 2500
        onTriggered: settings.catalogStatus = ""
    }

    Dialog {
        id: endSessionDialog
        modal: true
        title: "End Northstar session?"
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: 420
        x: (settings.width - width) / 2
        y: (settings.height - height) / 2

        contentItem: Text {
            color: settings.surfaceForeground
            text: "This closes the Northstar shell and its supervised compositor. Other user applications are not targeted."
            wrapMode: Text.WordWrap
            width: 360
        }

        onAccepted: {
            const requested = settings.hasSessionController && settings.sessionController.requestEndSession()
            if (requested) {
                settings.hide()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: settings.surfaceBackground
        border.color: settings.surfaceAccent
        border.width: 1
        radius: 12

        Column {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 18

            Row {
                width: parent.width
                spacing: 12

                Column {
                    width: parent.width - closeButton.width - parent.spacing
                    spacing: 3

                    Text {
                        color: settings.surfaceForeground
                        font.bold: true
                        font.pixelSize: 24
                        text: "Settings"
                    }

                    Text {
                        color: settings.surfaceMuted
                        font.pixelSize: 12
                        text: "Northstar desktop preferences"
                    }
                }

                Button {
                    id: closeButton
                    text: "Close"
                    onClicked: settings.hide()
                }
            }

            Row {
                width: parent.width
                height: parent.height - 74
                spacing: 18

                Rectangle {
                    color: settings.surfaceRaised
                    height: parent.height
                    radius: 8
                    width: 190

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        Repeater {
                            model: [
                                { id: "appearance", label: "Appearance" },
                                { id: "session", label: "Session" },
                                { id: "about", label: "About Northstar" }
                            ]

                            delegate: Rectangle {
                                required property var modelData

                                color: settings.selectedSection === modelData.id ? settings.surfaceAccent : "transparent"
                                height: 40
                                radius: 6
                                width: parent.width

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: settings.selectedSection === modelData.id ? settings.surfaceForeground : settings.surfaceMuted
                                    font.bold: settings.selectedSection === modelData.id
                                    font.pixelSize: 13
                                    text: modelData.label
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: settings.selectedSection = modelData.id
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    color: settings.surfaceRaised
                    height: parent.height
                    radius: 8
                    width: parent.width - 208

                    Column {
                        id: appearancePage
                        anchors.fill: parent
                        anchors.margins: 22
                        spacing: 16
                        visible: settings.selectedSection === "appearance"

                        Text {
                            color: settings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 20
                            text: "Appearance"
                        }

                        Text {
                            color: settings.surfaceMuted
                            font.pixelSize: 13
                            text: "Choose how the Northstar shell presents its panels and surfaces."
                            wrapMode: Text.WordWrap
                            width: parent.width
                        }

                        Rectangle {
                            color: settings.surfaceBackground
                            border.color: settings.surfaceMuted
                            border.width: 1
                            height: 76
                            radius: 8
                            width: parent.width

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 3
                                    width: parent.width - appearanceToggle.width - parent.spacing

                                    Text {
                                        color: settings.surfaceForeground
                                        font.bold: true
                                        font.pixelSize: 14
                                        text: "Dark appearance"
                                    }

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 12
                                        text: "Use the dark Northstar design tokens."
                                    }
                                }

                                CheckBox {
                                    id: appearanceToggle
                                    anchors.verticalCenter: parent.verticalCenter
                                    checked: settings.state ? settings.state.darkMode : true
                                    text: checked ? "On" : "Off"
                                    onToggled: {
                                        if (settings.state) {
                                            settings.state.setDarkMode(checked)
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            color: settings.surfaceMuted
                            font.pixelSize: 12
                            text: "More appearance controls will be added as the desktop settings service matures."
                            wrapMode: Text.WordWrap
                            width: parent.width
                        }
                    }

                    Column {
                        id: sessionPage
                        anchors.fill: parent
                        anchors.margins: 22
                        spacing: 16
                        visible: settings.selectedSection === "session"

                        Text {
                            color: settings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 20
                            text: "Session"
                        }

                        Text {
                            color: settings.surfaceMuted
                            font.pixelSize: 13
                            text: "Review the supervised Northstar session and refresh its application catalog."
                            wrapMode: Text.WordWrap
                            width: parent.width
                        }

                        Rectangle {
                            color: settings.surfaceBackground
                            border.color: settings.surfaceMuted
                            border.width: 1
                            height: 286
                            radius: 8
                            width: parent.width

                            Column {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 10

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Session"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: settings.hasSessionController && settings.sessionController.available
                                            ? settings.sessionController.state : "Not supervised"
                                    }
                                }

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Supervisor PID"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: settings.hasSessionController && settings.sessionController.supervisorPid > 0
                                            ? settings.sessionController.supervisorPid : "—"
                                    }
                                }

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Wayland display"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: settings.hasSessionController && settings.sessionController.available
                                            ? settings.sessionController.waylandDisplay : "—"
                                    }
                                }

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Shell PID"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: settings.hasSessionController && settings.sessionController.shellPid > 0
                                            ? settings.sessionController.shellPid : "—"
                                    }
                                }

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Compositor PID"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: settings.hasSessionController && settings.sessionController.compositorPid > 0
                                            ? settings.sessionController.compositorPid : "—"
                                    }
                                }

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Restart count"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: settings.hasSessionController ? settings.sessionController.restartCount : 0
                                    }
                                }

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Last event"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: settings.hasSessionController && settings.sessionController.lastEvent.length > 0
                                            ? settings.sessionController.lastEvent : "—"
                                    }
                                }

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Desktop"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: settings.state && settings.state.activeWindowTitle
                                            ? settings.state.activeWindowTitle : "Desktop"
                                    }
                                }

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Display"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: settings.screenWidth + " x " + settings.screenHeight
                                    }
                                }

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Applications"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: settings.launcherController && settings.launcherController.applications
                                            ? settings.launcherController.applications.length : 0
                                    }
                                }
                            }
                        }

                        Button {
                            text: "Refresh Application Catalog"
                            onClicked: {
                                if (settings.launcherController) {
                                    const changed = settings.launcherController.refreshApplications()
                                    settings.catalogStatus = changed
                                        ? "Application catalog refreshed."
                                        : "Application catalog is already current."
                                    catalogStatusTimer.restart()
                                } else {
                                    settings.catalogStatus = "Application catalog is unavailable."
                                    catalogStatusTimer.restart()
                                }
                            }
                        }

                        Text {
                            color: settings.surfaceMuted
                            font.pixelSize: 12
                            text: settings.catalogStatus
                            visible: text.length > 0
                        }

                        Button {
                            enabled: settings.hasSessionController && settings.sessionController.available
                            text: "End Northstar Session"
                            onClicked: endSessionDialog.open()
                        }
                    }

                    Column {
                        id: aboutPage
                        anchors.fill: parent
                        anchors.margins: 22
                        spacing: 16
                        visible: settings.selectedSection === "about"

                        Text {
                            color: settings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 20
                            text: "About Northstar"
                        }

                        Text {
                            color: settings.surfaceMuted
                            font.pixelSize: 13
                            text: "Northstar is a FreeBSD-native desktop experience built around a small, testable shell."
                            wrapMode: Text.WordWrap
                            width: parent.width
                        }

                        Rectangle {
                            color: settings.surfaceBackground
                            border.color: settings.surfaceMuted
                            border.width: 1
                            height: 132
                            radius: 8
                            width: parent.width

                            Column {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 10

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Application"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: settings.shellApplicationName
                                    }
                                }

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Version"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: settings.shellApplicationVersion
                                    }
                                }

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 13
                                        text: "Desktop"
                                        width: 150
                                    }

                                    Text {
                                        color: settings.surfaceForeground
                                        font.pixelSize: 13
                                        text: "Northstar / Wayland"
                                    }
                                }
                            }
                        }

                        Text {
                            color: settings.surfaceMuted
                            font.pixelSize: 12
                            text: "This development build is running from the user-local Northstar prefix."
                            wrapMode: Text.WordWrap
                            width: parent.width
                        }
                    }
                }
            }
        }
    }
}
