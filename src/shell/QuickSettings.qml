import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: quickSettings

    LunarPalette {
        id: lunar
        darkMode: quickSettings.state ? quickSettings.state.darkMode : true
    }

    AuroraMetrics { id: metrics }

    property var state
    property var controller
    property var powerController
    property var settingsWindow
    property var systemMenu
    property var targetScreen
    property int panelHeight: metrics.panelHeight
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property color surfaceBackground: lunar.panelStrong
    property color surfaceForeground: lunar.foreground
    property color surfaceMuted: lunar.muted
    property color surfaceAccent: lunar.accent
    property color surfaceRaised: lunar.raised

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.Tool
    modality: Qt.NonModal
    title: "Northstar Quick Settings"

    width: Math.min(metrics.quickSettingsWidth, screenWidth - 24)
    height: Math.min(metrics.quickSettingsHeight,
                     screenHeight - panelHeight - 20)
    x: screenX + screenWidth - width - metrics.quickSettingsRight
    y: screenY + metrics.quickSettingsTop

    WindowDragController {
        id: quickSettingsDrag
        window: quickSettings
        screenX: quickSettings.screenX
        screenY: quickSettings.screenY
        screenWidth: quickSettings.screenWidth
        screenHeight: quickSettings.screenHeight
        topInset: quickSettings.panelHeight
        bottomInset: 12
        defaultX: quickSettings.screenX + quickSettings.screenWidth - quickSettings.width
            - metrics.quickSettingsRight
        defaultY: quickSettings.screenY + metrics.quickSettingsTop
    }

    function openPanel() {
        if (controller)
            controller.refresh()
        if (powerController) {
            powerController.refreshBattery()
            powerController.refreshPowerCapabilities()
        }
        quickSettingsDrag.prepareForOpen()
        show()
        raise()
        requestActivate()
    }

    function togglePanel() {
        if (visible)
            hide()
        else
            openPanel()
    }

    component PrimaryToggle: Rectangle {
        id: primaryToggle

        required property string iconName
        required property string label
        required property string status
        property bool active: false
        property bool actionable: true
        signal activated()

        color: primaryMouse.containsMouse && actionable ? lunar.raisedHover : "transparent"
        height: 106
        radius: lunar.radiusMedium
        width: (parent.width - (2 * parent.spacing)) / 3

        Behavior on color { ColorAnimation { duration: 130 } }

        Column {
            anchors.centerIn: parent
            spacing: 5

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                color: primaryToggle.active ? lunar.accent : lunar.raised
                border.color: primaryToggle.active ? lunar.accentBright : lunar.borderSoft
                border.width: 1
                height: 48
                radius: 24
                width: 48

                Behavior on color { ColorAnimation { duration: 150 } }

                NorthstarSystemIcon {
                    anchors.centerIn: parent
                    accented: false
                    darkMode: quickSettings.state ? quickSettings.state.darkMode : true
                    height: 30
                    iconName: primaryToggle.iconName
                    strokeColor: primaryToggle.active ? "#062039" : quickSettings.surfaceForeground
                    width: 30
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: quickSettings.surfaceForeground
                font.bold: true
                font.pixelSize: 13
                text: primaryToggle.label
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: quickSettings.surfaceMuted
                elide: Text.ElideRight
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                text: primaryToggle.status
                width: 104
            }
        }

        MouseArea {
            id: primaryMouse
            anchors.fill: parent
            cursorShape: primaryToggle.actionable ? Qt.PointingHandCursor : Qt.ArrowCursor
            enabled: primaryToggle.actionable
            hoverEnabled: true
            onClicked: primaryToggle.activated()
        }
    }

    component RoundAction: Rectangle {
        id: roundAction

        required property string iconName
        required property string description
        property bool actionable: true
        signal activated()

        color: actionMouse.containsMouse && actionable ? lunar.accentSoft : lunar.raised
        border.color: actionMouse.containsMouse && actionable ? lunar.accentBright : lunar.borderSoft
        border.width: 1
        height: 44
        opacity: actionable ? 1 : 0.45
        radius: 22
        width: 44

        Behavior on color { ColorAnimation { duration: 120 } }

        NorthstarSystemIcon {
            anchors.centerIn: parent
            accented: false
            darkMode: quickSettings.state ? quickSettings.state.darkMode : true
            height: 25
            iconName: roundAction.iconName
            width: 25
        }

        MouseArea {
            id: actionMouse
            anchors.fill: parent
            cursorShape: roundAction.actionable ? Qt.PointingHandCursor : Qt.ArrowCursor
            enabled: roundAction.actionable
            hoverEnabled: true
            onClicked: roundAction.activated()
        }

        ToolTip.delay: 400
        ToolTip.text: description
        ToolTip.visible: actionMouse.containsMouse
    }

    Dialog {
        id: sleepDialog
        modal: true
        title: "Sleep and lock Northstar?"
        standardButtons: Dialog.Cancel | Dialog.Ok
        padding: 16
        width: quickSettings.width - 20
        x: (quickSettings.width - width) / 2
        y: (quickSettings.height - height) / 2

        background: Rectangle {
            color: quickSettings.surfaceBackground
            border.color: quickSettings.surfaceAccent
            border.width: 1
            radius: lunar.radiusMedium
        }

        contentItem: Text {
            color: quickSettings.surfaceForeground
            text: "Northstar will suspend and require login after resume. Save important work first."
            wrapMode: Text.WordWrap
            width: sleepDialog.width - (2 * sleepDialog.padding)
        }

        onAccepted: {
            if (quickSettings.powerController
                    && quickSettings.powerController.requestSuspend()) {
                quickSettings.hide()
            } else {
                sleepDialog.open()
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: quickSettings.visible && !sleepDialog.visible
        onActivated: quickSettings.hide()
    }

    Rectangle {
        anchors.fill: parent
        border.color: lunar.border
        border.width: 1
        color: quickSettings.surfaceBackground
        radius: lunar.radiusPanel

        gradient: Gradient {
            GradientStop { position: 0.0; color: lunar.panelStrong }
            GradientStop { position: 1.0; color: lunar.panel }
        }

        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            spacing: 12

            Item {
                id: quickSettingsDragHandle
                height: 12
                width: parent.width

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    color: lunar.subtle
                    height: 3
                    opacity: dragHandleHover.hovered ? 0.85 : 0.45
                    radius: 2
                    width: dragHandleHover.hovered ? 46 : 34

                    Behavior on opacity { NumberAnimation { duration: 120 } }
                    Behavior on width { NumberAnimation { duration: 140 } }
                }

                HoverHandler {
                    id: dragHandleHover
                    cursorShape: Qt.OpenHandCursor
                }

                NativeWindowMoveHandler {
                    window: quickSettings
                    onMoveStarted: quickSettingsDrag.hasCustomPosition = true
                }
            }

            Row {
                spacing: 8
                width: parent.width

                PrimaryToggle {
                    active: !!quickSettings.controller && quickSettings.controller.wifiEnabled
                    actionable: !!quickSettings.controller && quickSettings.controller.wifiWritable
                    iconName: "wifi"
                    label: "Wi-Fi"
                    status: quickSettings.controller ? quickSettings.controller.wifiStatus : "Unavailable"
                    onActivated: quickSettings.controller.setWifiEnabled(!quickSettings.controller.wifiEnabled)
                }

                PrimaryToggle {
                    active: !!quickSettings.controller && quickSettings.controller.bluetoothEnabled
                    actionable: !!quickSettings.controller && quickSettings.controller.bluetoothWritable
                    iconName: "bluetooth"
                    label: "Bluetooth"
                    status: quickSettings.controller ? quickSettings.controller.bluetoothStatus : "Unavailable"
                    onActivated: quickSettings.controller.setBluetoothEnabled(!quickSettings.controller.bluetoothEnabled)
                }

                PrimaryToggle {
                    active: !!quickSettings.state && quickSettings.state.darkMode
                    iconName: "appearance"
                    label: "Dark Mode"
                    status: active ? "On" : "Off"
                    onActivated: if (quickSettings.state) quickSettings.state.darkMode = !quickSettings.state.darkMode
                }
            }

            Rectangle {
                border.color: lunar.borderSoft
                border.width: 1
                color: lunar.raised
                height: 104
                radius: lunar.radiusMedium
                width: parent.width

                Column {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    anchors.topMargin: 10
                    spacing: 5

                    Row {
                        height: 22
                        spacing: 8
                        width: parent.width

                        NorthstarSystemIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            darkMode: quickSettings.state ? quickSettings.state.darkMode : true
                            height: 21
                            iconName: "brightness"
                            width: 21
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 13
                            text: "Display"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: quickSettings.surfaceMuted
                            elide: Text.ElideRight
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                            text: quickSettings.controller ? quickSettings.controller.displayStatus : "Unavailable"
                            width: parent.width - 104
                        }
                    }

                    AuroraSlider {
                        id: displaySlider
                        darkMode: quickSettings.state ? quickSettings.state.darkMode : true
                        enabled: !!quickSettings.controller && quickSettings.controller.displayWritable
                        from: 1
                        live: false
                        to: 100
                        value: quickSettings.controller ? quickSettings.controller.displayBrightness : 0
                        width: parent.width
                        onPressedChanged: {
                            if (!pressed && quickSettings.controller)
                                quickSettings.controller.setDisplayBrightness(Math.round(value))
                        }
                    }
                }
            }

            Rectangle {
                border.color: lunar.borderSoft
                border.width: 1
                color: lunar.raised
                height: 104
                radius: lunar.radiusMedium
                width: parent.width

                Column {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    anchors.topMargin: 10
                    spacing: 5

                    Row {
                        height: 22
                        spacing: 8
                        width: parent.width

                        NorthstarSystemIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            darkMode: quickSettings.state ? quickSettings.state.darkMode : true
                            height: 21
                            iconName: "sound"
                            width: 21
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 13
                            text: "Sound"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: quickSettings.surfaceMuted
                            elide: Text.ElideRight
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                            text: quickSettings.controller ? quickSettings.controller.soundStatus : "Unavailable"
                            width: parent.width - 96
                        }
                    }

                    Row {
                        spacing: 8
                        width: parent.width

                        RoundAction {
                            actionable: !!quickSettings.controller && quickSettings.controller.soundAvailable
                            description: quickSettings.controller && quickSettings.controller.muted ? "Unmute" : "Mute"
                            height: 34
                            iconName: "sound"
                            width: 34
                            onActivated: quickSettings.controller.setMuted(!quickSettings.controller.muted)
                        }

                        AuroraSlider {
                            id: soundSlider
                            darkMode: quickSettings.state ? quickSettings.state.darkMode : true
                            enabled: !!quickSettings.controller && quickSettings.controller.soundAvailable
                            from: 0
                            live: false
                            to: 100
                            value: quickSettings.controller ? quickSettings.controller.volume : 0
                            width: parent.width - 42
                            onPressedChanged: {
                                if (!pressed && quickSettings.controller)
                                    quickSettings.controller.setVolume(Math.round(value))
                            }
                        }
                    }
                }
            }

            Row {
                height: 72
                spacing: 8
                width: parent.width

                Rectangle {
                    border.color: lunar.borderSoft
                    border.width: 1
                    color: lunar.raised
                    height: parent.height
                    radius: lunar.radiusMedium
                    width: parent.width - (3 * 48) - (3 * parent.spacing)

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 10
                        spacing: 10

                        NorthstarSystemIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            darkMode: quickSettings.state ? quickSettings.state.darkMode : true
                            height: 28
                            iconName: "battery"
                            width: 28
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            width: parent.width - 40

                            Text {
                                color: quickSettings.surfaceForeground
                                font.bold: true
                                font.pixelSize: 14
                                text: quickSettings.powerController && quickSettings.powerController.batteryAvailable
                                    ? quickSettings.powerController.batteryPercentage + "%" : "Power"
                            }

                            Text {
                                color: quickSettings.surfaceMuted
                                elide: Text.ElideRight
                                font.pixelSize: 10
                                text: quickSettings.powerController
                                    ? quickSettings.powerController.batteryStatus : "Unavailable"
                                width: parent.width
                            }
                        }
                    }
                }

                RoundAction {
                    actionable: !!quickSettings.powerController
                        && quickSettings.powerController.suspendAvailable
                    description: "Sleep and lock"
                    iconName: "lock"
                    height: 48
                    radius: 24
                    width: 48
                    onActivated: sleepDialog.open()
                }

                RoundAction {
                    actionable: !!quickSettings.systemMenu
                    description: "Power and session"
                    iconName: "power"
                    height: 48
                    radius: 24
                    width: 48
                    onActivated: {
                        quickSettings.hide()
                        quickSettings.systemMenu.openMenu()
                    }
                }

                RoundAction {
                    actionable: !!quickSettings.settingsWindow
                    description: "Settings"
                    iconName: "settings"
                    height: 48
                    radius: 24
                    width: 48
                    onActivated: {
                        quickSettings.hide()
                        quickSettings.settingsWindow.openSettings()
                    }
                }
            }

            Text {
                color: quickSettings.controller && quickSettings.controller.statusMessage.length > 0
                    ? lunar.warning : quickSettings.surfaceMuted
                elide: Text.ElideRight
                font.pixelSize: 10
                height: visible ? 18 : 0
                text: quickSettings.controller ? quickSettings.controller.statusMessage : ""
                verticalAlignment: Text.AlignVCenter
                visible: text.length > 0
                width: parent.width
            }
        }
    }
}
