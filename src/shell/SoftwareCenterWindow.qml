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
    property int lastTransactionResult: 0
    property string authorizationStatusText: !software.updateAuthorization
        ? "Update authorization is unavailable."
        : software.updateAuthorization.transactionStatus.length > 0
            ? software.updateAuthorization.transactionStatus
            : software.updateAuthorization.status
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

    Connections {
        target: software.updateAuthorization

        function onTransactionFinished(success) {
            software.lastTransactionResult = success ? 1 : -1
            if (success && software.packageCatalog) {
                software.packageCatalog.refresh()
            }
            if (software.updatePlan && software.packageCatalog) {
                software.updatePlan.reload()
                software.updatePlan.preview(software.packageCatalog.packages)
            }
            updatePlanDialog.open()
        }

        function onTransactionStateChanged() {
            if (software.updateAuthorization && software.updateAuthorization.transactionBusy) {
                software.lastTransactionResult = 0
            }
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
                id: updateSafetyCard
                color: software.surfaceRaised
                height: software.authorizationStatusText.indexOf("\n") >= 0
                    || software.lastTransactionResult !== 0 ? 142 : 72
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
                                    : software.updateAuthorization.transactionBusy
                                        ? "#f0b45a"
                                        : software.updateAuthorization.preflightValid
                                            && software.updateAuthorization.authorizationAvailable
                                            ? "#55c58a" : "#c34f65"
                                font.bold: true
                                font.pixelSize: 13
                                text: !software.updateAuthorization
                                    ? "Unavailable"
                                    : software.updateAuthorization.transactionBusy
                                        ? "Working"
                                        : software.updateAuthorization.authorizationAvailable
                                            && software.updateAuthorization.preflightValid
                                            ? "Ready"
                                            : software.updateAuthorization.preflightValid
                                                ? "Preflight only" : "Blocked"
                            }
                        }

                        ScrollView {
                            id: updateStatusView
                            clip: true
                            height: updateSafetyCard.height - 40
                            width: parent.width
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded

                            Text {
                                color: software.lastTransactionResult > 0
                                    ? "#2f8f65"
                                    : software.lastTransactionResult < 0
                                        ? "#c34f65" : software.surfaceMuted
                                font.pixelSize: 11
                                text: software.authorizationStatusText
                                width: updateStatusView.availableWidth
                                wrapMode: Text.WrapAnywhere
                            }
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

            Row {
                spacing: 8
                width: parent.width

                Repeater {
                    model: [
                        { value: "requested", label: "Installed by you" },
                        { value: "updatable", label: "Updates" },
                        { value: "all", label: "Everything" }
                    ]

                    delegate: Button {
                        required property var modelData

                        checkable: true
                        checked: software.packageCatalog
                            && software.packageCatalog.filter === modelData.value
                        text: modelData.value === "requested" && software.packageCatalog
                            ? modelData.label + " (" + software.packageCatalog.requestedCount + ")"
                            : modelData.value === "all" && software.packageCatalog
                                ? modelData.label + " (" + software.packageCatalog.installedCount + ")"
                                : modelData.value === "updatable" && software.packageCatalog
                                    && software.packageCatalog.updatesKnown
                                    ? modelData.label + " (" + software.packageCatalog.updatableCount + ")"
                                    : modelData.label
                        onClicked: {
                            if (software.packageCatalog) {
                                software.packageCatalog.filter = modelData.value
                            }
                        }
                    }
                }

                Button {
                    enabled: software.packageCatalog
                        && !software.packageCatalog.scanningUpdates
                    text: "Check for updates"
                    onClicked: software.packageCatalog.scanForUpdates()
                }
            }

            Text {
                color: software.surfaceMuted
                font.pixelSize: 11
                // Until the scan has run, the honest answer is that nothing
                // is known yet rather than that everything is current.
                text: !software.packageCatalog ? ""
                    : software.packageCatalog.scanningUpdates
                        ? software.packageCatalog.updateStatus
                        : software.packageCatalog.updatesKnown
                            ? software.packageCatalog.updateStatus
                            : "Updates have not been checked for yet."
                width: parent.width
                wrapMode: Text.WordWrap
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

                    // A list of hundreds with no scrollbar gives no sense of
                    // how far through it you are, or how much is left.
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AlwaysOn
                        visible: packageList.contentHeight > packageList.height
                    }

                    // Something has to be on screen when a filter matches
                    // nothing, or the panel reads as broken.
                    Text {
                        anchors.centerIn: parent
                        color: software.surfaceMuted
                        font.pixelSize: 12
                        visible: packageList.count === 0
                        width: parent.width - 40
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: !software.packageCatalog ? "No package inventory."
                            : software.packageCatalog.query.length > 0
                                ? "Nothing here matches that search."
                                : software.packageCatalog.filter === "updatable"
                                    ? (software.packageCatalog.updatesKnown
                                        ? "Everything is up to date."
                                        : "Check for updates to see what can be updated.")
                                    : "No packages to show."
                    }

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
                                        text: modelData.updatable && modelData.availableVersion
                                            ? modelData.version + "  \u2192  " + modelData.availableVersion
                                            : modelData.version
                                        width: parent.width - 280
                                    }

                                    Text {
                                        color: modelData.orphaned ? lunar.warning : software.surfaceAccent
                                        font.bold: true
                                        font.pixelSize: 10
                                        text: modelData.orphaned ? "NO LONGER PACKAGED"
                                            : modelData.updatable ? "UPDATE"
                                            : modelData.automatic ? "DEPENDENCY" : ""
                                        visible: text.length > 0
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
                text: software.lastTransactionResult > 0
                    ? "Update completed"
                    : software.lastTransactionResult < 0
                        ? "Update did not complete" : "Review only"
            }

            Rectangle {
                color: software.surfaceRaised
                radius: 8
                width: parent.width
                height: software.lastTransactionResult !== 0 ? 150 : 66

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    Text {
                        color: software.lastTransactionResult < 0 ? "#c34f65" : "#70d6a6"
                        font.bold: true
                        font.pixelSize: 12
                        text: software.lastTransactionResult > 0
                            ? "Protected transaction completed successfully"
                            : software.lastTransactionResult < 0
                                ? "Protected transaction requires attention"
                                : "No changes have been made yet"
                    }

                    ScrollView {
                        id: resultStatusView
                        clip: true
                        height: parent.height - 26
                        width: parent.width
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ScrollBar.vertical.policy: ScrollBar.AsNeeded

                        Text {
                            color: software.surfaceMuted
                            font.pixelSize: 11
                            text: software.lastTransactionResult !== 0
                                ? software.authorizationStatusText
                                : "Review is read-only. Applying requires a verified plan, a protected transaction service, explicit confirmation, and PolicyKit authorization."
                            width: resultStatusView.availableWidth
                            wrapMode: Text.WrapAnywhere
                        }
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
                    text: "Verified channel"
                }

                Text {
                    color: software.surfaceForeground
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: software.updatePlan && software.updatePlan.metadataValid
                        ? software.updatePlan.channel + " / " + software.updatePlan.repositoryTag
                            + " / r" + software.updatePlan.repositoryRevision
                        : "No verified development channel"
                    width: updatePlanDialog.width - 220
                }

                Text {
                    color: software.surfaceMuted
                    font.pixelSize: 11
                    text: "Manifest digest"
                }

                Text {
                    color: software.surfaceForeground
                    elide: Text.ElideMiddle
                    font.family: "monospace"
                    font.pixelSize: 10
                    text: software.updatePlan
                        ? software.updatePlan.publicationManifestSha256 : ""
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
                        ? (software.updateAuthorization.transactionStatus.length > 0
                            ? software.updateAuthorization.transactionStatus
                            : software.updateAuthorization.status)
                        : "Authorization preflight unavailable."
                    width: updatePlanDialog.width - 220
                }
            }

            Rectangle {
                color: software.surfaceRaised
                height: visible ? 74 : 0
                radius: 8
                visible: !!software.updatePlan
                    && software.updatePlan.packageProvenance.length > 0
                width: parent.width

                ListView {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    model: software.updatePlan ? software.updatePlan.packageProvenance : []
                    orientation: ListView.Horizontal
                    spacing: 8

                    delegate: Rectangle {
                        required property var modelData

                        color: software.surfaceBackground
                        border.color: software.surfaceAccent
                        border.width: 1
                        height: 58
                        radius: 7
                        width: 230

                        Column {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2

                            Text {
                                color: software.surfaceForeground
                                elide: Text.ElideRight
                                font.bold: true
                                font.pixelSize: 11
                                text: modelData.name + " " + modelData.version
                                width: parent.width
                            }

                            Text {
                                color: software.surfaceMuted
                                elide: Text.ElideMiddle
                                font.pixelSize: 10
                                text: modelData.origin + " · " + modelData.projectRevision
                                width: parent.width
                            }
                        }
                    }
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

            Row {
                spacing: 10

                Button {
                    enabled: !!software.updateAuthorization
                        && software.updateAuthorization.authorizationAvailable
                        && software.updateAuthorization.preflightValid
                        && !software.updateAuthorization.transactionBusy
                        && !!software.updatePlan
                        && (software.updatePlan.updateCount + software.updatePlan.installCount) > 0
                    text: software.updateAuthorization && software.updateAuthorization.transactionBusy
                        ? "Update in progress..." : "Apply Verified Update"
                    onClicked: updateConfirmationDialog.open()
                }

                Button {
                    enabled: !!software.updateAuthorization
                        && software.updateAuthorization.authorizationAvailable
                        && !software.updateAuthorization.transactionBusy
                    text: "Schedule Rollback"
                    onClicked: rollbackConfirmationDialog.open()
                }
            }
        }
    }

    Dialog {
        id: updateConfirmationDialog
        title: "Apply verified update?"
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        width: Math.min(500, software.width - 48)
        x: (software.width - width) / 2
        y: (software.height - height) / 2

        Text {
            color: software.surfaceForeground
            text: "Northstar will create '" + (software.updateAuthorization
                ? software.updateAuthorization.bootEnvironmentName : "")
                + "' before updating. PolicyKit administrator authorization is required."
            width: parent.width
            wrapMode: Text.WordWrap
        }

        onAccepted: {
            if (software.updateAuthorization) {
                software.updateAuthorization.applyUpdate()
            }
            updatePlanDialog.close()
        }
    }

    Dialog {
        id: rollbackConfirmationDialog
        title: "Schedule rollback?"
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        width: Math.min(500, software.width - 48)
        x: (software.width - width) / 2
        y: (software.height - height) / 2

        Text {
            color: software.surfaceForeground
            text: "The last verified pre-update boot environment will be activated for the next reboot. Home data is stored outside the root boot environment."
            width: parent.width
            wrapMode: Text.WordWrap
        }

        onAccepted: {
            if (software.updateAuthorization) {
                software.updateAuthorization.scheduleRollback()
            }
            updatePlanDialog.close()
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
