import QtQuick
import QtQuick.Controls

Window {
    id: settings

    LunarPalette {
        id: lunar
        darkMode: settings.state ? settings.state.darkMode : true
    }

    property var state
    property var desktopLayoutController
    property var launcherController
    property var sessionController
    property bool hasSessionController: sessionController !== null && sessionController !== undefined
    property bool sessionFailed: settings.hasSessionController
        && settings.sessionController.state === "failed"
    property var targetScreen
    property string shellApplicationName: "northstar-shell"
    property string shellApplicationVersion: "0.1.0"
    property int panelHeight: 44
    property int desktopMargin: 24
    property string selectedSection: "appearance"
    property string layoutStatus: ""
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property int minimumSurfaceWidth: 640
    property int minimumSurfaceHeight: 420
    property bool maximized: false
    property point normalGeometryPosition: Qt.point(0, 0)
    property size normalGeometrySize: Qt.size(0, 0)
    property bool dragging: false
    property point dragOrigin: Qt.point(0, 0)
    property point windowOrigin: Qt.point(0, 0)
    property point resizeOrigin: Qt.point(0, 0)
    property size resizeSize: Qt.size(0, 0)
    property color surfaceBackground: lunar.panelStrong
    property color surfaceForeground: lunar.foreground
    property color surfaceMuted: lunar.muted
    property color surfaceAccent: lunar.accent
    property color surfaceRaised: lunar.raised
    property string catalogStatus: ""

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.NonModal
    title: "Northstar Settings"

    minimumWidth: settings.minimumSurfaceWidth
    minimumHeight: settings.minimumSurfaceHeight
    width: Math.min(900, Math.max(minimumSurfaceWidth, screenWidth - (desktopMargin * 2)))
    height: Math.min(screenHeight - panelHeight - desktopMargin, Math.max(minimumSurfaceHeight, 520))
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

    function toggleMaximize() {
        if (settings.maximized) {
            settings.x = settings.normalGeometryPosition.x
            settings.y = settings.normalGeometryPosition.y
            settings.width = settings.normalGeometrySize.width
            settings.height = settings.normalGeometrySize.height
            settings.maximized = false
            return
        }

        settings.normalGeometryPosition = Qt.point(settings.x, settings.y)
        settings.normalGeometrySize = Qt.size(settings.width, settings.height)
        settings.x = settings.screenX
        settings.y = settings.screenY + settings.panelHeight
        settings.width = settings.screenWidth
        settings.height = Math.max(settings.minimumSurfaceHeight, settings.screenHeight - settings.panelHeight)
        settings.maximized = true
    }

    function beginDrag(mouseX, mouseY) {
        if (settings.maximized) {
            return
        }
        if (settings.startSystemMove()) {
            settings.dragging = false
            return
        }
        settings.dragging = true
        settings.dragOrigin = Qt.point(mouseX, mouseY)
        settings.windowOrigin = Qt.point(settings.x, settings.y)
    }

    function updateDrag(mouseX, mouseY) {
        if (!settings.dragging || settings.maximized) {
            return
        }
        const deltaX = mouseX - settings.dragOrigin.x
        const deltaY = mouseY - settings.dragOrigin.y
        const maxX = settings.screenX + settings.screenWidth - settings.width
        const maxY = settings.screenY + settings.screenHeight - settings.height
        settings.x = Math.max(settings.screenX, Math.min(maxX, settings.windowOrigin.x + deltaX))
        settings.y = Math.max(settings.screenY + settings.panelHeight, Math.min(maxY, settings.windowOrigin.y + deltaY))
    }

    function endDrag() {
        settings.dragging = false
    }

    function beginResize(mouseX, mouseY) {
        if (settings.maximized) {
            return
        }
        settings.resizeOrigin = Qt.point(mouseX, mouseY)
        settings.resizeSize = Qt.size(settings.width, settings.height)
    }

    function updateResize(mouseX, mouseY) {
        if (settings.maximized) {
            return
        }
        const deltaX = mouseX - settings.resizeOrigin.x
        const deltaY = mouseY - settings.resizeOrigin.y
        settings.width = Math.min(settings.screenX + settings.screenWidth - settings.x,
                                  Math.max(settings.minimumSurfaceWidth, settings.resizeSize.width + deltaX))
        settings.height = Math.min(settings.screenY + settings.screenHeight - settings.y,
                                   Math.max(settings.minimumSurfaceHeight, settings.resizeSize.height + deltaY))
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

    Timer {
        id: layoutStatusTimer
        interval: 2500
        onTriggered: settings.layoutStatus = ""
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

    Dialog {
        id: restartShellDialog
        modal: true
        title: "Restart Northstar shell?"
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: 420
        x: (settings.width - width) / 2
        y: (settings.height - height) / 2

        contentItem: Text {
            color: settings.surfaceForeground
            text: "This restarts only the supervised Northstar shell. The compositor and other user applications are not targeted."
            wrapMode: Text.WordWrap
            width: 360
        }

        onAccepted: {
            const requested = settings.hasSessionController
                && settings.sessionController.requestShellRestart()
            if (requested) {
                settings.hide()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: settings.surfaceBackground
        border.color: lunar.border
        border.width: 1
        radius: lunar.radiusPanel

        gradient: Gradient {
            GradientStop { position: 0.0; color: lunar.panelStrong }
            GradientStop { position: 1.0; color: lunar.panel }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 18

            Item {
                id: titleBar
                width: parent.width
                height: 48

                MouseArea {
                    anchors.fill: parent
                    onPressed: settings.beginDrag(mouse.x, mouse.y)
                    onPositionChanged: settings.updateDrag(mouse.x, mouse.y)
                    onReleased: settings.endDrag()
                    onCanceled: settings.endDrag()
                }

                Column {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - closeButton.width - 12
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

                Row {
                    id: closeButton
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    Rectangle {
                        color: minimizeMouse.containsMouse ? settings.surfaceAccent : settings.surfaceRaised
                        height: 32
                        radius: 16
                        width: 34

                        Text {
                            anchors.centerIn: parent
                            color: settings.surfaceForeground
                            font.pixelSize: 15
                            text: "_"
                        }

                        MouseArea {
                            id: minimizeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: settings.hide()
                        }
                    }

                    Rectangle {
                        color: maximizeMouse.containsMouse ? settings.surfaceAccent : settings.surfaceRaised
                        height: 32
                        radius: 16
                        width: 34

                        Text {
                            anchors.centerIn: parent
                            color: settings.surfaceForeground
                            font.pixelSize: 14
                            text: settings.maximized ? "❐" : "□"
                        }

                        MouseArea {
                            id: maximizeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: settings.toggleMaximize()
                        }
                    }

                    Rectangle {
                        color: closeMouse.containsMouse ? lunar.danger : settings.surfaceRaised
                        height: 32
                        radius: 16
                        width: 34

                        Text {
                            anchors.centerIn: parent
                            color: settings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 15
                            text: "×"
                        }

                        MouseArea {
                            id: closeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: settings.hide()
                        }
                    }
                }
            }

            Row {
                width: parent.width
                height: parent.height - 74
                spacing: 18

                Rectangle {
                    color: lunar.panel
                    border.color: lunar.borderSoft
                    border.width: 1
                    height: parent.height
                    radius: lunar.radiusLarge
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

                                color: settings.selectedSection === modelData.id
                                    ? lunar.accentSoft : sectionMouse.containsMouse ? lunar.raisedHover : "transparent"
                                height: 40
                                radius: lunar.radiusSmall
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
                                    id: sectionMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
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

                        Button {
                            enabled: !!settings.desktopLayoutController
                            text: "Reset Desktop Icon Layout"
                            onClicked: {
                                settings.desktopLayoutController.reset()
                                settings.layoutStatus = "Desktop icon layout reset to the default column."
                                layoutStatusTimer.restart()
                            }
                        }

                        Text {
                            color: settings.surfaceMuted
                            font.pixelSize: 12
                            text: settings.layoutStatus
                            visible: text.length > 0
                            wrapMode: Text.WordWrap
                            width: parent.width
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
                                        text: settings.hasSessionController && settings.sessionController.state.length > 0
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

                        Rectangle {
                            color: settings.state && settings.state.darkMode ? "#3b2328" : "#fff0f0"
                            border.color: settings.state && settings.state.darkMode ? "#d96c7a" : "#c7465b"
                            border.width: 1
                            height: sessionRecoveryMessage.implicitHeight + 24
                            radius: 8
                            visible: settings.sessionFailed
                            width: parent.width

                            Text {
                                id: sessionRecoveryMessage
                                anchors.fill: parent
                                anchors.margins: 12
                                color: settings.state && settings.state.darkMode ? "#ffd9de" : "#7f1d2d"
                                font.pixelSize: 12
                                text: "The supervised session reached a terminal failure. Save your work and restart Northstar from the console or login session before testing again."
                                wrapMode: Text.WordWrap
                            }
                        }

                        Button {
                            enabled: settings.hasSessionController
                                && settings.sessionController.restartable
                            text: "Restart Northstar Shell"
                            onClicked: restartShellDialog.open()
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

    Rectangle {
        id: resizeHandle
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: settings.maximized ? "transparent" : settings.surfaceAccent
        height: 18
        opacity: settings.maximized ? 0 : 0.85
        visible: !settings.maximized
        width: 18
        z: 10

        Text {
            anchors.centerIn: parent
            color: settings.surfaceBackground
            font.pixelSize: 12
            rotation: 45
            text: "···"
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            onPressed: settings.beginResize(mouse.x, mouse.y)
            onPositionChanged: settings.updateResize(mouse.x, mouse.y)
        }
    }
}
