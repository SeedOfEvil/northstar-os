import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: dock

    LunarPalette {
        id: lunar
        darkMode: shellState.darkMode
    }

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool
    height: 72
    width: 1280
    title: "Northstar Dock"

    property color dockBackground: lunar.dockGlass
    property color dockForeground: lunar.foreground
    property color dockMuted: lunar.muted
    property color dockAccent: lunar.accent
    property color dockButton: lunar.raised
    property var chooserGroup: null

    function applicationIconName(applicationId) {
        const descriptor = String(applicationId || "").toLowerCase()
        if (descriptor.indexOf("terminal") >= 0 || descriptor.indexOf("qterminal") >= 0
                || descriptor.indexOf("shell") >= 0 || descriptor.indexOf("console") >= 0) {
            return "terminal"
        }
        if (descriptor.indexOf("firefox") >= 0 || descriptor.indexOf("browser") >= 0) {
            return "browser"
        }
        if (descriptor.indexOf("file") >= 0 || descriptor.indexOf("manager") >= 0) {
            return "files"
        }
        if (descriptor.indexOf("setting") >= 0 || descriptor.indexOf("preference") >= 0) {
            return "settings"
        }
        if (descriptor.indexOf("software") >= 0 || descriptor.indexOf("package") >= 0) {
            return "software"
        }
        if (descriptor.indexOf("text") >= 0 || descriptor.indexOf("editor") >= 0
                || descriptor.indexOf("note") >= 0) {
            return "editor"
        }
        return "northstar"
    }

    function applicationIconSize(applicationId, defaultSize) {
        return defaultSize
    }

    function applicationIconVerticalOffset(applicationId) {
        const iconName = applicationIconName(applicationId)
        return iconName === "terminal" || iconName === "browser" || iconName === "files" ? 0 : 5
    }

    function groupFor(applicationId) {
        const groups = northstarWindowController ? northstarWindowController.applicationGroups : []
        for (let index = 0; index < groups.length; ++index) {
            if (groups[index].identity === applicationId
                    || dock.desktopIdForGroup(groups[index]) === applicationId) {
                return groups[index]
            }
        }
        return null
    }

    function unpinnedGroups() {
        const groups = northstarWindowController ? northstarWindowController.applicationGroups : []
        const result = []
        for (let index = 0; index < groups.length; ++index) {
            const desktopId = dock.desktopIdForGroup(groups[index])
            if (!desktopId || !pinnedApplicationModel.isPinned(desktopId)) {
                result.push(groups[index])
            }
        }
        return result
    }

    function desktopIdForGroup(group) {
        if (!group) {
            return ""
        }
        return launcher.desktopIdForWindow(group.appId || "", group.title || "")
    }

    function launchPinned(applicationId) {
        if (applicationId === "qterminal") {
            launcher.launchTerminal()
        } else if (applicationId === "firefox") {
            launcher.launchBrowser()
        } else {
            launcher.launchApplication(applicationId)
        }
        refreshTimer.restart()
    }

    function activateGroup(group) {
        if (!group || !group.windows || group.windows.length === 0) {
            return
        }
        if (group.windows.length === 1) {
            activateOrToggle(group.windows[0])
            return
        }
        chooserGroup = group
        windowChooser.open()
    }

    function activatePinned(applicationId) {
        const group = groupFor(applicationId)
        if (group) {
            activateGroup(group)
        } else {
            launchPinned(applicationId)
        }
    }

    function openDockMenu(desktopId, pinIndex, sourceItem) {
        const point = sourceItem.mapToItem(dock.contentItem, 0, 0)
        dockAppMenu.desktopId = desktopId || ""
        dockAppMenu.pinIndex = pinIndex
        // Read once as the menu opens rather than bound: the list is fixed
        // for as long as the menu is on screen.
        dockAppMenu.applicationActions = desktopId ? launcher.applicationActions(desktopId) : []
        dockAppMenu.x = Math.max(8, Math.min(
            dock.width - dockAppMenu.implicitWidth - 8,
            point.x))
        dockAppMenu.y = -dockAppMenu.implicitHeight + 4
        dockAppMenu.open()
    }

    function activateOrToggle(window) {
        if (!northstarWindowController || !window) {
            return
        }
        if (window.minimized || window.active) {
            northstarWindowController.toggleMinimize(window.viewId)
        } else {
            northstarWindowController.activateWindow(window.viewId)
        }
        refreshTimer.restart()
    }

    Timer {
        id: refreshTimer
        interval: 1200
        repeat: true
        running: true
        onTriggered: northstarWindowController.refresh()
    }

    Component.onCompleted: northstarWindowController.refresh()

    Rectangle {
        id: dockShadow
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        anchors.horizontalCenter: parent.horizontalCenter
        color: lunar.shadow
        height: 68
        radius: 22
        width: Math.min(parent.width - 20, dockSurface.width + 12)
        y: 8
    }

    Rectangle {
        id: dockSurface
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.horizontalCenter: parent.horizontalCenter
        color: dock.dockBackground
        height: 64
        radius: 20
        border.color: lunar.dockGlassEdge
        border.width: 1
        width: Math.min(parent.width - 24, Math.max(560, dockContent.implicitWidth + 20))

        gradient: Gradient {
            GradientStop { position: 0.0; color: lunar.dockGlass }
            GradientStop { position: 1.0; color: Qt.rgba(lunar.panel.r, lunar.panel.g, lunar.panel.b, 0.16) }
        }

        Row {
            id: dockContent
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            height: 56
            spacing: 8

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: logoMouse.containsMouse ? lunar.raisedHover : lunar.accentSoft
                height: 54
                radius: 17
                scale: logoMouse.containsMouse ? 1.12 : 1.0
                width: 54

                Behavior on scale { NumberAnimation { duration: 140 } }

                Image {
                    anchors.centerIn: parent
                    fillMode: Image.PreserveAspectFit
                    height: 42
                    mipmap: true
                    smooth: true
                    source: northstarLogoSource
                    sourceClipRect: Qt.rect(270, 245, 485, 335)
                    width: 42
                }

                MouseArea {
                    id: logoMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: launcher.refreshApplications()
                }

                ToolTip.visible: logoMouse.containsMouse
                ToolTip.text: "Northstar"
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: lunar.borderSoft
                height: 34
                opacity: 0.8
                width: 1
            }

            Repeater {
                model: pinnedApplicationModel

                delegate: Rectangle {
                    id: pinnedDelegate
                    required property int index
                    required property string desktopId
                    property real dragStartX: 0
                    readonly property var applicationGroup: dock.groupFor(desktopId)
                    readonly property bool running: applicationGroup !== null

                    anchors.verticalCenter: parent.verticalCenter
                    color: pinnedMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: 54
                    radius: 14
                    scale: pinnedMouse.containsMouse ? 1.12 : 1.0
                    width: 54

                    Behavior on scale { NumberAnimation { duration: 140 } }

                    NorthstarIcon {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: dock.applicationIconVerticalOffset(desktopId)
                        height: dock.applicationIconSize(desktopId, 42)
                        iconName: dock.applicationIconName(desktopId)
                        width: height
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: dock.dockAccent
                        height: 4
                        radius: 2
                        visible: parent.running
                        width: 18
                    }

                    MouseArea {
                        id: pinnedMouse
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                dock.openDockMenu(pinnedDelegate.desktopId,
                                                  pinnedDelegate.index,
                                                  pinnedDelegate)
                            } else {
                                dock.activatePinned(pinnedDelegate.desktopId)
                            }
                        }
                    }

                    DragHandler {
                        id: pinDrag
                        target: pinnedDelegate
                        xAxis.enabled: true
                        yAxis.enabled: false
                        onActiveChanged: {
                            if (active) {
                                pinnedDelegate.dragStartX = pinnedDelegate.x
                            } else {
                                const offset = Math.round(
                                    (pinnedDelegate.x - pinnedDelegate.dragStartX) / 62)
                                const destination = Math.max(0, Math.min(
                                    pinnedApplicationModel.count - 1,
                                    pinnedDelegate.index + offset))
                                pinnedDelegate.x = pinnedDelegate.dragStartX
                                if (destination !== pinnedDelegate.index) {
                                    pinnedApplicationModel.movePinned(pinnedDelegate.index, destination)
                                }
                            }
                        }
                    }

                    ToolTip.visible: pinnedMouse.containsMouse
                    ToolTip.text: desktopId === "qterminal" ? "Terminal"
                        : desktopId === "firefox" ? "Firefox" : desktopId
                }
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: filesMouse.containsMouse ? lunar.raisedHover : "transparent"
                height: 54
                radius: 14
                scale: filesMouse.containsMouse ? 1.12 : 1.0
                width: 54

                Behavior on scale { NumberAnimation { duration: 140 } }

                NorthstarIcon {
                    anchors.centerIn: parent
                    height: 42
                    iconName: "files"
                    width: 42
                }

                MouseArea {
                    id: filesMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: filesWindow.openBrowser()
                }

                ToolTip.visible: filesMouse.containsMouse
                ToolTip.text: "Files"
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: lunar.borderSoft
                height: 34
                opacity: 0.8
                width: 1
            }

            Flickable {
                anchors.verticalCenter: parent.verticalCenter
                clip: true
                contentWidth: runningRow.width
                height: 56
                interactive: runningRow.width > width
                width: {
                    const groups = dock.unpinnedGroups()
                    return groups.length === 0 ? 92 : Math.min(250, Math.max(56, groups.length * 54))
                }

                Row {
                    id: runningRow
                    height: parent.height
                    spacing: 6

                    Repeater {
                        model: dock.unpinnedGroups()

                        delegate: Rectangle {
                            required property var modelData

                            anchors.verticalCenter: parent.verticalCenter
                            color: modelData.active ? lunar.accentSoft
                                : runningMouse.containsMouse ? lunar.raisedHover : "transparent"
                            border.color: modelData.active ? lunar.accentBright : "transparent"
                            border.width: modelData.active ? 1 : 0
                            height: 54
                            radius: 14
                            scale: runningMouse.containsMouse ? 1.06 : 1.0
                            width: 54

                            Behavior on scale { NumberAnimation { duration: 140 } }

                            NorthstarIcon {
                                anchors.centerIn: parent
                                anchors.verticalCenterOffset: dock.applicationIconVerticalOffset(
                                    modelData.identity || modelData.title)
                                height: dock.applicationIconSize(
                                    modelData.identity || modelData.title, 40)
                                iconName: dock.applicationIconName(modelData.identity || modelData.title)
                                opacity: modelData.allMinimized ? 0.62 : 1.0
                                width: height
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 2
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: modelData.allMinimized ? lunar.muted : lunar.accentBright
                                height: 4
                                radius: 2
                                width: modelData.active ? 22 : 12
                            }

                            MouseArea {
                                id: runningMouse
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                hoverEnabled: true
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        dock.openDockMenu(dock.desktopIdForGroup(modelData),
                                                          -1,
                                                          parent)
                                    } else {
                                        dock.activateGroup(modelData)
                                    }
                                }
                            }

                            ToolTip.visible: runningMouse.containsMouse
                            ToolTip.text: modelData.title
                                + (modelData.count > 1 ? " (" + modelData.count + " windows)" : "")
                                + (modelData.allMinimized ? " (minimized)" : "")
                                + "\nRight-click for dock options"
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: dock.dockMuted
                        font.pixelSize: 11
                        text: northstarWindowController.applicationGroups.length === 0 ? "No open apps" : ""
                        visible: text.length > 0
                    }
                }
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: lunar.borderSoft
                height: 34
                opacity: 0.8
                width: 1
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: trashMouse.containsMouse ? "#664052" : "transparent"
                height: 54
                radius: 14
                scale: trashMouse.containsMouse ? 1.12 : 1.0
                width: 54

                Behavior on scale { NumberAnimation { duration: 140 } }

                NorthstarIcon {
                    anchors.centerIn: parent
                    height: 42
                    iconName: "trash"
                    width: 42
                }

                MouseArea {
                    id: trashMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: filesWindow.openTrash()
                }

                ToolTip.visible: trashMouse.containsMouse
                ToolTip.text: "Trash"
            }
        }
    }

    FileBrowserWindow {
        id: filesWindow
        fileBrowserController: northstarFileBrowserController
        applicationLauncher: launcher
        volumeController: northstarVolumeController
        state: shellState
        targetScreen: targetScreen
        panelHeight: 44
    }

    Menu {
        id: dockAppMenu
        parent: dock.contentItem
        property string desktopId: ""
        property int pinIndex: -1
        property var applicationActions: []

        // The application's own actions come first, the way a jump list does
        // on other desktops, with the dock's own commands below them.
        Instantiator {
            model: dockAppMenu.applicationActions

            delegate: MenuItem {
                required property var modelData
                text: modelData.name
                onTriggered: launcher.launchApplicationAction(dockAppMenu.desktopId, modelData.id)
            }

            onObjectAdded: (index, object) => dockAppMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => dockAppMenu.removeItem(object)
        }

        MenuSeparator { visible: dockAppMenu.applicationActions.length > 0 }

        MenuItem {
            enabled: dockAppMenu.desktopId.length > 0
            text: pinnedApplicationModel.isPinned(dockAppMenu.desktopId)
                ? "Unpin from Dock" : "Pin to Dock"
            onTriggered: {
                if (pinnedApplicationModel.isPinned(dockAppMenu.desktopId)) {
                    pinnedApplicationModel.unpin(dockAppMenu.desktopId)
                } else {
                    pinnedApplicationModel.pin(dockAppMenu.desktopId)
                }
            }
        }
        MenuSeparator { visible: dockAppMenu.pinIndex >= 0 }
        MenuItem {
            enabled: dockAppMenu.pinIndex > 0
            text: "Move left"
            visible: dockAppMenu.pinIndex >= 0
            onTriggered: pinnedApplicationModel.movePinned(dockAppMenu.pinIndex, dockAppMenu.pinIndex - 1)
        }
        MenuItem {
            enabled: dockAppMenu.pinIndex >= 0
                && dockAppMenu.pinIndex < pinnedApplicationModel.count - 1
            text: "Move right"
            visible: dockAppMenu.pinIndex >= 0
            onTriggered: pinnedApplicationModel.movePinned(dockAppMenu.pinIndex, dockAppMenu.pinIndex + 1)
        }
    }

    Popup {
        id: windowChooser
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        modal: false
        padding: 8
        width: 280

        background: Rectangle {
            color: lunar.panel
            border.color: lunar.border
            border.width: 1
            radius: 12
        }

        contentItem: Column {
            spacing: 4

            Repeater {
                model: dock.chooserGroup && dock.chooserGroup.windows
                    ? dock.chooserGroup.windows : []

                delegate: AuroraButton {
                    required property var modelData
                    flat: true
                    text: modelData.title + (modelData.minimized ? " (minimized)" : "")
                    width: 264
                    onClicked: {
                        windowChooser.close()
                        dock.activateOrToggle(modelData)
                    }
                }
            }
        }
    }
}
