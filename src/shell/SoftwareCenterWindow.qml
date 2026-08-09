import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: software

    LunarPalette {
        id: lunar
        darkMode: software.state ? software.state.darkMode : true
    }

    property var packageCatalog
    property var packageTrust
    property var updatePlan
    property var updateAuthorization
    property var applicationLauncher
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
    property bool maximized: false
    property point normalGeometryPosition: Qt.point(0, 0)
    property size normalGeometrySize: Qt.size(0, 0)
    property var selectedPackage: null
    property var selectedApplication: null
    property point dragOrigin: Qt.point(0, 0)
    property point windowOrigin: Qt.point(0, 0)
    property point resizeOrigin: Qt.point(0, 0)
    property size resizeSize: Qt.size(0, 0)
    property color surfaceBackground: lunar.panelStrong
    property color surfaceForeground: lunar.foreground
    property color surfaceMuted: lunar.muted
    property color surfaceAccent: lunar.accent
    property color surfaceRaised: lunar.raised

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
        if (software.applicationLauncher) {
            software.applicationLauncher.setApplicationQuery("")
            software.applicationLauncher.refreshApplications()
        }
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

    function showApplicationDetails(applicationInfo) {
        software.selectedApplication = applicationInfo
        applicationDetailsDialog.open()
    }

    function openUpdatePlan() {
        if (software.packageTrust) {
            software.packageTrust.planUpdate()
        }
        if (software.updatePlan && software.packageCatalog) {
            software.updatePlan.reload()
            software.updatePlan.preview(software.packageCatalog.packages)
        }
        if (software.updateAuthorization) {
            software.updateAuthorization.refresh()
        }
        updatePlanDialog.open()
    }

    function beginDrag(mouseX, mouseY) {
        if (software.maximized) {
            return
        }
        if (software.startSystemMove()) {
            software.dragging = false
            return
        }
        software.dragging = true
        software.dragOrigin = Qt.point(mouseX, mouseY)
        software.windowOrigin = Qt.point(software.x, software.y)
    }

    function updateDrag(mouseX, mouseY) {
        if (!software.dragging || software.maximized) {
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
        if (software.maximized) {
            return
        }
        software.resizeOrigin = Qt.point(mouseX, mouseY)
        software.resizeSize = Qt.size(software.width, software.height)
    }

    function updateResize(mouseX, mouseY) {
        if (software.maximized) {
            return
        }
        const deltaX = mouseX - software.resizeOrigin.x
        const deltaY = mouseY - software.resizeOrigin.y
        software.width = Math.min(software.screenX + software.screenWidth - software.x,
                                  Math.max(software.minimumSurfaceWidth, software.resizeSize.width + deltaX))
        software.height = Math.min(software.screenY + software.screenHeight - software.y,
                                   Math.max(software.minimumSurfaceHeight, software.resizeSize.height + deltaY))
    }

    function toggleMaximize() {
        if (software.maximized) {
            software.x = software.normalGeometryPosition.x
            software.y = software.normalGeometryPosition.y
            software.width = software.normalGeometrySize.width
            software.height = software.normalGeometrySize.height
            software.maximized = false
            return
        }
        software.normalGeometryPosition = Qt.point(software.x, software.y)
        software.normalGeometrySize = Qt.size(software.width, software.height)
        software.x = software.screenX
        software.y = software.screenY + software.panelHeight
        software.width = software.screenWidth
        software.height = Math.max(software.minimumSurfaceHeight,
            software.screenHeight - software.panelHeight)
        software.maximized = true
    }

    NorthstarWindowFrame {
        anchors.fill: parent
        darkMode: lunar.darkMode

        Column {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 16

            Item {
                id: titleBar
                height: 54
                width: parent.width

                NativeWindowMoveHandler {
                    enabled: false
                    window: software
                }

                NorthstarWindowTitleBar {
                    anchors.fill: parent
                    maximized: software.maximized
                    lunarPalette: lunar
                    subtitle: "Installed FreeBSD packages"
                    title: "Software"
                    window: software
                    onMaximizeRequested: software.toggleMaximize()

                    actions: [
                        Rectangle {
                            color: sharedPlanMouse.containsMouse ? lunar.raisedHover : software.surfaceRaised
                            height: 34
                            radius: lunar.radiusSmall
                            width: 112
                            Text {
                                anchors.centerIn: parent
                                color: software.surfaceForeground
                                font.pixelSize: 12
                                text: "Plan Update"
                            }
                            MouseArea {
                                id: sharedPlanMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: software.openUpdatePlan()
                            }
                        },
                        Rectangle {
                            color: sharedRefreshMouse.containsMouse ? lunar.raisedHover : software.surfaceRaised
                            height: 34
                            radius: lunar.radiusSmall
                            width: 96
                            Text {
                                anchors.centerIn: parent
                                color: software.surfaceForeground
                                font.pixelSize: 12
                                text: software.packageCatalog && software.packageCatalog.refreshing
                                    ? "Refreshing..." : "Refresh"
                            }
                            MouseArea {
                                id: sharedRefreshMouse
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
                    ]
                }

                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12
                    visible: false

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
                    visible: false

                    Rectangle {
                        color: minimizeSoftwareMouse.containsMouse ? lunar.warning : software.surfaceRaised
                        height: 34
                        radius: 17
                        width: 34

                        Text {
                            anchors.centerIn: parent
                            color: software.surfaceForeground
                            font.bold: true
                            font.pixelSize: 14
                            text: "−"
                        }

                        MouseArea {
                            id: minimizeSoftwareMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: software.hide()
                        }
                    }

                    Rectangle {
                        color: maximizeSoftwareMouse.containsMouse ? lunar.success : software.surfaceRaised
                        height: 34
                        radius: 17
                        width: 34

                        Text {
                            anchors.centerIn: parent
                            color: software.surfaceForeground
                            font.pixelSize: 13
                            text: software.maximized ? "❐" : "□"
                        }

                        MouseArea {
                            id: maximizeSoftwareMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: software.toggleMaximize()
                        }
                    }

                    Rectangle {
                        color: planMouse.containsMouse ? lunar.raisedHover : software.surfaceRaised
                        height: 34
                        radius: lunar.radiusSmall
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
                            onClicked: software.openUpdatePlan()
                        }
                    }

                    Rectangle {
                        color: refreshMouse.containsMouse ? lunar.raisedHover : software.surfaceRaised
                        height: 34
                        radius: lunar.radiusSmall
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
                        color: closeSoftwareMouse.containsMouse ? lunar.danger : software.surfaceRaised
                        height: 34
                        radius: 17
                        width: 34

                        Text {
                            anchors.centerIn: parent
                            color: software.surfaceForeground
                            font.bold: true
                            font.pixelSize: 15
                            text: "×"
                        }

                        MouseArea {
                            id: closeSoftwareMouse
                            anchors.fill: parent
                            hoverEnabled: true
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
                height: visible ? 108 : 0
                radius: 8
                visible: !!software.applicationLauncher
                    && software.applicationLauncher.matchingApplications.length > 0
                width: parent.width
                clip: true

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    Text {
                        color: software.surfaceForeground
                        font.bold: true
                        font.pixelSize: 13
                        text: "Northstar application catalog"
                    }

                    ListView {
                        id: applicationList
                        height: 70
                        spacing: 8
                        width: parent.width
                        clip: true
                        orientation: ListView.Horizontal
                        model: software.applicationLauncher
                            ? software.applicationLauncher.matchingApplications : []

                        delegate: Rectangle {
                            required property var modelData

                            color: applicationMouse.containsMouse
                                ? software.surfaceAccent : software.surfaceBackground
                            height: 70
                            radius: 7
                            width: 220

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8

                                NorthstarIcon {
                                    anchors.verticalCenter: parent.verticalCenter
                                    height: 34
                                    width: 34
                                    iconName: modelData.sourceType === "bundle"
                                        ? "welcome" : "applications"
                                }

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2
                                    width: parent.width - 42

                                    Text {
                                        color: software.surfaceForeground
                                        elide: Text.ElideRight
                                        font.bold: true
                                        font.pixelSize: 12
                                        text: modelData.name
                                        width: parent.width
                                    }

                                    Text {
                                        color: software.surfaceMuted
                                        elide: Text.ElideRight
                                        font.pixelSize: 10
                                        text: modelData.version
                                            || modelData.genericName
                                            || "Desktop application"
                                        width: parent.width
                                    }
                                }
                            }

                            MouseArea {
                                id: applicationMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: software.showApplicationDetails(modelData)
                            }
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
                placeholderText: "Search packages and applications..."
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
                    if (software.applicationLauncher
                            && software.applicationLauncher.applicationQuery !== text) {
                        software.applicationLauncher.setApplicationQuery(text)
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
                    text: "Review Update Plan"
                    onClicked: {
                        packageDetailsDialog.close()
                        software.openUpdatePlan()
                    }
                }

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

    Dialog {
        id: updatePlanDialog
        title: "Update plan review"
        modal: true
        standardButtons: Dialog.Close
        width: Math.min(620, software.width - 48)
        x: (software.width - width) / 2
        y: (software.height - height) / 2

        background: Rectangle {
            color: software.surfaceBackground
            border.color: software.surfaceAccent
            border.width: 1
            radius: 10
        }

        contentItem: Column {
            spacing: 12
            width: updatePlanDialog.width - (2 * updatePlanDialog.padding)

            Text {
                color: software.surfaceForeground
                font.bold: true
                font.pixelSize: 18
                text: "Review only"
            }

            Rectangle {
                color: software.surfaceRaised
                radius: 8
                width: parent.width
                height: 66

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    Text {
                        color: "#70d6a6"
                        font.bold: true
                        font.pixelSize: 12
                        text: "No changes have been made"
                    }

                    Text {
                        color: software.surfaceMuted
                        font.pixelSize: 11
                        text: "This review reads trust and update state only. It does not invoke pkg, write repository configuration, or create a boot environment."
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Grid {
                columns: 2
                columnSpacing: 18
                rowSpacing: 7
                width: parent.width

                Text {
                    color: software.surfaceMuted
                    font.pixelSize: 11
                    text: "Repository"
                }

                Text {
                    color: software.surfaceForeground
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: software.packageTrust
                        ? software.packageTrust.trustStatus : "Trust state unavailable."
                    width: updatePlanDialog.width - 220
                }

                Text {
                    color: software.surfaceMuted
                    font.pixelSize: 11
                    text: "Publication"
                }

                Text {
                    color: software.surfaceForeground
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: software.updatePlan
                        ? software.updatePlan.metadataStatus : "Publication metadata unavailable."
                    width: updatePlanDialog.width - 220
                }

                Text {
                    color: software.surfaceMuted
                    font.pixelSize: 11
                    text: "Candidate plan"
                }

                Text {
                    color: software.surfaceForeground
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: software.updatePlan
                        ? software.updatePlan.planStatus : "Update preview unavailable."
                    width: updatePlanDialog.width - 220
                }

                Text {
                    color: software.surfaceMuted
                    font.pixelSize: 11
                    text: "Authorization"
                }

                Text {
                    color: software.surfaceForeground
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: software.updateAuthorization
                        ? software.updateAuthorization.status : "Authorization preflight unavailable."
                    width: updatePlanDialog.width - 220
                }
            }

            Text {
                color: software.surfaceMuted
                font.pixelSize: 11
                text: software.updatePlan ? software.updatePlan.planPreview : ""
                visible: text.length > 0
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Text {
                color: software.surfaceMuted
                font.pixelSize: 11
                text: software.updateAuthorization && software.updateAuthorization.plan.length > 0
                    ? software.updateAuthorization.plan
                    : "A privileged update and rollback workflow is not enabled on this system."
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Button {
                enabled: false
                text: "Apply Update (protected)"
            }
        }
    }

    Dialog {
        id: applicationDetailsDialog
        property var application

        title: applicationDetailsDialog.application
            ? applicationDetailsDialog.application.name : "Application details"
        modal: true
        standardButtons: Dialog.Close
        width: Math.min(560, software.width - 48)
        x: (software.width - width) / 2
        y: (software.height - height) / 2

        onOpened: applicationDetailsDialog.application = software.selectedApplication

        background: Rectangle {
            color: software.surfaceBackground
            border.color: software.surfaceAccent
            border.width: 1
            radius: 10
        }

        contentItem: Column {
            spacing: 12
            width: applicationDetailsDialog.width - (2 * applicationDetailsDialog.padding)

            Row {
                spacing: 12
                width: parent.width

                NorthstarIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 44
                    width: 44
                    iconName: applicationDetailsDialog.application
                        && applicationDetailsDialog.application.sourceType === "bundle"
                        ? "welcome" : "applications"
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    width: parent.width - 56

                    Text {
                        color: software.surfaceForeground
                        elide: Text.ElideRight
                        font.bold: true
                        font.pixelSize: 18
                        text: applicationDetailsDialog.application
                            ? applicationDetailsDialog.application.name : "Application details"
                        width: parent.width
                    }

                    Text {
                        color: software.surfaceMuted
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        text: applicationDetailsDialog.application
                            ? (applicationDetailsDialog.application.version
                                || applicationDetailsDialog.application.genericName
                                || "Northstar application") : ""
                        width: parent.width
                    }
                }
            }

            Text {
                color: software.surfaceForeground
                font.pixelSize: 13
                text: applicationDetailsDialog.application
                    ? (applicationDetailsDialog.application.comment
                        || applicationDetailsDialog.application.genericName
                        || "No application description provided.")
                    : ""
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Text {
                color: applicationDetailsDialog.application
                    && applicationDetailsDialog.application.launchable === false
                    ? "#c34f65" : "#55c58a"
                font.bold: true
                font.pixelSize: 12
                text: applicationDetailsDialog.application
                    && applicationDetailsDialog.application.launchable === false
                    ? "Unavailable: the declared executable is missing or unsafe."
                    : "Available through the Northstar application catalog."
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Text {
                color: software.surfaceMuted
                font.pixelSize: 11
                text: "Launching uses the same validated catalog and user permissions as Apps and Open With."
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Button {
                enabled: !!applicationDetailsDialog.application
                    && applicationDetailsDialog.application.launchable !== false
                text: "Launch"
                onClicked: {
                    if (software.applicationLauncher) {
                        software.applicationLauncher.launchApplication(
                            applicationDetailsDialog.application.desktopId)
                    }
                    applicationDetailsDialog.close()
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
        visible: false
        width: 18

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            onPressed: software.beginResize(mouse.x, mouse.y)
            onPositionChanged: software.updateResize(mouse.x, mouse.y)
        }
    }

    NativeWindowResizeHandler {
        resizingEnabled: !software.maximized
        window: software
    }
}
