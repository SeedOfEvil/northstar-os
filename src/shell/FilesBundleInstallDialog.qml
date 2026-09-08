import QtQuick
import QtQuick.Controls

Window {
    id: bundleInstallDialog
    objectName: "bundleInstallDialog"
    required property var ownerWindow
    required property var theme
    property alias statusText: bundleInstallStatus.text
    color: "transparent"
    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.WindowModal
    transientParent: ownerWindow
    visible: false
    property string itemPath: ""
    property var details: ({})
    property bool operationComplete: false
    property bool operationError: false
    title: details.valid ? details.displayName : "Northstar application"
    width: Math.min(520, ownerWindow.width - 48)
    height: Math.min(ownerWindow.screen.height - 64, Math.max(300, installDialogContent.implicitHeight + 40))
    x: ownerWindow.x + (ownerWindow.width - width) / 2
    y: ownerWindow.y + (ownerWindow.height - height) / 2

    Rectangle {
        anchors.fill: parent
        color: ownerWindow.surfaceBackground
        border.color: theme.borderSoft
        border.width: 1
        radius: theme.radiusLarge

        ScrollView {
            id: installDialogScroll
            objectName: "installDialogScroll"
            anchors.fill: parent
            anchors.margins: 20
            clip: true
            contentWidth: availableWidth

        Column {
            id: installDialogContent
            width: installDialogScroll.availableWidth
            spacing: 12

            Text {
                color: ownerWindow.surfaceForeground
                font.bold: true
                font.pixelSize: 18
                text: bundleInstallDialog.operationComplete
                    ? bundleInstallDialog.details.displayName + " was installed"
                    : bundleInstallDialog.details.valid
                    ? "Install " + bundleInstallDialog.details.displayName + "?"
                    : "This application cannot be installed"
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Text {
                color: ownerWindow.surfaceMuted
                text: bundleInstallDialog.details.valid
                    ? "Version " + bundleInstallDialog.details.version
                        + "  -  " + bundleInstallDialog.details.bundleIdentifier
                    : (bundleInstallDialog.details.validationError
                        || "Northstar could not validate this application.")
                width: parent.width
                wrapMode: Text.WrapAnywhere
            }

            Rectangle {
                color: theme.raised
                radius: theme.radiusMedium
                height: visible ? installProvenance.implicitHeight + 24 : 0
                visible: bundleInstallDialog.details.valid === true
                width: parent.width

                Column {
                    id: installProvenance
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    Text { color: ownerWindow.surfaceForeground; text: "Source: " + (bundleInstallDialog.details.source || ""); width: parent.width; wrapMode: Text.WrapAnywhere }
                    Text { color: ownerWindow.surfaceForeground; text: "Package: " + (bundleInstallDialog.details.package || ""); width: parent.width; wrapMode: Text.WrapAnywhere }
                    Text { color: ownerWindow.surfaceForeground; text: "Revision: " + (bundleInstallDialog.details.revision || ""); width: parent.width; wrapMode: Text.WrapAnywhere }
                    Text { color: ownerWindow.surfaceForeground; visible: !!bundleInstallDialog.details.webUrl; text: "Website: " + (bundleInstallDialog.details.webUrl || ""); width: parent.width; wrapMode: Text.WrapAnywhere }
                    Text { color: ownerWindow.surfaceForeground; visible: !!bundleInstallDialog.details.webUrl; text: "Origin: " + (bundleInstallDialog.details.webOrigin || ""); width: parent.width; wrapMode: Text.WrapAnywhere }
                    Text { color: ownerWindow.surfaceMuted; visible: !!bundleInstallDialog.details.webUrl; text: bundleInstallDialog.details.webNotice || ""; width: parent.width; wrapMode: Text.WordWrap }
                    Text {
                        color: ownerWindow.surfaceMuted
                        width: parent.width
                        wrapMode: Text.WordWrap
                        textFormat: Text.PlainText
                        visible: !!bundleInstallDialog.details.compatibility
                        text: "Compatibility: " + (bundleInstallDialog.details.compatibility || {}).format
                              + "\n" + ((bundleInstallDialog.details.compatibility || {}).message || "")
                    }
                }
            }

            Text {
                color: bundleInstallDialog.details.alreadyInstalled ? theme.danger : ownerWindow.surfaceMuted
                text: bundleInstallDialog.operationComplete ? ""
                    : !bundleInstallDialog.details.valid ? ""
                    : bundleInstallDialog.details.installedScope === "system"
                        ? "A package-owned application already uses this identifier. Northstar will not replace or shadow it."
                    : bundleInstallDialog.details.installedScope === "user"
                        ? "This application is already installed for your account."
                    : "The application will be copied into your private Applications directory without administrator access."
                visible: text.length > 0
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Text {
                id: bundleInstallStatus
                color: bundleInstallDialog.operationError ? theme.danger : theme.success
                visible: text.length > 0
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Item {
                height: installButtons.implicitHeight
                width: parent.width

                Row {
                    id: installButtons
                    anchors.right: parent.right
                    spacing: 10

                    Button {
                        text: bundleInstallDialog.operationComplete ? "Done" : "Cancel"
                        onClicked: bundleInstallDialog.close()
                    }

                    Button {
                        enabled: bundleInstallDialog.details.valid === true
                            && bundleInstallDialog.details.alreadyInstalled !== true
                            && !bundleInstallDialog.operationComplete
                        visible: !bundleInstallDialog.operationComplete
                        text: "Install"
                        onClicked: {
                            if (!ownerWindow.applicationLauncher) {
                                bundleInstallStatus.text = "The Northstar application installer is unavailable."
                                bundleInstallDialog.operationError = true
                                return
                            }
                            const succeeded = ownerWindow.applicationLauncher.installApplicationBundle(
                                bundleInstallDialog.itemPath)
                            bundleInstallDialog.operationError = ownerWindow.applicationLauncher.applicationBundleError()
                            bundleInstallStatus.text = ownerWindow.applicationLauncher.applicationBundleStatusMessage()
                            if (succeeded) {
                                bundleInstallDialog.operationComplete = true
                                bundleInstallDialog.details = ownerWindow.applicationLauncher.applicationBundleDetails(
                                    bundleInstallDialog.itemPath)
                                if (ownerWindow.applicationLauncher) {
                                    ownerWindow.applicationLauncher.refreshApplications()
                                }
                            }
                        }
                    }
                }
            }
        }
        }
    }
}
