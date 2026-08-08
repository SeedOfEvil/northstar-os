import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    width: 1920
    height: 1080
    focus: true

    property color backgroundColor: config.background
    property color panelColor: config.panel
    property color panelAltColor: config.panelAlt
    property color accentColor: config.accent
    property color accentStrongColor: config.accentStrong
    property color textColor: config.text
    property color mutedColor: config.muted
    property color errorColor: config.error
    property string logoSource: config.logo
    property string backgroundImageSource: config.backgroundImage
    property int selectedSessionIndex: sessionModel.lastIndex >= 0 ? sessionModel.lastIndex : 0
    property bool authenticating: false
    property string errorMessage: ""
    property string clockText: ""

    function updateClock() {
        root.clockText = Qt.formatDateTime(new Date(), "ddd, MMM d  -  HH:mm")
    }

    function attemptLogin() {
        if (root.authenticating)
            return

        var username = usernameField.text.trim()
        if (username.length === 0) {
            root.errorMessage = "Enter your username to continue."
            usernameField.forceActiveFocus()
            return
        }
        if (passwordField.text.length === 0) {
            root.errorMessage = "Enter your password to continue."
            passwordField.forceActiveFocus()
            return
        }

        root.errorMessage = ""
        root.authenticating = true
        sddm.login(username, passwordField.text, root.selectedSessionIndex)
    }

    Component.onCompleted: {
        root.updateClock()
        if (userModel.lastUser)
            usernameField.text = userModel.lastUser
        passwordField.forceActiveFocus()
    }

    Connections {
        target: sddm

        function onLoginSucceeded() {
            root.authenticating = false
            root.errorMessage = ""
        }

        function onLoginFailed() {
            root.authenticating = false
            root.errorMessage = "That login was not accepted. Try again."
            passwordField.selectAll()
            passwordField.forceActiveFocus()
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: root.updateClock()
    }

    Rectangle {
        anchors.fill: parent
        color: root.backgroundColor
    }

    Image {
        anchors.fill: parent
        source: root.backgroundImageSource
        fillMode: Image.PreserveAspectCrop
        opacity: 0.15
        smooth: true
        mipmap: true
    }

    Rectangle {
        anchors.fill: parent
        color: root.backgroundColor
        opacity: 0.78
    }

    Rectangle {
        id: brandPane
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.max(420, parent.width * 0.55)
        color: "#000000"

        Rectangle {
            anchors.fill: parent
            color: "#081522"
            opacity: 0.44
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * 0.015
            color: root.accentStrongColor
        }

        Image {
            id: logo
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: Math.max(54, parent.height * 0.12)
            width: Math.min(parent.width * 0.78, 560)
            height: width
            source: root.logoSource
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        Column {
            anchors.left: parent.left
            anchors.leftMargin: Math.max(48, parent.width * 0.11)
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.max(42, parent.height * 0.08)
            spacing: 8

            Text {
                text: "A calm place to begin."
                color: root.textColor
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            Text {
                text: "Northstar desktop"
                color: root.mutedColor
                font.pixelSize: 15
            }
        }
    }

    Column {
        id: loginPanel
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: Math.max(46, parent.width * 0.08)
        width: Math.min(460, parent.width * 0.36)
        spacing: 18

        Text {
            text: "Welcome back"
            color: root.textColor
            font.pixelSize: 34
            font.weight: Font.DemiBold
        }

        Text {
            text: "Sign in to continue to Northstar"
            color: root.mutedColor
            font.pixelSize: 16
        }

        Item { width: 1; height: 4 }

        Text {
            text: "Username"
            color: root.mutedColor
            font.pixelSize: 13
        }

        Rectangle {
            width: parent.width
            height: 54
            radius: 10
            color: root.panelAltColor
            border.width: 1
            border.color: usernameField.activeFocus ? root.accentColor : "#2b4258"

            TextInput {
                id: usernameField
                anchors.fill: parent
                anchors.margins: 16
                color: root.textColor
                font.pixelSize: 17
                clip: true
                selectByMouse: true
                verticalAlignment: TextInput.AlignVCenter
                onAccepted: passwordField.forceActiveFocus()
            }
        }

        Text {
            text: "Password"
            color: root.mutedColor
            font.pixelSize: 13
        }

        Rectangle {
            width: parent.width
            height: 54
            radius: 10
            color: root.panelAltColor
            border.width: 1
            border.color: passwordField.activeFocus ? root.accentColor : "#2b4258"

            TextInput {
                id: passwordField
                anchors.fill: parent
                anchors.margins: 16
                color: root.textColor
                font.pixelSize: 17
                echoMode: TextInput.Password
                clip: true
                selectByMouse: true
                verticalAlignment: TextInput.AlignVCenter
                onAccepted: root.attemptLogin()
            }
        }

        Text {
            text: "Session"
            color: root.mutedColor
            font.pixelSize: 13
        }

        ComboBox {
            id: sessionPicker
            width: parent.width
            height: 50
            model: sessionModel
            textRole: "name"
            currentIndex: root.selectedSessionIndex
            onActivated: root.selectedSessionIndex = index

            contentItem: Text {
                leftPadding: 16
                rightPadding: 40
                text: sessionPicker.displayText
                color: root.textColor
                font.pixelSize: 16
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 10
                color: root.panelAltColor
                border.width: 1
                border.color: "#2b4258"
            }
        }

        Rectangle {
            width: parent.width
            height: 56
            radius: 10
            color: root.authenticating ? "#315779" : root.accentStrongColor
            opacity: root.authenticating ? 0.8 : 1

            Text {
                anchors.centerIn: parent
                text: root.authenticating ? "Signing in..." : "Sign in"
                color: "#ffffff"
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }

            MouseArea {
                anchors.fill: parent
                enabled: !root.authenticating
                onClicked: root.attemptLogin()
            }
        }

        Text {
            width: parent.width
            height: 22
            text: root.errorMessage
            color: root.errorColor
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            visible: text.length > 0
        }

        Row {
            width: parent.width
            spacing: 12
            visible: sddm.canPowerOff || sddm.canReboot

            Rectangle {
                width: (parent.width - parent.spacing) / 2
                height: 42
                radius: 8
                color: "transparent"
                border.width: 1
                border.color: "#2b4258"
                visible: sddm.canReboot

                Text {
                    anchors.centerIn: parent
                    text: "Restart"
                    color: root.mutedColor
                    font.pixelSize: 14
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: sddm.reboot()
                }
            }

            Rectangle {
                width: (parent.width - parent.spacing) / 2
                height: 42
                radius: 8
                color: "transparent"
                border.width: 1
                border.color: "#2b4258"
                visible: sddm.canPowerOff

                Text {
                    anchors.centerIn: parent
                    text: "Shut down"
                    color: root.mutedColor
                    font.pixelSize: 14
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: sddm.powerOff()
                }
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: Math.max(46, parent.width * 0.08)
        anchors.bottomMargin: 30
        spacing: 20

        Text {
            text: root.clockText
            color: root.mutedColor
            font.pixelSize: 14
        }

        Text {
            text: sddm.hostname || "Northstar"
            color: root.mutedColor
            font.pixelSize: 14
        }
    }
}
