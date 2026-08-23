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

    width: 354
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
                            text: "Northstar controls"
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
                    height: 62
                    radius: lunar.radiusMedium
                    width: (parent.width - parent.spacing) / 2

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
                        spacing: 3

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
                            width: 138
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
                    height: 62
                    radius: lunar.radiusMedium
                    width: (parent.width - parent.spacing) / 2

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
                        spacing: 3

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
                            width: 138
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
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

                    Slider {
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

                        Button {
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

                        Slider {
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

                    ComboBox {
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
                        : "Controls reflect confirmed FreeBSD capabilities."
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width - settingsButton.width - parent.spacing
                }

                Rectangle {
                    id: settingsButton
                    color: settingsMouse.containsMouse ? lunar.accentSoft : quickSettings.surfaceRaised
                    border.color: lunar.borderSoft
                    border.width: 1
                    height: 28
                    radius: lunar.radiusSmall
                    width: 64

                    Text {
                        anchors.centerIn: parent
                        color: quickSettings.surfaceForeground
                        font.pixelSize: 10
                        text: "Settings"
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
