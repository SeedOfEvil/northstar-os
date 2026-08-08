import QtQuick
import QtQuick.Controls

Window {
    id: menu

    property var launcherController
    property var overviewWindow
    property var settingsSurface
    property var sessionController
    property var state
    property var targetScreen
    property int panelHeight: 96
    property int desktopMargin: 18
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property color surfaceBackground: state && state.darkMode ? "#171a21" : "#f4f6fb"
    property color surfaceForeground: state && state.darkMode ? "#f5f7fb" : "#1e2430"
    property color surfaceMuted: state && state.darkMode ? "#a9b1c2" : "#637083"
    property color surfaceAccent: state && state.darkMode ? "#79b8ff" : "#1769aa"
    property bool canLogout: sessionController !== null
        && sessionController !== undefined
        && sessionController.available
    property string logoutError: ""

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    title: "Northstar Menu"

    width: 320
    height: 330
    x: screenX + screenWidth - width - desktopMargin
    y: screenY + panelHeight + 8

    function openMenu() {
        show()
        raise()
        requestActivate()
    }

    function closeMenu() {
        hide()
    }

    function triggerAction(action) {
        if (action === "applications") {
            closeMenu()
            overviewWindow.show()
            overviewWindow.raise()
            overviewWindow.requestActivate()
        } else if (action === "settings") {
            closeMenu()
            settingsSurface.openSettings()
        } else if (action === "refresh") {
            closeMenu()
            launcherController.refreshApplications()
        } else if (action === "theme") {
            closeMenu()
            state.toggleDarkMode()
        } else if (action === "terminal") {
            closeMenu()
            launcherController.launchTerminal()
        } else if (action === "browser") {
            closeMenu()
            launcherController.launchBrowser()
        } else if (action === "logout") {
            if (menu.canLogout) {
                menu.logoutError = ""
                logoutDialog.open()
            } else {
                closeMenu()
                Qt.quit()
            }
        }
    }

    Dialog {
        id: logoutDialog
        modal: true
        title: "Log out of Northstar?"
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: 420
        x: (menu.width - width) / 2
        y: (menu.height - height) / 2

        contentItem: Column {
            spacing: 10
            width: 360

            Text {
                color: menu.surfaceForeground
                text: "This ends the Northstar shell and its supervised compositor. Other user applications are not targeted."
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Text {
                color: "#c34f65"
                text: menu.logoutError
                visible: text.length > 0
                wrapMode: Text.WordWrap
                width: parent.width
            }
        }

        onAccepted: {
            const requested = menu.canLogout && menu.sessionController.requestEndSession()
            if (requested) {
                menu.hide()
            } else {
                menu.logoutError = "The supervised Northstar session is no longer available."
            }
        }

        onRejected: menu.logoutError = ""
    }

    Rectangle {
        anchors.fill: parent
        color: menu.surfaceBackground
        border.color: menu.surfaceAccent
        border.width: 1
        radius: 10

        Column {
            id: menuColumn
            anchors.fill: parent
            anchors.margins: 10
            spacing: 2

            Text {
                color: menu.surfaceMuted
                font.pixelSize: 11
                leftPadding: 8
                text: "Northstar"
                verticalAlignment: Text.AlignVCenter
                width: parent.width
                height: 24
            }

            ListView {
                id: menuList
                clip: true
                interactive: false
                model: [
                    { kind: "action", id: "applications", label: "Applications" },
                    { kind: "action", id: "refresh", label: "Refresh Applications" },
                    { kind: "separator" },
                    { kind: "action", id: "settings", label: "Settings" },
                    { kind: "action", id: "theme", label: "" },
                    { kind: "action", id: "terminal", label: "Launch Terminal" },
                    { kind: "action", id: "browser", label: "Launch Firefox" },
                    { kind: "separator" },
                    { kind: "action", id: "logout", label: "Log Out of Northstar" }
                ]
                width: parent.width
                height: parent.height - 26

                delegate: Rectangle {
                    required property var modelData

                    color: modelData.kind === "separator"
                        ? menu.surfaceMuted
                        : menuItemMouse.containsMouse ? menu.surfaceAccent : "transparent"
                    height: modelData.kind === "separator" ? 1 : 38
                    radius: modelData.kind === "separator" ? 0 : 6
                    width: menuList.width

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        color: menu.surfaceForeground
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        text: modelData.kind === "separator" ? "" : modelData.id === "theme"
                            ? (menu.state && menu.state.darkMode
                                ? "Use light appearance" : "Use dark appearance")
                            : modelData.id === "logout" && !menu.canLogout
                            ? "Quit Northstar Shell"
                            : modelData.label
                        verticalAlignment: Text.AlignVCenter
                    }

                    MouseArea {
                        id: menuItemMouse
                        anchors.fill: parent
                        enabled: modelData.kind !== "separator"
                        hoverEnabled: true
                        onClicked: menu.triggerAction(modelData.id)
                    }
                }
            }
        }
    }
}
