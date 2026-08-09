import QtQuick
import QtQuick.Controls

Window {
    id: software

    property var packageCatalog
    property var packageTrust
    property var updatePlan
    property var updateAuthorization
    property var state
    property var targetScreen
    property int panelHeight: 44
    property int desktopMargin: 24
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property int minimumSurfaceWidth: 700
    property int minimumSurfaceHeight: 500
    property bool dragging: false
    property var selectedPackage: null
    property point dragOrigin: Qt.point(0, 0)
    property point windowOrigin: Qt.point(0, 0)
    property point resizeOrigin: Qt.point(0, 0)
    property size resizeSize: Qt.size(0, 0)
    property color surfaceBackground: state && state.darkMode ? "#171a21" : "#f4f6fb"
    property color surfaceForeground: state && state.darkMode ? "#f5f7fb" : "#1e2430"
    property color surfaceMuted: state && state.darkMode ? "#a9b1c2" : "#637083"
    property color surfaceAccent: state && state.darkMode ? "#79b8ff" : "#1769aa"
    property color surfaceRaised: state && state.darkMode ? "#252b36" : "#e8edf5"

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.NonModal
    title: "Northstar Software"

    function activateSearchField() {
        if (!software.visible) {
            return
        }
        software.raise()
        software.requestActivate()
        searchField.forceActiveFocus()
    }

    onVisibleChanged: {
        if (visible) {
            // The compositor may not have mapped the window when visible changes.
            // Defer focus until the surface can accept keyboard activation.
            Qt.callLater(software.activateSearchField)
        }
    }

    onActiveChanged: {
        if (active && visible) {
            searchField.forceActiveFocus()
        }
    }

    minimumWidth: software.minimumSurfaceWidth
    minimumHeight: software.minimumSurfaceHeight
    width: Math.min(980, Math.max(software.minimumSurfaceWidth,
        software.screenWidth - (software.desktopMargin * 2)))
    height: Math.min(700, Math.max(software.minimumSurfaceHeight,
        software.screenHeight - software.panelHeight - (software.desktopMargin * 2)))
    x: software.screenX + Math.max(software.desktopMargin, (software.screenWidth - software.width) / 2)
    y: software.screenY + software.panelHeight
        + Math.max(software.desktopMargin, (software.screenHeight - software.panelHeight - software.height) / 2)

    function openSoftware() {
        if (!software.packageCatalog) {
            return
        }
        software.packageCatalog.setQuery("")
        searchField.text = ""
        software.packageCatalog.refresh()
        show()
        raise()
        requestActivate()
        Qt.callLater(software.activateSearchField)
    }

    function showPackageDetails(packageInfo) {
        software.selectedPackage = packageInfo
        packageDetailsDialog.open()
    }

    function beginDrag(mouseX, mouseY) {
        software.dragging = true
        software.dragOrigin = Qt.point(mouseX, mouseY)
        software.windowOrigin = Qt.point(software.x, software.y)
    }

    function updateDrag(mouseX, mouseY) {
        if (!software.dragging) {
            return
        }
        const deltaX = mouseX - software.dragOrigin.x
        const deltaY = mouseY - software.dragOrigin.y
        const maxX = software.screenX + software.screenWidth - software.width
        const maxY = software.screenY + software.screenHeight - software.height
        software.x = Math.max(software.screenX, Math.min(maxX, software.windowOrigin.x + deltaX))
        software.y = Math.max(software.screenY + software.panelHeight,
                              Math.min(maxY, software.windowOrigin.y + deltaY))
    }

    function endDrag() {
        software.dragging = false
    }

    function beginResize(mouseX, mouseY) {
        software.resizeOrigin = Qt.point(mouseX, mouseY)
        software.resizeSize = Qt.size(software.width, software.height)
    }

    function updateResize(mouseX, mouseY) {
        const deltaX = mouseX - software.resizeOrigin.x
        const deltaY = mouseY - software.resizeOrigin.y
        software.width = Math.min(software.screenX + software.screenWidth - software.x,
                                  Math.max(software.minimumSurfaceWidth, software.resizeSize.width + deltaX))
        software.height = Math.min(software.screenY + software.screenHeight - software.y,
                                   Math.max(software.minimumSurfaceHeight, software.resizeSize.height + deltaY))
    }

    Rectangle {
        anchors.fill: parent
        color: software.surfaceBackground
        border.color: software.surfaceAccent
        border.width: 1
        radius: 12

        Column {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 16

            Item {
                id: titleBar
                height: 54
                width: parent.width

                MouseArea {
                    anchors.fill: parent
                    onPressed: software.beginDrag(mouse.x, mouse.y)
                    onPositionChanged: software.updateDrag(mouse.x, mouse.y)
                    onReleased: software.endDrag()
                    onCanceled: software.endDrag()
                }

                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12

                    NorthstarIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 38
                        width: 38
                        iconName: "software"
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Text {
                            color: software.surfaceForeground
                            font.bold: true
                            font.pixelSize: 24
                            text: "Software"
                        }

                        Text {
                            color: software.surfaceMuted
                            font.pixelSize: 12
                            text: "Installed FreeBSD packages"
                        }
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Rectangle {
                        color: planMouse.containsMouse ? software.surfaceAccent : software.surfaceRaised
                        height: 34
                        radius: 6
                        width: 124

                        Text {
                            anchors.centerIn: parent
                            color: software.surfaceForeground
                            font.pixelSize: 12
                            text: "Plan Update"
                        }

                        MouseArea {
                            id: planMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (software.packageTrust) {
                                    software.packageTrust.planUpdate()
                                }
                                if (software.updatePlan && software.packageCatalog) {
                                    software.updatePlan.reload()
                                    software.updatePlan.preview(software.packageCatalog.packages)
                                }
                            }
                        }
                    }

                    Rectangle {
                        color: refreshMouse.containsMouse ? software.surfaceAccent : software.surfaceRaised
                        height: 34
                        radius: 6
                        width: 108

                        Text {
                            anchors.centerIn: parent
                            color: software.surfaceForeground
                            font.pixelSize: 12
                            text: software.packageCatalog && software.packageCatalog.refreshing
                                ? "Refreshing..." : "Refresh"
                        }

                        MouseArea {
                            id: refreshMouse
                            anchors.fill: parent
                            enabled: software.packageCatalog && !software.packageCatalog.refreshing
                            hoverEnabled: true
                            onClicked: {
                                software.packageCatalog.refresh()
                                if (software.updatePlan) {
                                    software.updatePlan.preview(software.packageCatalog.packages)
                                }
                            }
                        }
                    }

                    Rectangle {
                        color: software.surfaceRaised
                        height: 34
                        radius: 6
                        width: 76

                        Text {
                            anchors.centerIn: parent
                            color: software.surfaceForeground
                            font.pixelSize: 12
                            text: "Close"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: software.hide()
                        }
                    }
                }
            }

            Row {
                spacing: 12
                width: parent.width

                Rectangle {
                    color: software.surfaceRaised
                    height: 72
                    radius: 8
                    width: (parent.width - 36) / 4

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            color: software.surfaceMuted
                            font.pixelSize: 11
                            text: "Installed"
                        }

                        Text {
                            color: software.surfaceForeground
                            font.bold: true
                            font.pixelSize: 22
                            text: software.packageCatalog ? software.packageCatalog.installedCount : "—"
                        }
                    }
                }

                Rectangle {
                    color: software.surfaceRaised
                    height: 72
                    radius: 8
                    width: (parent.width - 36) / 4

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            color: software.surfaceMuted
                            font.pixelSize: 11
                            text: "Package manager"
                        }

                        Text {
                            color: software.packageCatalog && software.packageCatalog.available
                                ? "#55c58a" : "#c34f65"
                            font.bold: true
                            font.pixelSize: 14
                            text: software.packageCatalog && software.packageCatalog.available
                                ? "Available" : "Unavailable"
                        }
                    }
                }

                Rectangle {
                    color: software.surfaceRaised
                    height: 72
                    radius: 8
                    width: (parent.width - 36) / 4

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            color: software.surfaceMuted
                            font.pixelSize: 11
                            text: "Repository policy"
                        }

                        Text {
                            color: software.packageTrust && software.packageTrust.policyValid
                                && software.packageTrust.trustStoreValid
                                ? "#55c58a" : "#c34f65"
                            font.bold: true
                            font.pixelSize: 13
                            text: !software.packageTrust
                                ? "Unavailable"
                                : software.packageTrust.policyValid && software.packageTrust.trustStoreValid
                                    ? "Ready"
                                    : software.packageTrust.policyPresent
                                        ? software.packageTrust.policyValid
                                            ? "Trust store incomplete" : "Rejected"
                                        : "Not configured"
                        }
                    }
                }

                Rectangle {
                    color: software.surfaceRaised
                    height: 72
                    radius: 8
                    width: (parent.width - 36) / 4

                    Column {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            color: software.surfaceMuted
                            font.pixelSize: 11
                            text: "Publication metadata"
                        }

                        Text {
                            color: software.updatePlan && software.updatePlan.metadataValid
                                ? (software.updatePlan.signatureVerified ? "#70d6a6" : "#f0b45a")
                                : "#c34f65"
                            font.bold: true
                            font.pixelSize: 13
                            text: !software.updatePlan
                                ? "Unavailable"
                                : software.updatePlan.metadataValid
                                    ? software.updatePlan.signatureVerified
                                        ? "Parsed / verified"
                                        : "Parsed / not verified"
                                    : software.updatePlan.metadataPresent ? "Rejected" : "Not configured"
                        }
                    }
                }
            }

            Rectangle {
                color: software.surfaceRaised
                height: 72
                radius: 8
                width: parent.width

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    NorthstarIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 30
                        width: 30
                        iconName: "settings"
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 3
                        width: parent.width - 42

                        Row {
                            spacing: 10

                            Text {
                                color: software.surfaceMuted
                                font.pixelSize: 11
                                text: "Update safety"
                            }

                            Text {
                                color: !software.updateAuthorization
                                    ? software.surfaceMuted
                                    : software.updateAuthorization.preflightValid
                                        ? "#f0b45a" : "#c34f65"
                                font.bold: true
                                font.pixelSize: 13
                                text: !software.updateAuthorization
                                    ? "Unavailable"
                                    : software.updateAuthorization.authorizationAvailable
                                        ? "Authorized"
                                        : software.updateAuthorization.preflightValid
                                            ? "Preflight only" : "Blocked"
                            }
                        }

                        Text {
                            color: software.surfaceMuted
                            elide: Text.ElideRight
                            font.pixelSize: 11
                            text: software.updateAuthorization
                                ? software.updateAuthorization.status
                                : "Update authorization is unavailable."
                            width: parent.width
                        }
                    }
                }
            }

            TextField {
                id: searchField
                activeFocusOnPress: true
                color: software.surfaceForeground
                focus: true
                font.pixelSize: 13
                placeholderText: "Search installed packages..."
                placeholderTextColor: software.surfaceMuted
                selectByMouse: true
                width: parent.width

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: software.activateSearchField()
                }

                background: Rectangle {
                    color: software.surfaceRaised
                    border.color: searchField.activeFocus ? software.surfaceAccent : "transparent"
                    border.width: 1
                    radius: 7
                }

                onTextChanged: {
                    if (software.packageCatalog && software.packageCatalog.query !== text) {
                        software.packageCatalog.setQuery(text)
                    }
                }
            }

            Row {
                spacing: 10
                width: parent.width

                NorthstarIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 24
                    width: 24
                    iconName: software.packageCatalog && software.packageCatalog.available
                        ? "info" : "northstar"
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    width: parent.width - 34

                    Text {
                        color: software.surfaceMuted
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        text: software.packageCatalog
                            ? software.packageCatalog.statusMessage : "Package inventory is unavailable."
                        width: parent.width
                    }

                    Text {
                        color: software.surfaceMuted
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        text: software.packageTrust
                            ? software.packageTrust.trustStatus : "Signed repository policy is unavailable."
                        width: parent.width
                    }

                    Text {
                        color: software.surfaceMuted
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        text: software.packageTrust
                            ? software.packageTrust.updatePlanStatus : "Update planning is unavailable."
                        width: parent.width
                    }

                    Text {
                        color: software.surfaceMuted
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        text: software.updatePlan
                            ? software.updatePlan.metadataStatus : "Repository metadata is unavailable."
                        width: parent.width
                    }

                    Text {
                        color: software.surfaceMuted
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        text: software.updatePlan
                            ? software.updatePlan.catalogueStatus : "Repository catalogue integrity is unavailable."
                        width: parent.width
                    }

                    Text {
                        color: software.surfaceMuted
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        text: software.updatePlan
                            ? software.updatePlan.planStatus : "Update preview is unavailable."
                        width: parent.width
                    }

                    Text {
                        color: software.surfaceMuted
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        text: software.updatePlan ? software.updatePlan.planPreview : ""
                        visible: text.length > 0
                        width: parent.width
                    }
                }
            }

            Rectangle {
                color: software.surfaceRaised
                radius: 8
                width: parent.width
                height: Math.max(180, parent.height - y - 38)

                ListView {
                    id: packageList
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true
                    model: software.packageCatalog ? software.packageCatalog.matchingPackages : []
                    spacing: 8

                    delegate: Rectangle {
                        required property var modelData

                        color: packageMouse.containsMouse ? software.surfaceAccent : software.surfaceBackground
                        height: 64
                        radius: 7
                        width: packageList.width

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            NorthstarIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                height: 30
                                width: 30
                                iconName: "software"
                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                width: parent.width - 40

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Text {
                                        color: software.surfaceForeground
                                        elide: Text.ElideRight
                                        font.bold: true
                                        font.pixelSize: 13
                                        text: modelData.name
                                        width: Math.min(260, parent.width * 0.45)
                                    }

                                    Text {
                                        color: software.surfaceMuted
                                        elide: Text.ElideRight
                                        font.pixelSize: 11
                                        text: modelData.version
                                        width: parent.width - 280
                                    }
                                }

                                Text {
                                    color: software.surfaceMuted
                                    elide: Text.ElideRight
                                    font.pixelSize: 11
                                    text: modelData.comment || "No package description provided."
                                    width: parent.width
                                }
                            }
                        }

                        MouseArea {
                            id: packageMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: software.showPackageDetails(modelData)
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        color: software.surfaceMuted
                        font.pixelSize: 13
                        text: software.packageCatalog && software.packageCatalog.query.length > 0
                            ? "No installed packages match this search."
                            : "Refresh to read the installed package inventory."
                        visible: packageList.count === 0
                    }
                }
            }

            Text {
                color: software.surfaceMuted
                font.pixelSize: 11
                text: "Read-only inventory and provenance-aware update preview. Update authorization, package mutation, and ZFS rollback remain protected M4 work."
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
    }

    Dialog {
        id: packageDetailsDialog
        title: software.selectedPackage ? software.selectedPackage.name : "Package details"
        modal: true
        standardButtons: Dialog.Close
        width: Math.min(560, software.width - 48)
        x: (software.width - width) / 2
        y: (software.height - height) / 2

        background: Rectangle {
            color: software.surfaceBackground
            border.color: software.surfaceAccent
            border.width: 1
            radius: 10
        }

        contentItem: Column {
            spacing: 14
            width: packageDetailsDialog.width - (2 * packageDetailsDialog.padding)

            Row {
                spacing: 12
                width: parent.width

                NorthstarIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 42
                    width: 42
                    iconName: "software"
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    width: parent.width - 54

                    Text {
                        color: software.surfaceForeground
                        elide: Text.ElideRight
                        font.bold: true
                        font.pixelSize: 18
                        text: software.selectedPackage ? software.selectedPackage.name : "Package details"
                        width: parent.width
                    }

                    Text {
                        color: software.surfaceMuted
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        text: software.selectedPackage
                            ? "Installed version " + software.selectedPackage.version
                            : ""
                        width: parent.width
                    }
                }
            }

            Text {
                color: software.surfaceForeground
                font.pixelSize: 13
                text: software.selectedPackage && software.selectedPackage.comment
                    ? software.selectedPackage.comment
                    : "No package description provided."
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Rectangle {
                color: software.surfaceRaised
                radius: 8
                width: parent.width
                height: 76

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 5

                    Text {
                        color: "#55c58a"
                        font.bold: true
                        font.pixelSize: 12
                        text: "Installed on this system"
                    }

                    Text {
                        color: software.surfaceMuted
                        font.pixelSize: 11
                        text: "Install, remove, and upgrade actions remain disabled until the signed repository and privileged update gates are complete."
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Row {
                spacing: 8

                Button {
                    enabled: false
                    text: "Install"
                }

                Button {
                    enabled: false
                    text: "Remove"
                }
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        color: software.surfaceAccent
        height: 18
        opacity: 0.8
        width: 18

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            onPressed: software.beginResize(mouse.x, mouse.y)
            onPositionChanged: software.updateResize(mouse.x, mouse.y)
        }
    }
}
