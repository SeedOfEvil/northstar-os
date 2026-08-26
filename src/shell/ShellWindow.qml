import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: root

    LunarPalette {
        id: lunar
        darkMode: shellState.darkMode
    }

    AuroraMetrics { id: metrics }

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool
    height: metrics.panelHeight
    width: 1280
    title: "Northstar Shell"

    property color panelBackground: lunar.panelStrong
    property color panelForeground: lunar.foreground
    property color panelMuted: lunar.muted
    property color panelAccent: lunar.accent
    property date now: new Date()
    property string activeApplicationKind: "desktop"
    property string activeApplicationName: "Desktop"
    property int activeApplicationViewId: -1

    function menuItem(label, action, shortcut, enabled, checked) {
        return {
            kind: "action",
            label: label,
            action: action || "",
            shortcut: shortcut || "",
            enabled: enabled === undefined ? true : enabled,
            checked: checked === true
        }
    }

    function menuSeparator() {
        return { kind: "separator" }
    }

    function rememberApplication(kind, name, viewId) {
        activeApplicationKind = kind || "external"
        activeApplicationName = name && name.length > 0 ? name : "Application"
        activeApplicationViewId = viewId === undefined ? -1 : viewId
    }

    function activeShellSurface() {
        if (activeApplicationKind === "files") return fileBrowserWindow
        if (activeApplicationKind === "settings") return settingsWindow
        if (activeApplicationKind === "software") return softwareCenterWindow
        if (activeApplicationKind === "applications") return applicationOverview
        if (activeApplicationKind === "quicklook") return quickLookWindow
        return null
    }

    function refreshActiveApplication() {
        if (!northstarWindowController)
            return
        const windows = northstarWindowController.windows || []
        for (let index = 0; index < windows.length; ++index) {
            const window = windows[index]
            if (!window.active)
                continue
            const title = String(window.title || window.appId || "Application")
            const descriptor = (String(window.appId || "") + " " + title).toLowerCase()
            if (descriptor.indexOf("northstar files") >= 0)
                rememberApplication("files", "Files", window.viewId)
            else if (descriptor.indexOf("settings") >= 0)
                rememberApplication("settings", "Settings", window.viewId)
            else if (descriptor.indexOf("software") >= 0)
                rememberApplication("software", "Software", window.viewId)
            else if (descriptor.indexOf("applications") >= 0)
                rememberApplication("applications", "Applications", window.viewId)
            else if (descriptor.indexOf("quick look") >= 0)
                rememberApplication("quicklook", "Quick Look", window.viewId)
            else
                rememberApplication("external", title, window.viewId)
            return
        }
        // Clicking the panel or an open menu temporarily leaves every client
        // view unfocused, so retain the last application while any view still
        // exists. Once the compositor reports no application views at all,
        // the desktop is genuinely the active context and a stale app name is
        // misleading.
        if (windows.length === 0)
            rememberApplication("desktop", "Desktop", -1)
    }

    function unavailableExternalMenu() {
        return [menuItem("This app does not expose a global menu", "", "", false)]
    }

    function menuItemsFor(menuName) {
        const kind = activeApplicationKind
        const filesActive = kind === "files"
        const controller = fileBrowserWindow.fileBrowserController
        if (menuName === "Application") {
            return [
                menuItem("About Northstar", "help.about"),
                menuItem("Settings…", "settings.open", "Ctrl+,"),
                menuSeparator(),
                menuItem("Close " + activeApplicationName, "window.close", "Ctrl+W",
                         kind !== "desktop" && (kind !== "external" || activeApplicationViewId >= 0))
            ]
        }
        if (menuName === "File") {
            if (filesActive) {
                return [
                    menuItem("New Tab", "files.new-tab", "Ctrl+T"),
                    menuItem("Open Selected Item", "files.open-selected", "Enter",
                             fileBrowserWindow.hasSelection),
                    menuSeparator(),
                    menuItem("Close Tab", "files.close-tab", "Ctrl+W",
                             fileBrowserWindow.activeTabIndex >= 0),
                    menuItem("Close Window", "window.close", "Ctrl+Shift+W")
                ]
            }
            if (kind === "external") return unavailableExternalMenu()
            return [
                menuItem("New Files Window", "files.open", "Ctrl+N"),
                menuItem("Open Files", "files.open", "Ctrl+O"),
                menuSeparator(),
                menuItem("Close Window", "window.close", "Ctrl+W", kind !== "desktop")
            ]
        }
        if (menuName === "Edit") {
            if (!filesActive)
                return kind === "external" ? unavailableExternalMenu()
                    : [menuItem("No editable selection", "", "", false)]
            return [
                menuItem(controller && controller.undoLabel.length > 0
                         ? "Undo " + controller.undoLabel : "Undo",
                         "files.undo", "Ctrl+Z", controller && controller.canUndo),
                menuSeparator(),
                menuItem("Copy", "files.copy", "Ctrl+C", fileBrowserWindow.hasSelection),
                menuItem("Cut", "files.cut", "Ctrl+X", fileBrowserWindow.hasSelection
                         && !fileBrowserWindow.showingTrash && controller && controller.homeLocation),
                menuItem("Paste", "files.paste", "Ctrl+V", controller && controller.canPaste)
            ]
        }
        if (menuName === "View") {
            if (filesActive) {
                return [
                    menuItem("as Icons", "files.grid", "", true, fileBrowserWindow.gridView),
                    menuItem("as List", "files.list", "", true, !fileBrowserWindow.gridView),
                    menuSeparator(),
                    menuItem("Quick Look", "files.quick-look", "Space", fileBrowserWindow.hasSelection),
                    menuItem("Refresh", "files.refresh", "Ctrl+R")
                ]
            }
            if (kind === "settings") {
                return [
                    menuItem("Appearance", "settings.appearance", "", true),
                    menuItem("Network", "settings.network", "", true),
                    menuItem("Power", "settings.power", "", true),
                    menuSeparator(),
                    menuItem("Quick Settings", "quick-settings.open")
                ]
            }
            if (kind === "external") return unavailableExternalMenu()
            return [
                menuItem("Applications", "applications.open"),
                menuItem("Quick Settings", "quick-settings.open"),
                menuItem("Notifications", "notifications.open")
            ]
        }
        if (menuName === "Window") {
            const shellSurface = activeShellSurface()
            return [
                menuItem("Minimize", "window.minimize", "Ctrl+M",
                         shellSurface !== null || (kind === "external" && activeApplicationViewId >= 0)),
                menuItem("Zoom", "window.zoom", "", shellSurface !== null
                         && shellSurface.toggleMaximize !== undefined),
                menuSeparator(),
                menuItem("Show All Applications", "applications.open")
            ]
        }
        return [
            menuItem("Northstar Help", "help.welcome"),
            menuItem("Keyboard Shortcuts", "help.shortcuts"),
            menuSeparator(),
            menuItem("About Northstar", "help.about")
        ]
    }

    function openContextMenu(menuName, anchorItem) {
        if (systemMenu.visible) systemMenu.closeMenu()
        const position = anchorItem.mapToItem(root.contentItem, 0, 0)
        menuBarPopup.openMenu(menuName, menuItemsFor(menuName), root.x + position.x)
    }

    function triggerMenuAction(action) {
        const controller = fileBrowserWindow.fileBrowserController
        const surface = activeShellSurface()
        if (action === "settings.open") settingsWindow.openSettings()
        else if (action === "help.about") {
            settingsWindow.openSettings(); settingsWindow.selectSection("about")
        } else if (action === "help.shortcuts") {
            settingsWindow.openSettings(); settingsWindow.selectSection("session")
        } else if (action === "help.welcome") launcher.launchApplication("bundle:org.northstar.Welcome")
        else if (action === "files.open") fileBrowserWindow.openBrowser()
        else if (action === "files.new-tab") fileBrowserWindow.newTab()
        else if (action === "files.close-tab") fileBrowserWindow.closeTab(fileBrowserWindow.activeTabIndex)
        else if (action === "files.open-selected") fileBrowserWindow.openSelectedEntry()
        else if (action === "files.copy") controller.copyEntry(fileBrowserWindow.selectedPath)
        else if (action === "files.cut") controller.cutEntry(fileBrowserWindow.selectedPath)
        else if (action === "files.paste") controller.pasteClipboard()
        else if (action === "files.undo") controller.undoLastTransfer()
        else if (action === "files.grid") fileBrowserWindow.setGridView(true)
        else if (action === "files.list") fileBrowserWindow.setGridView(false)
        else if (action === "files.quick-look") fileBrowserWindow.previewSelectedEntry()
        else if (action === "files.refresh") controller.refresh()
        else if (action === "settings.appearance") settingsWindow.selectSection("appearance")
        else if (action === "settings.network") settingsWindow.selectSection("network")
        else if (action === "settings.power") settingsWindow.selectSection("power")
        else if (action === "quick-settings.open") quickSettingsWindow.togglePanel()
        else if (action === "notifications.open") notificationCenterWindow.togglePanel()
        else if (action === "applications.open") applicationOverview.openWithQuery("")
        else if (action === "window.close") {
            if (surface) surface.hide()
            else if (activeApplicationViewId >= 0)
                northstarWindowController.closeWindow(activeApplicationViewId)
        }
        else if (action === "window.minimize") {
            if (surface) surface.showMinimized()
            else if (activeApplicationViewId >= 0)
                northstarWindowController.toggleMinimize(activeApplicationViewId)
        } else if (action === "window.zoom" && surface && surface.toggleMaximize)
            surface.toggleMaximize()
    }

    Connections {
        target: northstarWindowController
        function onWindowsChanged() { root.refreshActiveApplication() }
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: root.now = new Date()
    }

    Timer {
        id: launchNotificationTimer
        interval: 3500
        onTriggered: launcher.clearLaunchMessage()
    }

    Connections {
        target: launcher

        function onLaunchStatusChanged() {
            if (launcher.launchMessage.length > 0) {
                launchNotificationTimer.restart()
            }
        }
    }

    Connections {
        target: northstarDesktopItemsController

        function onOpenPathRequested(path, isDirectory, isLaunchable) {
            fileBrowserWindow.openPath(path, isDirectory, isLaunchable)
        }

        function onOpenWithRequested(path) {
            fileBrowserWindow.openAssociationForPath(path)
        }
    }

    // The desktop background and dock are keyboard-inert and the panel is
    // KeyboardInteractivityOnDemand, so the panel is the only shell surface that
    // can hold keyboard focus. A transient surface such as unified search is a
    // normal toplevel: when it closes, the compositor has nothing to hand focus
    // back to, and every Qt.ApplicationShortcut stops receiving keys until the
    // user clicks the panel. Ask for focus back explicitly instead.
    // Global shortcuts are bound in the compositor and delivered over the
    // shell control socket, because a Qt application shortcut only fires while
    // a shell window holds keyboard focus.
    function runShellCommand(command) {
        if (displayIndex !== 0) {
            return
        }
        if (command === "open-search") {
            searchOverlay.openSearch("")
        } else if (command === "toggle-search") {
            if (searchOverlay.visible) {
                searchOverlay.closeSearch()
            } else {
                searchOverlay.openSearch("")
            }
        }
    }

    Connections {
        target: typeof northstarShellCommands !== "undefined" ? northstarShellCommands : null

        function onCommandReceived(command) {
            root.runShellCommand(command)
        }
    }

    function restoreShellFocus() {
        if (displayIndex !== 0) {
            return
        }
        // requestActivate() alone is ignored for an on-demand layer surface,
        // so the panel asks the compositor for focus through layer-shell.
        if (typeof northstarShellFocus !== "undefined" && northstarShellFocus) {
            northstarShellFocus.restore()
            return
        }
        root.requestActivate()
    }

    function closeTransientSurfaces() {
        if (menuBarPopup.visible) {
            menuBarPopup.closeMenu()
        } else if (searchOverlay.visible) {
            searchOverlay.closeSearch()
        } else if (systemMenu.visible) {
            systemMenu.closeMenu()
        } else if (applicationOverview.visible) {
            applicationOverview.hide()
        } else if (quickSettingsWindow.visible) {
            quickSettingsWindow.hide()
        } else if (notificationCenterWindow.visible) {
            notificationCenterWindow.hide()
        } else if (quickLookWindow.visible) {
            quickLookWindow.hide()
        } else if (softwareCenterWindow.visible) {
            softwareCenterWindow.hide()
        } else if (fileBrowserWindow.visible) {
            fileBrowserWindow.hide()
        } else if (settingsWindow.visible) {
            settingsWindow.hide()
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0
        sequence: "Ctrl+K"
        onActivated: searchOverlay.openSearch("")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("applications") : ""
        onActivated: systemMenu.triggerAction("applications")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("files") : ""
        onActivated: systemMenu.triggerAction("files")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("software") : ""
        onActivated: systemMenu.triggerAction("software")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("settings") : ""
        onActivated: systemMenu.triggerAction("settings")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("terminal") : ""
        onActivated: systemMenu.triggerAction("terminal")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("browser") : ""
        onActivated: systemMenu.triggerAction("browser")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: displayIndex === 0 && !!northstarShortcutCatalog
        sequence: northstarShortcutCatalog
            ? northstarShortcutCatalog.sequenceFor("refresh") : ""
        onActivated: systemMenu.triggerAction("refresh")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: searchOverlay.visible || systemMenu.visible || applicationOverview.visible || quickSettingsWindow.visible
            || notificationCenterWindow.visible || softwareCenterWindow.visible
            || fileBrowserWindow.visible || settingsWindow.visible
        sequence: "Escape"
        onActivated: root.closeTransientSurfaces()
    }

    Rectangle {
        anchors.fill: parent
        color: root.panelBackground
        border.color: lunar.borderSoft
        border.width: 1
        radius: 18

        gradient: Gradient {
            GradientStop { position: 0.0; color: lunar.panelStrong }
            GradientStop { position: 1.0; color: lunar.panel }
        }

        Rectangle {
            id: topBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: metrics.panelHeight
            color: "transparent"

            Row {
                id: brandNavigation
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 9

                Rectangle {
                    color: systemMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: 38
                    radius: 11
                    width: 42

                    Image {
                        anchors.centerIn: parent
                        fillMode: Image.PreserveAspectFit
                        height: metrics.panelLogoHeight
                        mipmap: true
                        smooth: true
                        source: northstarLogoSource
                        sourceClipRect: Qt.rect(270, 245, 485, 335)
                        width: metrics.panelLogoHeight + 4
                    }

                    MouseArea {
                        id: systemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            menuBarPopup.closeMenu()
                            systemMenu.openMenu()
                        }
                    }
                }

                Repeater {
                    model: [
                        { label: root.activeApplicationName, menu: "Application", emphasized: true },
                        { label: "File", menu: "File" },
                        { label: "Edit", menu: "Edit" },
                        { label: "View", menu: "View" },
                        { label: "Window", menu: "Window" },
                        { label: "Help", menu: "Help" }
                    ]

                    delegate: Rectangle {
                        id: menuSelector
                        required property var modelData
                        color: menuBarPopup.visible && menuBarPopup.menuName === modelData.menu
                            ? lunar.accentSoft
                            : menuMouse.containsMouse ? lunar.raisedHover : "transparent"
                        height: 34
                        radius: 9
                        width: menuLabel.implicitWidth + 20

                        Text {
                            id: menuLabel
                            anchors.centerIn: parent
                            color: root.panelForeground
                            font.bold: modelData.emphasized === true
                            font.pixelSize: 14
                            text: modelData.label
                        }

                        MouseArea {
                            id: menuMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: if (menuBarPopup.visible)
                                root.openContextMenu(modelData.menu, menuSelector)
                            onClicked: root.openContextMenu(modelData.menu, menuSelector)
                        }
                    }
                }

                Rectangle {
                    visible: false
                    color: "transparent"
                    height: 32
                    width: 56

                    Text {
                        anchors.centerIn: parent
                        color: root.panelMuted
                        font.pixelSize: 11
                        text: "Desktop"
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: systemMenu.closeMenu()
                    }
                }

                Rectangle {
                    visible: false
                    color: filesNavMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: 32
                    radius: 9
                    width: 58

                    Row {
                        anchors.centerIn: parent
                        spacing: 3

                        NorthstarIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            height: 20
                            width: 20
                            iconName: "files"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: root.panelMuted
                            font.pixelSize: 11
                            text: "Files"
                        }
                    }

                    MouseArea {
                        id: filesNavMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: fileBrowserWindow.openBrowser()
                    }
                }

                Rectangle {
                    visible: false
                    color: appsNavMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: 32
                    radius: 9
                    width: 58

                    Row {
                        anchors.centerIn: parent
                        spacing: 3

                        NorthstarIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            height: 20
                            width: 20
                            iconName: "applications"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: root.panelMuted
                            font.pixelSize: 11
                            text: "Apps"
                        }
                    }

                    MouseArea {
                        id: appsNavMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: applicationOverview.openWithQuery("")
                    }
                }
            }

            Rectangle {
                id: globalSearchSurface
                visible: false
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                color: lunar.field
                border.color: globalSearchMouse.containsMouse ? lunar.accent : lunar.borderSoft
                border.width: 1
                height: 32
                radius: 12
                width: Math.min(360, root.width - 560)

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 34
                    anchors.right: parent.right
                    anchors.rightMargin: 58
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.panelMuted
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: "Search apps, files, and actions"
                }

                NorthstarIcon {
                    anchors.left: parent.left
                    anchors.leftMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                    height: 18
                    iconName: "search"
                    width: 18
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.panelMuted
                    font.pixelSize: 10
                    text: "⌘K"
                }

                MouseArea {
                    id: globalSearchMouse
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: searchOverlay.openSearch("")
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 7

                Repeater {
                    model: ["brightness", "wifi", "sound"]

                    delegate: Rectangle {
                        required property string modelData
                        color: statusMouse.containsMouse ? lunar.raisedHover : "transparent"
                        height: 34
                        radius: 9
                        width: 34

                        NorthstarSystemIcon {
                            anchors.centerIn: parent
                            darkMode: shellState.darkMode
                            height: metrics.panelIconSize
                            iconName: modelData
                            accented: false
                            width: metrics.panelIconSize
                        }

                        MouseArea {
                            id: statusMouse
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: quickSettingsWindow.togglePanel()
                        }
                    }
                }

                Row {
                    id: batteryIndicator
                    readonly property bool low: northstarPowerController.batteryAvailable
                        && !northstarPowerController.onAcPower
                        && northstarPowerController.batteryPercentage <= 15

                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: 2
                    height: 18
                    spacing: 4
                    visible: northstarPowerController.batteryAvailable

                    Item {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 14
                        width: 22

                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            border.color: batteryIndicator.low ? lunar.danger : root.panelForeground
                            border.width: 1
                            color: "transparent"
                            height: 12
                            radius: 2
                            width: 18

                            Rectangle {
                                anchors.left: parent.left
                                anchors.leftMargin: 2
                                anchors.verticalCenter: parent.verticalCenter
                                color: batteryIndicator.low ? lunar.danger
                                    : northstarPowerController.batteryCharging
                                    ? lunar.accentBright : root.panelForeground
                                height: 8
                                radius: 1
                                width: Math.max(1, 14 * northstarPowerController.batteryPercentage / 100)
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.leftMargin: 18
                            anchors.verticalCenter: parent.verticalCenter
                            color: batteryIndicator.low ? lunar.danger : root.panelForeground
                            height: 6
                            radius: 1
                            width: 3
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: batteryIndicator.low ? lunar.danger : root.panelForeground
                        font.pixelSize: 12
                        text: northstarPowerController.batteryPercentage + "%"
                    }
                }

                Rectangle {
                    visible: false
                    color: notificationMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: 30
                    radius: 9
                    width: 34

                    NorthstarIcon {
                        anchors.centerIn: parent
                        height: 22
                        width: 22
                        iconName: "notifications"
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                    color: lunar.danger
                        height: 12
                        radius: 6
                        visible: northstarNotificationCenter && northstarNotificationCenter.unreadCount > 0
                        width: 12
                    }

                    MouseArea {
                        id: notificationMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: notificationCenterWindow.togglePanel()
                    }
                }

                Rectangle {
                    visible: false
                    color: quickSettingsMouse.containsMouse ? lunar.raisedHover : "transparent"
                    height: 30
                    radius: 9
                    width: 34

                    NorthstarIcon {
                        anchors.centerIn: parent
                        height: 22
                        width: 22
                        iconName: "quick-settings"
                    }

                    MouseArea {
                        id: quickSettingsMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: quickSettingsWindow.togglePanel()
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: 2
                    color: root.panelForeground
                    font.pixelSize: 12
                    text: Qt.formatDateTime(root.now, "ddd d MMM  hh:mm")
                }
            }
        }

    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 5
        color: launcher.lastLaunchSucceeded ? lunar.accentSoft : lunar.danger
        height: 34
        radius: 12
        visible: launcher.launchMessage.length > 0
        width: Math.min(360, root.width - 40)
        z: 20

        Text {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            color: root.panelForeground
            elide: Text.ElideRight
            font.pixelSize: 12
            text: launcher.launchMessage
            verticalAlignment: Text.AlignVCenter
        }
    }

    MenuBarPopup {
        id: menuBarPopup
        darkMode: shellState.darkMode
        ownerWindow: root
        panelHeight: root.height
        screenX: targetScreen ? targetScreen.geometry.x : 0
        screenY: targetScreen ? targetScreen.geometry.y : 0
        screenWidth: targetScreen ? targetScreen.geometry.width : root.width
        onActionRequested: function(action) { root.triggerMenuAction(action) }
        onDismissed: root.restoreShellFocus()
    }

    SystemMenu {
        id: systemMenu
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        launcherController: launcher
        powerController: northstarPowerController
        overviewWindow: applicationOverview
        settingsSurface: settingsWindow
        filesWindow: fileBrowserWindow
        softwareWindow: softwareCenterWindow
        sessionController: northstarSessionController
        shortcutCatalog: northstarShortcutCatalog
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    Connections {
        target: northstarSearchController
        enabled: displayIndex === 0

        function onActionRequested(actionId) {
            if (actionId === "applications") {
                applicationOverview.openWithQuery("")
            } else if (actionId === "files") {
                fileBrowserWindow.openBrowser()
            } else if (actionId === "settings") {
                settingsWindow.openSettings()
            } else if (actionId === "software") {
                softwareCenterWindow.openSoftware()
            } else if (actionId === "terminal") {
                launcher.launchTerminal()
            } else if (actionId === "browser") {
                launcher.launchBrowser()
            }
        }

        function onApplicationRequested(desktopId) {
            launcher.launchApplication(desktopId)
        }

        function onFileRequested(path, isDirectory) {
            fileBrowserWindow.openPath(path, isDirectory, false)
        }
    }

    SearchOverlay {
        id: searchOverlay
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        controller: northstarSearchController
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    ApplicationOverview {
        id: applicationOverview
        onActiveChanged: if (active) root.rememberApplication("applications", "Applications", -1)
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        applicationLauncher: launcher
        pinnedApplications: northstarPinnedApplicationModel
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
        surfaceBackground: root.panelBackground
        surfaceForeground: root.panelForeground
        surfaceMuted: root.panelMuted
        surfaceAccent: root.panelAccent
    }

    QuickSettings {
        id: quickSettingsWindow
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        controller: northstarQuickSettingsController
        powerController: northstarPowerController
        state: shellState
        settingsWindow: settingsWindow
        systemMenu: systemMenu
        targetScreen: targetScreen
        panelHeight: root.height
    }

    NotificationCenterWindow {
        id: notificationCenterWindow
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        center: northstarNotificationCenter
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    SoftwareCenterWindow {
        id: softwareCenterWindow
        onActiveChanged: if (active) root.rememberApplication("software", "Software", -1)
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        packageCatalog: northstarPackageCatalog
        packageMutation: northstarPackageMutationController
        applicationLauncher: launcher
        packageTrust: northstarPackageTrustController
        updatePlan: northstarUpdatePlanController
        updateAuthorization: northstarUpdateAuthorizationController
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    FileBrowserWindow {
        id: fileBrowserWindow
        onActiveChanged: if (active) root.rememberApplication("files", "Files", -1)
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        fileBrowserController: northstarFileBrowserController
        applicationLauncher: launcher
        volumeController: northstarVolumeController
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
        previewWindow: quickLookWindow
    }

    QuickLookWindow {
        id: quickLookWindow
        onActiveChanged: if (active) root.rememberApplication("quicklook", "Quick Look", -1)
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        previewController: northstarPreviewController
        state: shellState
        targetScreen: targetScreen
        panelHeight: root.height
    }

    SettingsWindow {
        id: settingsWindow
        onActiveChanged: if (active) root.rememberApplication("settings", "Settings", -1)
        onVisibleChanged: if (!visible) root.restoreShellFocus()
        state: shellState
        desktopLayoutController: northstarDesktopLayoutController
        launcherController: launcher
        sessionController: northstarSessionController
        settingsCatalog: northstarSettingsCatalog
        wallpaperController: northstarWallpaperController
        targetScreen: targetScreen
        panelHeight: root.height
    }

}
