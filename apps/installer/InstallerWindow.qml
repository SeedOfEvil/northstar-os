import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Northstar.Ui 1.0

ApplicationWindow {
    id: root
    LunarPalette { id: lunar; darkMode: northstarDarkMode }

    color: lunar.background
    height: 720
    minimumHeight: 680
    minimumWidth: 1000
    title: "Install Northstar"
    visible: true
    width: 1100

    Component.onCompleted: if (!installerSelfTest) installerController.refresh()

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0; color: lunar.background }
            GradientStop { position: 1; color: lunar.desktopBottom }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 30
        spacing: 26

        ColumnLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: 245
            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 145
                Layout.preferredWidth: 190
                fillMode: Image.PreserveAspectFit
                source: "qrc:/Northstar/Installer/northstar-logo.png"
            }
            Label { Layout.fillWidth: true; color: lunar.foreground; font.bold: true; font.pixelSize: 25; horizontalAlignment: Text.AlignHCenter; text: "Install Northstar" }
            Label { Layout.fillWidth: true; color: lunar.muted; horizontalAlignment: Text.AlignHCenter; text: "Choose where Northstar will live."; wrapMode: Text.WordWrap }
            Item { Layout.preferredHeight: 14 }
            Repeater {
                model: ["Destination", "Confirm erase", "Review plan", "Install"]
                delegate: RowLayout {
                    required property int index
                    required property string modelData
                    Layout.fillWidth: true
                    Rectangle { color: index <= (installerController.planReady ? 2 : installerController.selectedIndex >= 0 ? 1 : 0) ? lunar.accent : lunar.raised; height: 12; radius: 6; width: 12 }
                    Label { color: lunar.muted; text: modelData }
                }
            }
            Item { Layout.fillHeight: true }
            Label { Layout.fillWidth: true; color: lunar.muted; font.pixelSize: 11; text: "This slice prepares a reviewed plan only. It cannot partition or erase a disk."; wrapMode: Text.WordWrap }
        }

        NorthstarWindowFrame {
            Layout.fillHeight: true
            Layout.fillWidth: true
            darkMode: northstarDarkMode

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label { color: lunar.foreground; font.bold: true; font.pixelSize: 27; text: installerController.planReady ? "Review installation plan" : "Select a destination" }
                        Label { color: lunar.muted; text: installerController.statusMessage; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                    Button { text: installerController.busy ? "Refreshing..." : "Refresh"; enabled: !installerController.busy && !installerController.planReady; onClicked: installerController.refresh() }
                }

                StackLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    currentIndex: installerController.planReady ? 1 : 0

                    ColumnLayout {
                        spacing: 12
                        ListView {
                            id: diskList
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            clip: true
                            model: installerController.disks
                            spacing: 9
                            delegate: Rectangle {
                                required property int index
                                required property string device
                                required property string sizeText
                                required property string description
                                required property string transport
                                required property bool systemDisk
                                required property bool eligible
                                required property string reason
                                width: diskList.width
                                height: 84
                                radius: 14
                                color: installerController.selectedIndex === index ? lunar.accentSoft : lunar.raised
                                border.color: installerController.selectedIndex === index ? lunar.accent : lunar.borderSoft
                                opacity: eligible ? 1.0 : 0.64
                                MouseArea { anchors.fill: parent; enabled: eligible; onClicked: installerController.selectDisk(index) }
                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 14; spacing: 14
                                    Rectangle { color: eligible ? lunar.accent : lunar.border; height: 48; radius: 12; width: 48; Label { anchors.centerIn: parent; color: lunar.foreground; font.pixelSize: 20; text: "▰" } }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 3
                                        Label { color: lunar.foreground; font.bold: true; text: "/dev/" + device + "  ·  " + sizeText }
                                        Label { color: lunar.muted; elide: Text.ElideRight; text: description + (transport === "unknown" ? "" : " · " + transport); Layout.fillWidth: true }
                                        Label { color: eligible ? lunar.success : lunar.warning; text: reason; Layout.fillWidth: true; elide: Text.ElideRight }
                                    }
                                    Label { color: systemDisk ? lunar.warning : lunar.muted; font.bold: true; text: systemDisk ? "CURRENT SYSTEM" : eligible ? "AVAILABLE" : "UNAVAILABLE" }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: installerController.selectedIndex >= 0 ? 150 : 0
                            visible: installerController.selectedIndex >= 0
                            color: lunar.raised
                            border.color: lunar.warning
                            radius: 14
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 14; spacing: 8
                                Label { color: lunar.warning; font.bold: true; text: "Confirm permanent erasure of /dev/" + installerController.selectedDevice }
                                TextField { id: confirmation; Layout.fillWidth: true; placeholderText: "Type " + installerController.selectedDevice; onTextChanged: installerController.setConfirmationText(text) }
                                CheckBox { id: eraseCheck; text: "I understand every partition and file on this disk will be permanently erased."; onCheckedChanged: installerController.setEraseAcknowledged(checked) }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 16
                        Rectangle {
                            Layout.fillHeight: true; Layout.fillWidth: true
                            color: lunar.raised; border.color: lunar.accent; radius: 16
                            Label { anchors.fill: parent; anchors.margins: 24; color: lunar.foreground; font.pixelSize: 15; text: installerController.planSummary; wrapMode: Text.WordWrap }
                        }
                        Label { Layout.fillWidth: true; color: lunar.warning; font.bold: true; text: "Installation execution is intentionally disabled until the privileged engine and recovery contract are reviewed."; wrapMode: Text.WordWrap }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button { text: "Back"; visible: installerController.planReady; onClicked: installerController.resetPlan() }
                    Item { Layout.fillWidth: true }
                    Button { text: installerController.planReady ? "Install unavailable" : "Review Plan"; enabled: !installerController.planReady && installerController.confirmationReady; onClicked: installerController.preparePlan() }
                }
            }
        }
    }
}
