import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Northstar.Ui 1.0

ApplicationWindow {
    id: root
    width: 760
    height: 620
    minimumWidth: 640
    minimumHeight: 520
    visible: true
    title: "Wi-Fi"
    color: lunar.background

    property string selectedHex: ""
    property string selectedSecurity: ""
    property string selectedName: ""

    LunarPalette { id: lunar; darkMode: northstarDarkMode }

    Connections {
        target: wifiController
        function onSecretsCleared() { passwordField.text = "" }
        function onAuthorizationPromptExpected() { root.hide() }
        function onAuthorizationCompleted() {
            root.show()
            root.raise()
            root.requestActivate()
        }
        function onConnectionFinished(success) {
            if (success) wifiController.refreshNetworks()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: "Wireless networks"
                color: lunar.foreground
                font.bold: true
                font.pixelSize: 28
            }
            Button {
                text: wifiController.busy ? "Working..." : "Refresh"
                enabled: !wifiController.busy
                onClicked: wifiController.refreshNetworks()
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Choose a nearby network. Northstar sends the password directly to the protected service and stores only a derived key."
            color: lunar.muted
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 14
            color: lunar.panel
            border.color: lunar.borderSoft

            ListView {
                id: networkList
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6
                clip: true
                model: wifiController.networks

                delegate: Rectangle {
                    required property var modelData
                    width: networkList.width
                    height: 62
                    radius: 10
                    color: root.selectedHex === modelData.ssidHex ? lunar.accentSoft : mouse.containsMouse ? lunar.raised : "transparent"
                    border.color: root.selectedHex === modelData.ssidHex ? lunar.accent : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        Label {
                            Layout.fillWidth: true
                            text: modelData.ssid
                            color: lunar.foreground
                            font.bold: root.selectedHex === modelData.ssidHex
                            elide: Text.ElideRight
                        }
                        Label {
                            visible: modelData.connected
                            text: "Connected"
                            color: lunar.accent
                            font.bold: true
                        }
                        Label {
                            text: modelData.secured ? "Secured" : "Open"
                            color: lunar.muted
                        }
                        Label {
                            text: modelData.signal >= -55 ? "Signal: strong"
                                : modelData.signal >= -72 ? "Signal: good" : "Signal: weak"
                            color: lunar.muted
                        }
                    }
                    MouseArea {
                        id: mouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: !wifiController.busy
                        onClicked: {
                            root.selectedHex = modelData.ssidHex
                            root.selectedSecurity = modelData.security
                            root.selectedName = modelData.ssid
                            passwordField.text = ""
                            passwordField.forceActiveFocus()
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: networkList.count === 0 && !wifiController.busy
                    text: "No networks listed. Choose Refresh to scan."
                    color: lunar.muted
                }
            }
        }

        Label {
            visible: root.selectedHex.length > 0
            text: "Connect to " + root.selectedName
            color: lunar.foreground
            font.bold: true
        }
        TextField {
            id: passwordField
            Layout.fillWidth: true
            visible: root.selectedSecurity === "secured"
            enabled: !wifiController.busy
            echoMode: TextInput.Password
            placeholderText: "Wi-Fi password"
            maximumLength: 63
            onAccepted: connectButton.clicked()
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: wifiController.statusMessage
                color: wifiController.statusIsError ? lunar.warning : lunar.muted
                wrapMode: Text.WordWrap
            }
            Button {
                id: connectButton
                text: "Connect"
                enabled: !wifiController.busy && root.selectedHex.length > 0
                    && (root.selectedSecurity === "open" || passwordField.text.length >= 8)
                onClicked: wifiController.connectNetwork(
                    root.selectedHex, root.selectedSecurity, passwordField.text)
            }
        }
    }

    Component.onCompleted: if (!wifiSelfTest) wifiController.refreshNetworks()
}
