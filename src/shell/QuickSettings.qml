import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: quickSettings

    LunarPalette {
        id: lunar
        darkMode: quickSettings.state ? quickSettings.state.darkMode : true
    }

    property var state
    property var controller
    property var powerController
    property var settingsWindow
    property var targetScreen
    property int panelHeight: 44
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

    width: 380
    height: Math.min(contentColumn.implicitHeight + 32,
                     screenHeight - panelHeight - 20)
    x: screenX + screenWidth - width - 18
    y: screenY + panelHeight + 8

    WindowDragController {
        id: quickSettingsDrag
        window: quickSettings
        screenX: quickSettings.screenX
        screenY: quickSettings.screenY
        screenWidth: quickSettings.screenWidth
        screenHeight: quickSettings.screenHeight
        topInset: quickSettings.panelHeight
        bottomInset: 12
        defaultX: quickSettings.screenX + quickSettings.screenWidth - quickSettings.width - 18
        defaultY: quickSettings.screenY + quickSettings.panelHeight + 8
    }

    function openPanel() {
        if (controller)
            controller.refresh()
        if (powerController)
            powerController.refreshBattery()
        if (powerController)
            powerController.refreshPowerCapabilities()
        quickSettingsDrag.prepareForOpen()
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

    Dialog {
        id: sleepDialog
        modal: true
        title: "Put Northstar to sleep?"
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

        contentItem: Column {
            spacing: 8
            width: sleepDialog.width - (2 * sleepDialog.padding)

            Text {
                color: quickSettings.surfaceForeground
                text: "This puts Northstar to sleep. Save important work before testing resume."
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Text {
                color: "#c34f65"
                text: quickSettings.powerController
                    ? quickSettings.powerController.statusMessage : ""
                visible: text.length > 0 && text !== "Sleep requested"
                wrapMode: Text.WordWrap
                width: parent.width
            }
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

    Rectangle {
        anchors.fill: parent
        color: quickSettings.surfaceBackground
        border.color: lunar.border
        border.width: 1
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

            Row {
                width: parent.width
                height: 0
                visible: false

                Item {
                    id: quickSettingsDragHandle
                    height: 34
                    width: parent.width - closeButton.width - 8

                    Column {
                        anchors.fill: parent
                        spacing: 2

                        Text {
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 18
                            text: "Quick settings"
                        }

                        Text {
                            color: quickSettings.surfaceMuted
                            font.pixelSize: 11
                            text: quickSettings.powerController
                                && quickSettings.powerController.batteryAvailable
                                ? quickSettings.powerController.batteryStatus
                                : "Northstar controls"
                        }
                    }

                    NativeWindowMoveHandler {
                        window: quickSettings
                    }
                }

                Rectangle {
                    id: closeButton
                    color: closeMouse.containsMouse ? quickSettings.surfaceAccent : quickSettings.surfaceRaised
                    height: 30
                    radius: lunar.radiusSmall
                    width: 62

                    Text {
                        anchors.centerIn: parent
                        color: quickSettings.surfaceForeground
                        font.pixelSize: 11
                        text: "Close"
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: quickSettings.hide()
                    }
                }
            }

            Row {
                spacing: 8
                width: parent.width

                Rectangle {
                    id: wifiTile

                    // Only a radio this build can actually change offers to be
                    // pressed. Everything else stays a reading.
                    readonly property bool writable: !!quickSettings.controller
                        && quickSettings.controller.wifiWritable

                    color: quickSettings.controller && quickSettings.controller.wifiEnabled
                        ? (wifiMouse.containsMouse && wifiTile.writable
                            ? lunar.accentBright : lunar.accentSoft)
                        : (wifiMouse.containsMouse && wifiTile.writable
                            ? lunar.raisedHover : quickSettings.surfaceRaised)
                    border.color: quickSettings.controller && quickSettings.controller.wifiEnabled
                        ? lunar.accentBright : lunar.borderSoft
                    border.width: 1
                    height: 92
                    radius: lunar.radiusMedium
                    width: (parent.width - (2 * parent.spacing)) / 3

                    MouseArea {
                        id: wifiMouse
                        anchors.fill: parent
                        enabled: wifiTile.writable
                        hoverEnabled: true
                        cursorShape: wifiTile.writable ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: quickSettings.controller.setWifiEnabled(
                            !quickSettings.controller.wifiEnabled)
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.controller && quickSettings.controller.wifiEnabled
                                ? lunar.accent : lunar.raisedHover
                            height: 32
                            radius: 16
                            width: 32

                            Text {
                                anchors.centerIn: parent
                                color: quickSettings.surfaceForeground
                                font.pixelSize: 17
                                text: "⌁"
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 12
                            text: "Wi-Fi"
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.surfaceForeground
                            font.pixelSize: 10
                            text: quickSettings.controller
                                ? quickSettings.controller.wifiStatus : "Status unavailable"
                            width: 100
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                Rectangle {
                    id: bluetoothTile

                    readonly property bool writable: !!quickSettings.controller
                        && quickSettings.controller.bluetoothWritable

                    color: quickSettings.controller && quickSettings.controller.bluetoothEnabled
                        ? (bluetoothMouse.containsMouse && bluetoothTile.writable
                            ? lunar.accentBright : lunar.accentSoft)
                        : (bluetoothMouse.containsMouse && bluetoothTile.writable
                            ? lunar.raisedHover : quickSettings.surfaceRaised)
                    border.color: quickSettings.controller && quickSettings.controller.bluetoothEnabled
                        ? lunar.accentBright : lunar.borderSoft
                    border.width: 1
                    height: 92
                    radius: lunar.radiusMedium
                    width: (parent.width - (2 * parent.spacing)) / 3

                    MouseArea {
                        id: bluetoothMouse
                        anchors.fill: parent
                        enabled: bluetoothTile.writable
                        hoverEnabled: true
                        cursorShape: bluetoothTile.writable ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: quickSettings.controller.setBluetoothEnabled(
                            !quickSettings.controller.bluetoothEnabled)
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.controller && quickSettings.controller.bluetoothEnabled
                                ? lunar.accent : lunar.raisedHover
                            height: 32
                            radius: 16
                            width: 32

                            Text {
                                anchors.centerIn: parent
                                color: quickSettings.surfaceForeground
                                font.pixelSize: 17
                                text: "ᛒ"
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 12
                            text: "Bluetooth"
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.surfaceForeground
                            font.pixelSize: 10
                            text: quickSettings.controller
                                ? quickSettings.controller.bluetoothStatus : "Status unavailable"
                            width: 100
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                Rectangle {
                    id: appearanceTile
                    color: quickSettings.state && quickSettings.state.darkMode
                        ? lunar.accentSoft
                        : appearanceMouse.containsMouse ? lunar.raisedHover : quickSettings.surfaceRaised
                    border.color: quickSettings.state && quickSettings.state.darkMode
                        ? lunar.accentBright : lunar.borderSoft
                    border.width: 1
                    height: 92
                    radius: lunar.radiusMedium
                    width: (parent.width - (2 * parent.spacing)) / 3

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.state && quickSettings.state.darkMode
                                ? lunar.accent : lunar.raisedHover
                            height: 32
                            radius: 16
                            width: 32

                            Text {
                                anchors.centerIn: parent
                                color: quickSettings.surfaceForeground
                                font.pixelSize: 17
                                text: "◐"
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 12
                            text: "Dark mode"
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: quickSettings.surfaceMuted
                            font.pixelSize: 10
                            text: quickSettings.state && quickSettings.state.darkMode ? "On" : "Off"
                        }
                    }

                    MouseArea {
                        id: appearanceMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (quickSettings.state)
                                quickSettings.state.darkMode = !quickSettings.state.darkMode
                        }
                    }
                }
            }

            Row {
                spacing: 8
                width: parent.width

                Rectangle {
                    color: quickSettings.controller && quickSettings.controller.nightLightEnabled
                        ? lunar.accentSoft : quickSettings.surfaceRaised
                    height: 56
                    radius: lunar.radiusMedium
                    width: (parent.width - parent.spacing) / 2

                    Text {
                        anchors.centerIn: parent
                        color: quickSettings.surfaceForeground
                        font.pixelSize: 11
                        text: "Night Light  " + (quickSettings.controller
                            ? quickSettings.controller.nightLightStatus : "Unavailable")
                        width: parent.width - 16
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Rectangle {
                    color: quickSettings.controller && quickSettings.controller.doNotDisturb
                        ? lunar.accentSoft : quickSettings.surfaceRaised
                    height: 56
                    radius: lunar.radiusMedium
                    width: (parent.width - parent.spacing) / 2

                    Text {
                        anchors.centerIn: parent
                        color: quickSettings.surfaceForeground
                        font.pixelSize: 11
                        text: "Do Not Disturb  "
                            + (quickSettings.controller && quickSettings.controller.doNotDisturb ? "On" : "Off")
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (quickSettings.controller)
                                quickSettings.controller.toggleDoNotDisturb()
                        }
                    }
                }
            }

            Rectangle {
                color: quickSettings.surfaceRaised
                height: mixerControls.implicitHeight + 20
                radius: lunar.radiusMedium
                border.color: lunar.borderSoft
                border.width: 1
                width: parent.width

                Column {
                    id: mixerControls
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    anchors.topMargin: 10
                    spacing: 3

                    Item {
                        height: 18
                        width: parent.width

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: quickSettings.surfaceForeground
                            font.pixelSize: 11
                            text: "Display"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            color: quickSettings.surfaceMuted
                            font.pixelSize: 10
                            text: quickSettings.controller
                                ? quickSettings.controller.displayStatus : "Unavailable"
                            width: 210
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    AuroraSlider {
                        id: displaySlider
                        enabled: quickSettings.controller && quickSettings.controller.displayWritable
                        from: 1
                        to: 100
                        value: quickSettings.controller ? quickSettings.controller.displayBrightness : 0
                        live: false
                        width: parent.width
                        onPressedChanged: {
                            if (!pressed && quickSettings.controller)
                                quickSettings.controller.setDisplayBrightness(Math.round(value))
                        }
                    }

                    Item {
                        height: 18
                        width: parent.width

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: quickSettings.surfaceForeground
                            font.pixelSize: 11
                            text: "Sound"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            color: quickSettings.surfaceMuted
                            font.pixelSize: 10
                            text: quickSettings.controller
                                ? quickSettings.controller.soundStatus : "Unavailable"
                            width: 210
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    Row {
                        spacing: 8
                        width: parent.width

                        AuroraButton {
                            id: muteButton
                            enabled: quickSettings.controller && quickSettings.controller.soundAvailable
                            text: quickSettings.controller && quickSettings.controller.muted
                                ? "Unmute" : "Mute"
                            width: 76
                            onClicked: {
                                if (quickSettings.controller)
                                    quickSettings.controller.setMuted(!quickSettings.controller.muted)
                            }
                        }

                        AuroraSlider {
                            id: soundSlider
                            enabled: quickSettings.controller && quickSettings.controller.soundAvailable
                            from: 0
                            to: 100
                            value: quickSettings.controller ? quickSettings.controller.volume : 0
                            live: false
                            width: parent.width - muteButton.width - parent.spacing
                            onPressedChanged: {
                                if (!pressed && quickSettings.controller)
                                    quickSettings.controller.setVolume(Math.round(value))
                            }
                        }
                    }

                    AuroraComboBox {
                        id: outputChooser
                        enabled: quickSettings.controller
                            && quickSettings.controller.soundOutputs.length > 1
                        model: quickSettings.controller
                            ? quickSettings.controller.soundOutputs : []
                        textRole: "label"
                        valueRole: "unit"
                        width: parent.width

                        function syncToOutput() {
                            currentIndex = indexOfValue(quickSettings.controller
                                ? quickSettings.controller.soundOutput : -1)
                        }

                        Component.onCompleted: syncToOutput()
                        onModelChanged: syncToOutput()
                        onActivated: {
                            if (!quickSettings.controller
                                || !quickSettings.controller.setSoundOutput(valueAt(currentIndex))) {
                                syncToOutput()
                            }
                        }
                    }
                }
            }

            Rectangle {
                color: quickSettings.surfaceRaised
                height: 66
                radius: lunar.radiusMedium
                border.color: lunar.borderSoft
                border.width: 1
                width: parent.width

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 12

                    Rectangle {
                        color: quickSettings.surfaceAccent
                        height: 34
                        radius: 8
                        width: 34
                        y: (parent.height - height) / 2

                        Text {
                            anchors.centerIn: parent
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 14
                            text: "♪"
                        }
                    }

                    Column {
                        spacing: 2
                        y: (parent.height - height) / 2

                        Text {
                            color: quickSettings.surfaceForeground
                            font.bold: true
                            font.pixelSize: 11
                            text: "Media"
                        }

                        Text {
                            color: quickSettings.surfaceMuted
                            font.pixelSize: 10
                            text: "No media playing"
                        }
                    }
                }
            }

            Row {
                spacing: 8
                width: parent.width

                Text {
                    color: quickSettings.surfaceMuted
                    elide: Text.ElideRight
                    font.pixelSize: 10
                    height: 28
                    text: quickSettings.controller && quickSettings.controller.statusMessage.length > 0
                        ? quickSettings.controller.statusMessage
                        : "Controls reflect confirmed hardware capabilities."
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width - sleepButton.width - settingsButton.width
                        - (2 * parent.spacing)
                }

                Rectangle {
                    id: sleepButton
                    color: sleepMouse.containsMouse && sleepMouse.enabled
                        ? lunar.accentSoft : quickSettings.surfaceRaised
                    border.color: lunar.borderSoft
                    border.width: 1
                    height: 34
                    radius: 17
                    width: 34

                    Text {
                        anchors.centerIn: parent
                        color: sleepMouse.enabled
                            ? quickSettings.surfaceForeground : quickSettings.surfaceMuted
                        font.pixelSize: 15
                        text: "◒"
                    }

                    MouseArea {
                        id: sleepMouse
                        anchors.fill: parent
                        enabled: !!quickSettings.powerController
                            && quickSettings.powerController.suspendAvailable
                        hoverEnabled: true
                        onClicked: sleepDialog.open()
                    }
                }

                Rectangle {
                    id: settingsButton
                    color: settingsMouse.containsMouse ? lunar.accentSoft : quickSettings.surfaceRaised
                    border.color: lunar.borderSoft
                    border.width: 1
                    height: 34
                    radius: 17
                    width: 34

                    Text {
                        anchors.centerIn: parent
                        color: quickSettings.surfaceForeground
                        font.pixelSize: 15
                        text: "⚙"
                    }

                    MouseArea {
                        id: settingsMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            quickSettings.hide()
                            if (quickSettings.settingsWindow)
                                quickSettings.settingsWindow.openSettings()
                        }
                    }
                }
            }
        }
    }
}
