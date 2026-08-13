import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Northstar.Ui 1.0

ApplicationWindow {
    id: root
    property bool recoveryOpen: false
    LunarPalette { id: lunar; darkMode: northstarDarkMode }

    color: lunar.background
    height: Screen.height
    minimumHeight: 680
    minimumWidth: 1000
    title: "Install Northstar"
    visible: true
    width: Screen.width
    x: 0
    y: 0

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
            Layout.fillWidth: false
            Layout.maximumWidth: 245
            Layout.minimumWidth: 245
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
            Label { Layout.fillWidth: true; color: lunar.muted; font.pixelSize: 11; text: "Installation media revalidates every protected phase. Interrupted attempts restart from a new reviewed plan."; wrapMode: Text.WordWrap }
        }

        NorthstarWindowFrame {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumWidth: 600
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
                        Label { color: lunar.foreground; font.bold: true; font.pixelSize: 27; text: root.recoveryOpen ? "Installation recovery" : installerController.planReady ? "Review installation plan" : "Select a destination" }
                        Label { color: lunar.muted; text: root.recoveryOpen ? installerRecoveryController.statusMessage : installerController.statusMessage; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                    Button {
                        text: root.recoveryOpen ? "Destinations" : "Recovery"
                        enabled: !installerController.busy && !installerRecoveryController.busy
                        onClicked: {
                            if (root.recoveryOpen) {
                                root.recoveryOpen = false
                                installerRecoveryController.reset()
                            } else {
                                root.recoveryOpen = true
                                installerRecoveryController.checkStatus()
                            }
                        }
                    }
                    Button { visible: !root.recoveryOpen; text: installerController.busy ? "Refreshing..." : "Refresh"; enabled: !installerController.busy && !installerController.planReady; onClicked: installerController.refresh() }
                }

                StackLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    currentIndex: root.recoveryOpen ? 2 : installerController.planReady ? 1 : 0

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
                        Label { Layout.fillWidth: true; color: lunar.warning; font.bold: true; text: "This review does not start installation. Execution remains gated by authenticated Northstar release media."; wrapMode: Text.WordWrap }
                    }

                    ColumnLayout {
                        spacing: 14

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 170
                            color: lunar.raised
                            border.color: installerRecoveryController.interruptedExecution ? lunar.warning : lunar.borderSoft
                            radius: 16

                            GridLayout {
                                anchors.fill: parent
                                anchors.margins: 18
                                columns: 2
                                columnSpacing: 20
                                rowSpacing: 8
                                Label { color: lunar.muted; text: "State" }
                                Label { color: lunar.foreground; font.bold: true; text: installerRecoveryController.state }
                                Label { color: lunar.muted; text: "Transaction" }
                                Label { color: lunar.foreground; elide: Text.ElideMiddle; Layout.fillWidth: true; text: installerRecoveryController.transactionId || "—" }
                                Label { color: lunar.muted; text: "Target" }
                                Label { color: lunar.foreground; font.bold: true; text: installerRecoveryController.targetDevice ? "/dev/" + installerRecoveryController.targetDevice : "—" }
                                Label { color: lunar.muted; text: "Last safe phase" }
                                Label { color: lunar.foreground; text: installerRecoveryController.lastPhase || "—" }
                                Label { color: lunar.muted; text: "Disk changes started" }
                                Label { color: installerRecoveryController.mutationStarted ? lunar.warning : lunar.success; font.bold: true; text: installerRecoveryController.mutationStarted ? "Yes" : "No" }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: installerRecoveryController.interruptedExecution ? 190 : 120
                            color: lunar.panelStrong
                            border.color: lunar.borderSoft
                            radius: 16

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 9
                                Label {
                                    Layout.fillWidth: true
                                    color: lunar.foreground
                                    font.bold: true
                                    text: installerRecoveryController.interruptedExecution
                                        ? "Start over safely"
                                        : installerRecoveryController.state === "idle"
                                            ? "Ready to install"
                                            : "Protected state requires attention"
                                }
                                Label {
                                    Layout.fillWidth: true
                                    color: lunar.muted
                                    text: installerRecoveryController.interruptedExecution
                                        ? "The interrupted attempt is never resumed midway. Export its sanitized evidence, type the exact target again, and archive it before creating a new plan."
                                        : installerRecoveryController.state === "idle"
                                            ? "No unfinished installation is blocking a new destination review."
                                            : "Do not remove installer media or alter disks until the protected state has been reviewed."
                                    wrapMode: Text.WordWrap
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    visible: installerRecoveryController.interruptedExecution
                                    placeholderText: "Type " + installerRecoveryController.targetDevice
                                    onTextChanged: installerRecoveryController.setRetryConfirmationText(text)
                                }
                                Label {
                                    Layout.fillWidth: true
                                    visible: installerRecoveryController.diagnosticsReady
                                    color: lunar.success
                                    text: installerRecoveryController.diagnosticPreview
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.fillWidth: true
                            Button {
                                text: installerRecoveryController.busy ? "Working..." : "Check again"
                                enabled: !installerRecoveryController.busy
                                onClicked: installerRecoveryController.checkStatus()
                            }
                            Button {
                                visible: installerRecoveryController.interruptedExecution
                                text: "Export Diagnostics"
                                enabled: !installerRecoveryController.busy
                                onClicked: installerRecoveryController.exportDiagnostics()
                            }
                            Item { Layout.fillWidth: true }
                            Button {
                                visible: installerRecoveryController.interruptedExecution
                                text: "Prepare Clean Retry"
                                enabled: !installerRecoveryController.busy && installerRecoveryController.retryConfirmationReady
                                onClicked: installerRecoveryController.prepareCleanRetry()
                            }
                            Button {
                                visible: installerRecoveryController.state === "idle" || installerRecoveryController.state === "retry-ready"
                                text: "Return to Destinations"
                                enabled: !installerRecoveryController.busy
                                onClicked: {
                                    root.recoveryOpen = false
                                    installerRecoveryController.reset()
                                    installerController.refresh()
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button { text: "Back"; visible: !root.recoveryOpen && installerController.planReady; onClicked: installerController.resetPlan() }
                    Item { Layout.fillWidth: true }
                    Button { visible: !root.recoveryOpen; text: installerController.planReady ? "Awaiting Release Media" : "Review Plan"; enabled: !installerController.planReady && installerController.confirmationReady; onClicked: installerController.preparePlan() }
                }
            }
        }
    }
}
