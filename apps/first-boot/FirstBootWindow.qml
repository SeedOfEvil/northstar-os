import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Northstar.Ui 1.0

ApplicationWindow {
    id: root

    LunarPalette { id: lunar; darkMode: northstarDarkMode }

    property int pageIndex: 0
    property string validationMessage: ""

    color: lunar.background
    height: Screen.height
    minimumHeight: 680
    minimumWidth: 920
    title: "Set up Northstar"
    visible: true
    width: Screen.width
    x: 0
    y: 0

    Connections {
        target: firstBootController
        function onSecretsCleared() {
            passwordField.text = ""
            confirmationField.text = ""
        }
        function onProvisioningFinished(success) {
            if (success) root.pageIndex = 3
        }
    }

    background: Rectangle {
        color: lunar.background
        gradient: Gradient {
            GradientStop { position: 0; color: lunar.background }
            GradientStop { position: 1; color: lunar.panel }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 36
        spacing: 32

        ColumnLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: 300
            spacing: 14

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 180
                Layout.preferredWidth: 230
                fillMode: Image.PreserveAspectFit
                source: "qrc:/Northstar/FirstBoot/northstar-logo.png"
            }

            Label {
                Layout.fillWidth: true
                color: lunar.foreground
                font.bold: true
                font.pixelSize: 28
                horizontalAlignment: Text.AlignHCenter
                text: "Welcome to Northstar"
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                color: lunar.muted
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                text: "A calm place to begin. Create the first administrator and choose this computer's regional settings."
                wrapMode: Text.WordWrap
            }

            Item { Layout.fillHeight: true }

            Repeater {
                model: ["Welcome", "Account", "Region", "Ready"]
                delegate: RowLayout {
                    required property int index
                    required property string modelData
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        color: index <= root.pageIndex ? lunar.accent : lunar.raised
                        height: 12
                        radius: 6
                        width: 12
                    }
                    Label {
                        color: index === root.pageIndex ? lunar.foreground : lunar.muted
                        font.bold: index === root.pageIndex
                        text: modelData
                    }
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.fillWidth: true
            border.color: lunar.borderSoft
            border.width: 1
            color: lunar.panel
            radius: 24

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 30
                spacing: 18

                StackLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    currentIndex: root.pageIndex

                    ColumnLayout {
                        spacing: 16
                        Label { color: lunar.foreground; font.bold: true; font.pixelSize: 30; text: "Make Northstar yours" }
                        Label {
                            Layout.fillWidth: true
                            color: lunar.muted
                            font.pixelSize: 15
                            text: "This one-time setup creates the first administrator. Your password is sent directly to the protected provisioning service and is never written to the setup request or logs."
                            wrapMode: Text.WordWrap
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 150
                            border.color: lunar.borderSoft
                            color: lunar.raised
                            radius: 16
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                Label { color: lunar.foreground; font.bold: true; text: "What happens next" }
                                Label { Layout.fillWidth: true; color: lunar.muted; text: "1. Create your administrator account\n2. Choose language, timezone, and keyboard\n3. Restart into the branded Northstar login"; wrapMode: Text.WordWrap }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {
                        spacing: 12
                        Label { color: lunar.foreground; font.bold: true; font.pixelSize: 28; text: "Create your account" }
                        Label { color: lunar.muted; text: "This account can authorize system changes using your password." }
                        Label { color: lunar.foreground; text: "Display name" }
                        TextField { id: displayNameField; Layout.fillWidth: true; placeholderText: "Northstar User"; maximumLength: 80 }
                        Label { color: lunar.foreground; text: "Username" }
                        TextField { id: usernameField; Layout.fillWidth: true; placeholderText: "hector"; maximumLength: 31 }
                        Label { color: lunar.foreground; text: "Password" }
                        TextField { id: passwordField; Layout.fillWidth: true; echoMode: TextInput.Password; maximumLength: 128 }
                        Label { color: lunar.foreground; text: "Confirm password" }
                        TextField { id: confirmationField; Layout.fillWidth: true; echoMode: TextInput.Password; maximumLength: 128 }
                        Label { Layout.fillWidth: true; color: lunar.warning; text: root.validationMessage; visible: text.length > 0; wrapMode: Text.WordWrap }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {
                        spacing: 14
                        Label { color: lunar.foreground; font.bold: true; font.pixelSize: 28; text: "Region and input" }
                        Label { color: lunar.foreground; text: "Language and locale" }
                        ComboBox { id: localeBox; Layout.fillWidth: true; model: firstBootController.locales }
                        Label { color: lunar.foreground; text: "Timezone" }
                        ComboBox { id: timezoneBox; Layout.fillWidth: true; model: firstBootController.timezones; currentIndex: 1 }
                        Label { color: lunar.foreground; text: "Keyboard layout" }
                        ComboBox { id: keyboardBox; Layout.fillWidth: true; model: firstBootController.keyboardLayouts }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 120
                            border.color: lunar.borderSoft
                            color: lunar.raised
                            radius: 14
                            Label {
                                anchors.fill: parent
                                anchors.margins: 18
                                color: lunar.muted
                                text: "Northstar will create \"" + displayNameField.text + "\" (" + usernameField.text + ") as the first administrator. Setup cannot be run again after successful provisioning."
                                verticalAlignment: Text.AlignVCenter
                                wrapMode: Text.WordWrap
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {
                        spacing: 18
                        Label { color: lunar.foreground; font.bold: true; font.pixelSize: 30; text: "Your Northstar is ready" }
                        Label { Layout.fillWidth: true; color: lunar.muted; font.pixelSize: 15; text: firstBootController.statusMessage; wrapMode: Text.WordWrap }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 130
                            border.color: lunar.accent
                            color: lunar.raised
                            radius: 16
                            Label {
                                anchors.fill: parent
                                anchors.margins: 20
                                color: lunar.foreground
                                text: "Restart this computer, sign in using \"" + usernameField.text + "\", and continue into the normal Northstar desktop."
                                verticalAlignment: Text.AlignVCenter
                                wrapMode: Text.WordWrap
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    color: firstBootController.busy ? lunar.accent : lunar.muted
                    text: firstBootController.statusMessage
                    visible: root.pageIndex === 2 || firstBootController.busy
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        enabled: root.pageIndex > 0 && root.pageIndex < 3 && !firstBootController.busy
                        text: "Back"
                        visible: root.pageIndex > 0 && root.pageIndex < 3
                        onClicked: {
                            root.validationMessage = ""
                            root.pageIndex--
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        enabled: !firstBootController.busy
                        text: root.pageIndex === 0 ? "Get Started" : root.pageIndex === 1 ? "Continue" : root.pageIndex === 2 ? "Create Account" : "Close"
                        onClicked: {
                            if (root.pageIndex === 0) {
                                root.pageIndex = 1
                            } else if (root.pageIndex === 1) {
                                root.validationMessage = firstBootController.validateProfile(
                                    displayNameField.text, usernameField.text,
                                    passwordField.text, confirmationField.text,
                                    localeBox.currentText, timezoneBox.currentText,
                                    keyboardBox.currentText)
                                if (root.validationMessage.length === 0) root.pageIndex = 2
                            } else if (root.pageIndex === 2) {
                                firstBootController.provision(
                                    displayNameField.text, usernameField.text,
                                    passwordField.text, confirmationField.text,
                                    localeBox.currentText, timezoneBox.currentText,
                                    keyboardBox.currentText)
                            } else {
                                root.close()
                            }
                        }
                    }
                }
            }
        }
    }
}
