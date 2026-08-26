import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: overview

    LunarPalette {
        id: lunar
        darkMode: overview.state ? overview.state.darkMode : true
    }

    property var applicationLauncher
    property var pinnedApplications
    property var state
    property var targetScreen
    property int panelHeight: 44
    property int desktopMargin: 24
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property int minimumSurfaceWidth: 720
    property int minimumSurfaceHeight: 460
    property bool maximized: visibility === Window.Maximized
    property color surfaceBackground: lunar.panelStrong
    property color surfaceForeground: lunar.foreground
    property color surfaceMuted: lunar.muted
    property color surfaceAccent: lunar.accent

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.NonModal
    title: "Northstar Applications"

    minimumWidth: minimumSurfaceWidth
    minimumHeight: minimumSurfaceHeight
    width: Math.min(900, screenWidth - 80)
    height: Math.min(590, screenHeight - panelHeight - 80)
    x: screenX + (screenWidth - width) / 2
    y: screenY + panelHeight + (screenHeight - panelHeight - height) / 2

    WindowDragController {
        id: overviewDrag
        window: overview
        screenX: overview.screenX
        screenY: overview.screenY
        screenWidth: overview.screenWidth
        screenHeight: overview.screenHeight
        topInset: overview.panelHeight
        bottomInset: 18
        defaultX: overview.screenX + (overview.screenWidth - overview.width) / 2
        defaultY: overview.screenY + overview.panelHeight
                  + (overview.screenHeight - overview.panelHeight - overview.height) / 2
    }

    onVisibleChanged: {
        if (visible) {
            overviewDrag.prepareForOpen()
            applicationLauncher.setApplicationQuery("")
            searchField.forceActiveFocus()
            requestActivate()
        } else {
            applicationLauncher.setApplicationQuery("")
        }
    }

    function openWithQuery(query) {
        show()
        raise()
        requestActivate()
        searchField.text = query || ""
        applicationLauncher.setApplicationQuery(searchField.text)
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    function applicationIconName(application) {
        if (!application) {
            return "northstar"
        }

        const descriptor = ((application.name || "") + " "
            + (application.genericName || "") + " "
            + (application.desktopId || "")).toLowerCase()
        if (descriptor.indexOf("terminal") >= 0 || descriptor.indexOf("shell") >= 0
                || descriptor.indexOf("console") >= 0) {
            return "terminal"
        }
        if (descriptor.indexOf("welcome") >= 0 || descriptor.indexOf("onboarding") >= 0) {
            return "welcome"
        }
        if (descriptor.indexOf("software") >= 0 || descriptor.indexOf("package") >= 0) {
            return "software"
        }
        if (descriptor.indexOf("firefox") >= 0 || descriptor.indexOf("browser") >= 0
                || descriptor.indexOf("web") >= 0) {
            return "browser"
        }
        if (descriptor.indexOf("setting") >= 0 || descriptor.indexOf("preference") >= 0
                || descriptor.indexOf("config") >= 0) {
            return "settings"
        }
        if (descriptor.indexOf("file") >= 0 || descriptor.indexOf("folder") >= 0
                || descriptor.indexOf("manager") >= 0) {
            return "files"
        }
        if (descriptor.indexOf("text") >= 0 || descriptor.indexOf("editor") >= 0
                || descriptor.indexOf("note") >= 0) {
            return "editor"
        }
        return "northstar"
    }

    function pinnedDisplayName(desktopId) {
        if (desktopId === "qterminal") {
            return "Terminal"
        }
        if (desktopId === "firefox") {
            return "Firefox"
        }
        const applications = applicationLauncher ? applicationLauncher.applications : []
        for (let index = 0; index < applications.length; ++index) {
            if (applications[index].desktopId === desktopId) {
                return applications[index].name
            }
        }
        return desktopId
    }

    function launchPinned(desktopId) {
        if (desktopId === "qterminal") {
            applicationLauncher.launchTerminal()
        } else if (desktopId === "firefox") {
            applicationLauncher.launchBrowser()
        } else {
            applicationLauncher.launchApplication(desktopId)
        }
        overview.hide()
    }

    function localPathFromDrop(drop) {
        if (!drop || !drop.urls || drop.urls.length === 0) {
            return ""
        }

        const url = drop.urls[0]
        if (url && typeof url.toLocalFile === "function") {
            return url.toLocalFile()
        }

        const text = String(url || "")
        return text.indexOf("file://") === 0 ? decodeURIComponent(text.slice(7)) : ""
    }

    NorthstarWindowFrame {
        anchors.fill: parent
        darkMode: overview.state ? overview.state.darkMode : true

        Column {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16

            Item {
                id: overviewTitleBar
                height: 46
                width: parent.width

                NorthstarWindowTitleBar {
                    anchors.fill: parent
                    lunarPalette: lunar
                    maximized: overview.maximized
                    subtitle: "Launch your Northstar applications"
                    title: "Applications"
                    window: overview
                    onMaximizeRequested: {
                        if (overview.maximized)
                            overview.showNormal()
                        else
                            overview.showMaximized()
                    }
                }
            }

            TextField {
                id: searchField
                background: Rectangle {
                    color: lunar.field
                    border.color: searchField.activeFocus ? lunar.accentBright : lunar.borderSoft
                    border.width: 1
                    radius: lunar.radiusMedium
                }
                color: overview.surfaceForeground
                height: 42
                width: parent.width
                leftPadding: 16
                placeholderText: "Search applications"
                placeholderTextColor: overview.surfaceMuted
                selectByMouse: true

                onTextChanged: applicationLauncher.setApplicationQuery(text)
                onAccepted: {
                    if (applicationLauncher.matchingApplications.length > 0) {
                        applicationLauncher.launchApplication(applicationLauncher.matchingApplications[0].desktopId)
                        overview.visible = false
                    }
                }
            }

            Rectangle {
                color: "transparent"
                border.color: lunar.borderSoft
                border.width: 1
                radius: lunar.radiusLarge
                height: parent.height - searchField.height - 126
                width: parent.width

                GridView {
                    id: applicationList
                    anchors.fill: parent
                    anchors.margins: 6
                    cellHeight: 124
                    cellWidth: 140
                    clip: true
                    model: applicationLauncher ? applicationLauncher.matchingApplications : []

                    delegate: Rectangle {
                        id: applicationDelegate
                        required property var modelData

                        color: applicationDropArea.containsDrag || applicationMouse.containsMouse
                            ? lunar.raisedHover : "transparent"
                        height: 114
                        radius: lunar.radiusMedium
                        width: 128
                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 5

                            Item {
                                anchors.horizontalCenter: parent.horizontalCenter
                                height: 56
                                width: 56

                                NorthstarIcon {
                                    anchors.fill: parent
                                    visible: modelData.sourceType !== "bundle" || !modelData.iconSource
                                    iconName: overview.applicationIconName(modelData)
                                }

                                Image {
                                    anchors.fill: parent
                                    asynchronous: true
                                    fillMode: Image.PreserveAspectFit
                                    mipmap: true
                                    smooth: true
                                    source: modelData.iconSource || ""
                                    visible: modelData.sourceType === "bundle" && !!modelData.iconSource
                                }
                            }

                            Text {
                                color: overview.surfaceForeground
                                elide: Text.ElideRight
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                text: modelData.name
                                width: parent.width
                            }

                            Text {
                                color: overview.surfaceMuted
                                elide: Text.ElideRight
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                text: modelData.genericName
                                    || (modelData.sourceType === "bundle"
                                        ? "Northstar .app"
                                        : (modelData.launchable === false ? "Unavailable" : "Desktop app"))
                                width: parent.width
                            }
                        }

                        MouseArea {
                            id: applicationMouse
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            hoverEnabled: true
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    applicationPinMenu.desktopId = modelData.desktopId
                                    applicationPinMenu.applicationName = modelData.name
                                    // The catalog already carries them, so
                                    // there is nothing further to ask for.
                                    applicationPinMenu.applicationActions =
                                        modelData.actions ? modelData.actions : []
                                    applicationPinMenu.x = Math.max(8, Math.min(
                                        overview.width - applicationPinMenu.implicitWidth - 8,
                                        applicationDelegate.mapToItem(overview.contentItem, 0, 0).x
                                            + mouse.x))
                                    applicationPinMenu.y = Math.max(8, Math.min(
                                        overview.height - applicationPinMenu.implicitHeight - 8,
                                        applicationDelegate.mapToItem(overview.contentItem, 0, 0).y
                                            + mouse.y))
                                    applicationPinMenu.open()
                                } else {
                                    applicationLauncher.launchApplication(modelData.desktopId)
                                    overview.visible = false
                                }
                            }
                        }

                        Rectangle {
                            anchors.right: parent.right
                            anchors.rightMargin: 7
                            anchors.top: parent.top
                            anchors.topMargin: 7
                            color: lunar.accent
                            height: 9
                            radius: 5
                            visible: overview.pinnedApplications
                                && overview.pinnedApplications.isPinned(modelData.desktopId)
                            width: 9
                        }

                        DropArea {
                            id: applicationDropArea
                            anchors.fill: parent
                            keys: ["text/uri-list"]

                            onDropped: function(drop) {
                                const filePath = overview.localPathFromDrop(drop)
                                if (!filePath) {
                                    return
                                }

                                if (applicationLauncher.launchApplicationWithFile(modelData.desktopId, filePath)) {
                                    drop.acceptProposedAction()
                                    overview.visible = false
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            color: overview.surfaceForeground
                            font.bold: true
                            font.pixelSize: 11
                            text: "Drop to open"
                            visible: applicationDropArea.containsDrag
                            z: 2
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }

                Text {
                    anchors.centerIn: parent
                    color: overview.surfaceMuted
                    font.pixelSize: 13
                    text: "No matching applications"
                    visible: applicationList.count === 0
                }
            }

            Row {
                height: 38
                spacing: 10
                width: parent.width

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: overview.surfaceMuted
                    font.bold: true
                    font.pixelSize: 11
                    text: "PINNED"
                    width: 58
                }

                Flickable {
                    clip: true
                    contentWidth: pinnedShortcutsRow.width
                    height: 38
                    interactive: contentWidth > width
                    width: Math.max(180, parent.width - 58 - 230 - (3 * parent.spacing))

                    Row {
                        id: pinnedShortcutsRow
                        height: parent.height
                        spacing: 10

                        Repeater {
                            model: overview.pinnedApplications
                                ? overview.pinnedApplications.desktopIds : []

                            delegate: Rectangle {
                                id: pinnedShortcut
                                required property string modelData

                                anchors.verticalCenter: parent.verticalCenter
                                color: shortcutMouse.containsMouse ? lunar.raisedHover : lunar.raised
                                height: 36
                                radius: 12
                                width: 112

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 7

                                    NorthstarIcon {
                                        height: 22
                                        iconName: overview.applicationIconName({ desktopId: pinnedShortcut.modelData })
                                        width: 22
                                    }
                                    Text {
                                        color: overview.surfaceForeground
                                        elide: Text.ElideRight
                                        font.pixelSize: 11
                                        text: overview.pinnedDisplayName(pinnedShortcut.modelData)
                                        width: 74
                                    }
                                }

                                MouseArea {
                                    id: shortcutMouse
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    hoverEnabled: true
                                    onClicked: function(mouse) {
                                        if (mouse.button === Qt.RightButton) {
                                            applicationPinMenu.desktopId = pinnedShortcut.modelData
                                            applicationPinMenu.applicationName = overview.pinnedDisplayName(
                                                pinnedShortcut.modelData)
                                            applicationPinMenu.open()
                                        } else {
                                            overview.launchPinned(pinnedShortcut.modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    ScrollBar.horizontal: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: overview.surfaceMuted
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignRight
                    text: "Tip: drag a file onto a compatible app"
                    width: 220
                }
            }
        }
    }

    NativeWindowResizeHandler {
        resizingEnabled: !overview.maximized
        window: overview
    }

    Menu {
        id: applicationPinMenu
        parent: overview.contentItem
        property string desktopId: ""
        property string applicationName: "Application"
        property var applicationActions: []

        // An application's own actions come first, matching the dock.
        Instantiator {
            model: applicationPinMenu.applicationActions

            delegate: MenuItem {
                required property var modelData
                text: modelData.name
                onTriggered: {
                    if (overview.applicationLauncher) {
                        overview.applicationLauncher.launchApplicationAction(
                            applicationPinMenu.desktopId, modelData.id)
                    }
                    overview.hide()
                }
            }

            onObjectAdded: (index, object) => applicationPinMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => applicationPinMenu.removeItem(object)
        }

        MenuSeparator { visible: applicationPinMenu.applicationActions.length > 0 }

        MenuItem {
            enabled: applicationPinMenu.desktopId.length > 0
            text: overview.pinnedApplications
                    && overview.pinnedApplications.isPinned(applicationPinMenu.desktopId)
                ? "Unpin from Dock" : "Pin to Dock"
            onTriggered: {
                if (!overview.pinnedApplications) {
                    return
                }
                if (overview.pinnedApplications.isPinned(applicationPinMenu.desktopId)) {
                    overview.pinnedApplications.unpin(applicationPinMenu.desktopId)
                } else {
                    overview.pinnedApplications.pin(applicationPinMenu.desktopId)
                }
            }
        }
    }
}
