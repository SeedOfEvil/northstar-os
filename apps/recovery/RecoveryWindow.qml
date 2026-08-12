import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

ApplicationWindow {
    id: recovery

    LunarPalette { id: lunar; darkMode: northstarDarkMode }

    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    height: 650
    minimumHeight: 560
    minimumWidth: 760
    title: "Northstar Recovery"
    visible: true
    width: 900

    background: NorthstarWindowFrame { darkMode: lunar.darkMode }

    Component.onCompleted: {
        raise()
        requestActivate()
        if (!recoverySelfTest) bootEnvironmentController.refresh()
    }

    Column {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        NorthstarWindowTitleBar {
            closeDestroysWindow: true
            iconSource: "qrc:/Northstar/Recovery/northstar-recovery.png"
            maximized: recovery.visibility === Window.Maximized
            lunarPalette: lunar
            subtitle: "Inspect recovery points and choose the next boot safely"
            title: "Recovery"
            width: parent.width
            window: recovery
            onMaximizeRequested: {
                if (recovery.visibility === Window.Maximized) recovery.showNormal()
                else recovery.showMaximized()
            }

            actions: [
                Button {
                    enabled: !bootEnvironmentController.busy
                    text: bootEnvironmentController.busy ? "Refreshing..." : "Refresh"
                    onClicked: bootEnvironmentController.refresh()
                }
            ]
        }

        Rectangle {
            border.color: lunar.borderSoft
            border.width: 1
            color: lunar.raised
            height: 66
            radius: 14
            width: parent.width

            Row {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    color: bootEnvironmentController.state === "error" ? "#d9485f" : lunar.accent
                    font.bold: true
                    font.pixelSize: 18
                    text: bootEnvironmentController.rebootRequired ? "Restart required" : "System protected"
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: lunar.muted
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    text: bootEnvironmentController.statusMessage
                    width: parent.width - 190
                }
            }
        }

        Text {
            color: lunar.foreground
            font.bold: true
            font.pixelSize: 16
            text: "Boot environments"
        }

        Rectangle {
            border.color: lunar.borderSoft
            border.width: 1
            color: lunar.panel
            radius: 14
            width: parent.width
            height: Math.max(190, recovery.height - 365)

            ListView {
                id: environmentList
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                model: bootEnvironmentController.environments
                spacing: 6

                delegate: Rectangle {
                    required property var modelData

                    border.color: modelData.name === bootEnvironmentController.selectedEnvironment
                        ? lunar.accent : lunar.borderSoft
                    border.width: modelData.name === bootEnvironmentController.selectedEnvironment ? 2 : 1
                    color: entryMouse.containsMouse || modelData.name === bootEnvironmentController.selectedEnvironment
                        ? lunar.raised : lunar.panelStrong
                    height: 72
                    radius: 12
                    width: environmentList.width

                    Row {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 14

                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            fillMode: Image.PreserveAspectFit
                            height: 34
                            source: "qrc:/Northstar/Recovery/northstar-recovery.png"
                            width: 34
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4
                            width: parent.width - 270
                            Text {
                                color: lunar.foreground
                                elide: Text.ElideMiddle
                                font.bold: true
                                font.pixelSize: 14
                                text: modelData.name
                                width: parent.width
                            }
                            Text {
                                color: lunar.muted
                                font.pixelSize: 11
                                text: modelData.created + "  ·  " + modelData.space
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: modelData.activeNow || modelData.activeNext ? lunar.accent : lunar.muted
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignRight
                            text: modelData.activeNow && modelData.activeNext ? "Current · Next boot"
                                : modelData.activeNow ? "Current"
                                : modelData.activeNext ? "Next boot"
                                : modelData.activatable ? "Recovery point" : "System environment"
                            width: 190
                        }
                    }

                    MouseArea {
                        id: entryMouse
                        anchors.fill: parent
                        enabled: modelData.managed
                        hoverEnabled: true
                        onClicked: bootEnvironmentController.selectEnvironment(modelData.name)
                    }
                }

                ScrollBar.vertical: ScrollBar { }
            }

            Text {
                anchors.centerIn: parent
                color: lunar.muted
                text: bootEnvironmentController.busy ? "Reading protected system state..." : "No boot environments available"
                visible: environmentList.count === 0
            }
        }

        Row {
            spacing: 10
            width: parent.width

            Button {
                enabled: bootEnvironmentController.environments.length > 0 && !bootEnvironmentController.busy
                text: "Export Diagnostics"
                onClicked: bootEnvironmentController.exportDiagnostics()
            }

            Item { height: 1; width: Math.max(1, parent.width - 350) }

            Button {
                enabled: bootEnvironmentController.selectedEnvironment.length > 0
                    && !bootEnvironmentController.busy
                text: "Use Recovery Point..."
                onClicked: activationDialog.open()
            }
        }

        Text {
            color: lunar.muted
            font.pixelSize: 11
            text: "Recovery changes only the environment selected for the next boot. It never deletes snapshots, modifies Home, or restarts automatically."
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }

    Dialog {
        id: activationDialog
        anchors.centerIn: parent
        modal: true
        padding: 18
        standardButtons: Dialog.Cancel
        title: "Use this recovery point at next boot?"
        width: 520

        contentItem: Column {
            spacing: 12
            width: activationDialog.width - (2 * activationDialog.padding)

            Text {
                color: lunar.foreground
                text: "Administrator authentication is required. Type the exact environment name to continue:"
                wrapMode: Text.WordWrap
                width: parent.width
            }
            Text {
                color: lunar.accent
                font.bold: true
                font.pixelSize: 13
                text: bootEnvironmentController.selectedEnvironment
                wrapMode: Text.WrapAnywhere
                width: parent.width
            }
            TextField {
                id: confirmationField
                placeholderText: "Exact recovery-point name"
                selectByMouse: true
                width: parent.width
                onTextChanged: bootEnvironmentController.setConfirmationText(text)
            }
            Button {
                enabled: bootEnvironmentController.activationReady
                text: "Authenticate and Schedule"
                width: parent.width
                onClicked: {
                    bootEnvironmentController.scheduleActivation()
                    activationDialog.close()
                }
            }
        }

        onOpened: {
            confirmationField.text = ""
            confirmationField.forceActiveFocus()
        }
    }

    NativeWindowResizeHandler {
        resizingEnabled: recovery.visibility !== Window.Maximized
        window: recovery
    }
}
