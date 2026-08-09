import QtQuick
import QtQuick.Controls

Window {
    id: menu

    LunarPalette {
        id: lunar
        darkMode: menu.state ? menu.state.darkMode : true
    }

    property var launcherController
    property var powerController
    property var overviewWindow
    property var settingsSurface
    property var filesWindow
    property var softwareWindow
    property var sessionController
    property var shortcutCatalog
    property var state
    property var targetScreen
    property int panelHeight: 44
    property int desktopMargin: 18
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property color surfaceBackground: lunar.panelStrong
    property color surfaceForeground: lunar.foreground
    property color surfaceMuted: lunar.muted
    property color surfaceAccent: lunar.accent
    property int menuRowHeight: 40
    property int menuSeparatorHeight: 1
    property int menuContentHeight: (11 * menuRowHeight) + (3 * menuSeparatorHeight)
    property bool canLogout: sessionController !== null
        && sessionController !== undefined
        && sessionController.available
    property bool canPower: powerController !== null
        && powerController !== undefined
        && powerController.available
    property string logoutError: ""
    property string powerError: ""
    property string pendingPowerAction: ""

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    title: "Northstar Menu"

    width: 292
    height: menuContentHeight + 134
    x: screenX + desktopMargin
    y: screenY + panelHeight + 8

    WindowDragController {
        id: menuDrag
        window: menu
        screenX: menu.screenX
        screenY: menu.screenY
        screenWidth: menu.screenWidth
        screenHeight: menu.screenHeight
        topInset: menu.panelHeight
        bottomInset: 12
        defaultX: menu.screenX + menu.desktopMargin
        defaultY: menu.screenY + menu.panelHeight + 8
    }

    function openMenu() {
        menuDrag.prepareForOpen()
        show()
        raise()
        requestActivate()
    }

    function closeMenu() {
        hide()
    }

    function actionIconName(action) {
        switch (action) {
        case "applications":
            return "applications"
        case "files":
            return "files"
        case "software":
            return "software"
        case "refresh":
            return "quick-settings"
        case "settings":
        case "theme":
            return "settings"
        case "terminal":
            return "terminal"
        case "browser":
            return "browser"
        case "logout":
        case "restart":
        case "shutdown":
            return "power"
        default:
            return "info"
        }
    }

    function shortcutForAction(action) {
        return menu.shortcutCatalog ? menu.shortcutCatalog.sequenceFor(action) : ""
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
        } else if (action === "files") {
            closeMenu()
            filesWindow.openBrowser()
        } else if (action === "software") {
            closeMenu()
            softwareWindow.openSoftware()
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
        } else if (action === "restart" || action === "shutdown") {
            menu.pendingPowerAction = action
            menu.powerError = ""
            powerDialog.open()
        }
    }

    Dialog {
        id: logoutDialog
        modal: true
        title: "Log out of Northstar?"
        standardButtons: Dialog.Cancel | Dialog.Ok
        padding: 16
        width: menu.width - 20
        x: (menu.width - width) / 2
        y: (menu.height - height) / 2

        background: Rectangle {
            color: menu.surfaceBackground
            border.color: menu.surfaceAccent
            border.width: 1
            radius: lunar.radiusMedium
        }

        contentItem: Column {
            spacing: 10
            width: logoutDialog.width - (2 * logoutDialog.padding)

            Text {
                id: menuDragHandle
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

    Dialog {
        id: powerDialog
        modal: true
        title: menu.pendingPowerAction === "restart" ? "Restart Northstar?" : "Shut down Northstar?"
        standardButtons: Dialog.Cancel | Dialog.Ok
        padding: 16
        width: menu.width - 20
        x: (menu.width - width) / 2
        y: (menu.height - height) / 2

        background: Rectangle {
            color: menu.surfaceBackground
            border.color: menu.surfaceAccent
            border.width: 1
            radius: lunar.radiusMedium
        }

        contentItem: Column {
            spacing: 10
            width: powerDialog.width - (2 * powerDialog.padding)

            Text {
                color: menu.surfaceForeground
                text: menu.canPower
                    ? (menu.pendingPowerAction === "restart"
                        ? "This restarts the FreeBSD system and closes Northstar."
                        : "This shuts down the FreeBSD system and closes Northstar.")
                    : "Power controls are not configured for this system."
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Text {
                color: "#c34f65"
                text: menu.powerError
                visible: text.length > 0
                wrapMode: Text.WordWrap
                width: parent.width
            }
        }

        onAccepted: {
            const requested = menu.canPower && menu.pendingPowerAction === "restart"
                ? menu.powerController.requestRestart()
                : menu.canPower && menu.pendingPowerAction === "shutdown"
                ? menu.powerController.requestShutdown()
                : false
            if (requested) {
                menu.hide()
            } else {
                menu.powerError = menu.powerController && menu.powerController.statusMessage.length > 0
                    ? menu.powerController.statusMessage
                    : "The requested power action is unavailable."
                powerDialog.open()
            }
        }

        onRejected: menu.powerError = ""
    }

    Rectangle {
        anchors.fill: parent
        color: menu.surfaceBackground
        border.color: lunar.border
        border.width: 1
        radius: lunar.radiusPanel

        gradient: Gradient {
            GradientStop { position: 0.0; color: lunar.panelStrong }
            GradientStop { position: 1.0; color: lunar.panel }
        }

        Column {
            id: menuColumn
            anchors.fill: parent
            anchors.margins: 12
            spacing: 4

            Text {
                color: menu.surfaceForeground
                font.bold: true
                font.pixelSize: 14
                leftPadding: 10
                text: "Northstar"
                verticalAlignment: Text.AlignVCenter
                width: parent.width
                height: 24

                NativeWindowMoveHandler {
                    window: menu
                }
            }

            ListView {
                id: menuList
                clip: true
                interactive: false
                model: [
                    { kind: "action", id: "applications", label: "Applications" },
                    { kind: "action", id: "software", label: "Software Center" },
                    { kind: "action", id: "files", label: "Open Files" },
                    { kind: "action", id: "refresh", label: "Refresh Applications" },
                    { kind: "separator" },
                    { kind: "action", id: "settings", label: "Settings" },
                    { kind: "action", id: "theme", label: "" },
                    { kind: "action", id: "terminal", label: "Launch Terminal" },
                    { kind: "action", id: "browser", label: "Launch Firefox" },
                    { kind: "separator" },
                    { kind: "action", id: "logout", label: "Log Out of Northstar" },
                    { kind: "separator" },
                    { kind: "action", id: "restart", label: "Restart FreeBSD" },
                    { kind: "action", id: "shutdown", label: "Shut Down FreeBSD" }
                ]
                width: parent.width
                height: parent.height - 92

                delegate: Rectangle {
                    required property var modelData

                    color: modelData.kind === "separator"
                        ? menu.surfaceMuted
                        : menuItemMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: modelData.kind === "separator" ? menu.menuSeparatorHeight : menu.menuRowHeight
                    radius: modelData.kind === "separator" ? 0 : lunar.radiusSmall
                    width: menuList.width

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 42
                        anchors.rightMargin: shortcutText.visible ? shortcutText.implicitWidth + 16 : 10
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

                    Text {
                        id: shortcutText
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        color: menu.surfaceMuted
                        font.pixelSize: 10
                        text: modelData.kind === "separator"
                            ? "" : menu.shortcutForAction(modelData.id)
                        visible: text.length > 0
                    }

                    NorthstarIcon {
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        height: 24
                        visible: modelData.kind !== "separator"
                        width: 24
                        iconName: menu.actionIconName(modelData.id)
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

            Rectangle {
                color: lunar.field
                border.color: lunar.borderSoft
                border.width: 1
                height: 52
                radius: lunar.radiusMedium
                width: parent.width

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        color: lunar.accentSoft
                        height: 34
                        radius: 17
                        width: 34

                        Image {
                            anchors.centerIn: parent
                            fillMode: Image.PreserveAspectFit
                            height: 25
                            mipmap: true
                            smooth: true
                            source: northstarLogoSource
                            sourceClipRect: Qt.rect(270, 245, 485, 335)
                            width: 25
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 1

                        Text {
                            color: menu.surfaceForeground
                            font.bold: true
                            font.pixelSize: 12
                            text: "Northstar User"
                        }

                        Text {
                            color: menu.surfaceMuted
                            font.pixelSize: 10
                            text: "FreeBSD workspace"
                        }
                    }
                }
            }
        }
    }
}
