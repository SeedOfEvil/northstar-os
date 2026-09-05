import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: files
    objectName: "fileBrowserWindow"

    LunarPalette {
        id: lunar
        darkMode: files.state ? files.state.darkMode : true
    }

    property var fileBrowserController
    property var applicationLauncher
    property var volumeController
    property var previewWindow
    property var state
    property var targetScreen
    property int panelHeight: 44
    property int desktopMargin: 24
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property int minimumSurfaceWidth: files.sidebarVisible ? 900 : 620
    property int minimumSurfaceHeight: 460
    property bool dragging: false
    // Read from the compositor rather than tracked here. A shell-held
    // copy can disagree with the real state, and on Wayland the
    // compositor is the only thing that knows.
    property bool maximized: visibility === Window.Maximized
    property point dragOrigin: Qt.point(0, 0)
    property point windowOrigin: Qt.point(0, 0)
    property point resizeOrigin: Qt.point(0, 0)
    property size resizeSize: Qt.size(0, 0)
    property color surfaceBackground: lunar.panelStrong
    property color surfaceForeground: lunar.foreground
    property color surfaceMuted: lunar.muted
    property color surfaceAccent: lunar.accent
    property color surfaceRaised: lunar.raised
    property bool gridView: files.state ? files.state.filesGridView : true
    property bool sidebarVisible: files.screenWidth >= 1000
    property int sidebarWidth: 194
    property int sidebarRefreshToken: 0
    property int selectedIndex: -1
    property var tabs: []
    property int activeTabIndex: -1
    property bool switchingTabs: false
    property var selectedEntry: files.fileBrowserController
        && files.selectedIndex >= 0
        && files.selectedIndex < files.fileBrowserController.entries.length
        ? files.fileBrowserController.entries[files.selectedIndex] : null
    property string selectedPath: files.selectedEntry ? files.selectedEntry.path : ""
    property string selectedName: files.selectedEntry ? files.selectedEntry.name : ""
    property bool hasSelection: !!files.selectedEntry && files.selectedPath.length > 0
    property bool showingTrash: files.fileBrowserController && files.fileBrowserController.showingTrash
    property var sortModes: ["name", "type", "size", "modified"]
    property var sidebarFavorites: [
        { label: "Home", iconName: "desktop", kind: "home", relativePath: "" },
        { label: "Desktop", iconName: "desktop", kind: "folder", relativePath: "Desktop" },
        { label: "Documents", iconName: "files", kind: "folder", relativePath: "Documents" },
        { label: "Downloads", iconName: "files", kind: "folder", relativePath: "Downloads" },
        { label: "Trash", iconName: "trash", kind: "trash", relativePath: "" }
    ]

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.NonModal
    title: "Northstar Files"

    Connections {
        target: files.fileBrowserController

        function onSearchQueryChanged() {
            if (searchField.text !== files.fileBrowserController.searchQuery) {
                searchField.text = files.fileBrowserController.searchQuery
            }
        }

        function onEntriesChanged() {
            files.sidebarRefreshToken++
        }

        function onCurrentPathChanged() {
            files.syncActiveTab()
            files.syncLocationField()
        }

        function onLocationChanged() {
            files.syncActiveTab()
            files.syncLocationField()
        }

        function onConflictChanged() {
            if (files.fileBrowserController.conflictPending) {
                pasteConflictDialog.open()
            }
        }
    }

    Timer {
        id: searchDebounceTimer
        interval: 220
        repeat: false

        onTriggered: {
            if (files.fileBrowserController
                    && searchField.text !== files.fileBrowserController.searchQuery) {
                files.fileBrowserController.setSearchQuery(searchField.text)
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Copy]
        enabled: files.visible && files.hasSelection && !files.showingTrash
        onActivated: files.fileBrowserController.copyEntry(files.selectedPath)
    }

    Shortcut {
        sequences: [StandardKey.Cut]
        enabled: files.visible && files.hasSelection && !files.showingTrash
            && files.fileBrowserController && files.fileBrowserController.homeLocation
        onActivated: files.fileBrowserController.cutEntry(files.selectedPath)
    }

    Shortcut {
        sequences: [StandardKey.Paste]
        enabled: files.visible && files.fileBrowserController && files.fileBrowserController.canPaste
        onActivated: files.fileBrowserController.pasteClipboard()
    }

    Shortcut {
        sequences: [StandardKey.Undo]
        enabled: files.visible && files.fileBrowserController && files.fileBrowserController.canUndo
        onActivated: files.fileBrowserController.undoLastTransfer()
    }

    Shortcut {
        sequence: "Ctrl+T"
        enabled: files.visible
        onActivated: files.newTab()
    }

    Shortcut {
        sequence: "Ctrl+W"
        enabled: files.visible && files.activeTabIndex >= 0
        onActivated: files.closeTab(files.activeTabIndex)
    }

    Shortcut {
        sequence: "Ctrl+L"
        enabled: files.visible && !files.showingTrash
        onActivated: {
            locationField.forceActiveFocus()
            locationField.selectAll()
        }
    }

    minimumWidth: files.minimumSurfaceWidth
    minimumHeight: files.minimumSurfaceHeight
    width: Math.min(1120, Math.max(files.minimumSurfaceWidth, files.screenWidth - (files.desktopMargin * 2)))
    height: Math.min(680, Math.max(files.minimumSurfaceHeight, files.screenHeight - files.panelHeight - (files.desktopMargin * 2)))
    x: files.screenX + Math.max(files.desktopMargin, (files.screenWidth - files.width) / 2)
    y: files.screenY + files.panelHeight + Math.max(files.desktopMargin, (files.screenHeight - files.panelHeight - files.height) / 2)

    function openBrowser() {
        filesRecovery.restoreToReach()
        if (!files.fileBrowserController) {
            return
        }
        if (files.volumeController) {
            files.volumeController.refresh()
        }
        files.clearSelection()
        files.fileBrowserController.setSearchQuery("")
        searchField.text = ""
        files.fileBrowserController.refresh()
        files.ensureInitialTab()
        files.syncLocationField()
        files.presentWindow()
    }

    function currentLocationSnapshot() {
        if (!files.fileBrowserController) {
            return { title: "Home", kind: "home", path: "", root: "", label: "Home" }
        }
        if (files.fileBrowserController.showingTrash) {
            return { title: "Trash", kind: "trash", path: "", root: "", label: "Trash" }
        }
        const path = files.fileBrowserController.currentPath
        const root = files.fileBrowserController.locationRoot
        const home = files.fileBrowserController.homeLocation
        let title = home && path === files.fileBrowserController.homePath
            ? "Home" : String(path).split("/").filter(function(part) { return part.length > 0 }).pop()
        if (!title) {
            title = home ? "Home" : "Volume"
        }
        let label = title
        if (!home && files.activeTabIndex >= 0 && files.activeTabIndex < files.tabs.length) {
            const activeTab = files.tabs[files.activeTabIndex]
            if (activeTab.kind === "mounted" && activeTab.root === root && activeTab.label) {
                label = activeTab.label
            }
        }
        return {
            title: title,
            kind: home ? "home" : "mounted",
            path: path,
            root: root,
            label: label
        }
    }

    function ensureInitialTab() {
        if (files.tabs.length > 0) {
            return
        }
        files.tabs = [files.currentLocationSnapshot()]
        files.activeTabIndex = 0
    }

    function syncActiveTab() {
        if (files.switchingTabs || !files.fileBrowserController) {
            return
        }
        files.ensureInitialTab()
        if (files.activeTabIndex < 0 || files.activeTabIndex >= files.tabs.length) {
            return
        }
        const nextTabs = files.tabs.slice(0)
        nextTabs[files.activeTabIndex] = files.currentLocationSnapshot()
        files.tabs = nextTabs
    }

    function newTab() {
        files.ensureInitialTab()
        const nextTabs = files.tabs.slice(0)
        nextTabs.push(files.currentLocationSnapshot())
        files.tabs = nextTabs
        files.activeTabIndex = nextTabs.length - 1
        files.activateTab(files.activeTabIndex)
    }

    function closeTab(index) {
        if (index < 0 || index >= files.tabs.length) {
            return
        }
        if (files.tabs.length === 1) {
            files.hide()
            return
        }
        const nextTabs = files.tabs.slice(0)
        nextTabs.splice(index, 1)
        files.tabs = nextTabs
        files.activeTabIndex = Math.min(index, nextTabs.length - 1)
        files.activateTab(files.activeTabIndex)
    }

    function activateTab(index) {
        if (!files.fileBrowserController || index < 0 || index >= files.tabs.length) {
            return
        }
        files.activeTabIndex = index
        const tab = files.tabs[index]
        files.switchingTabs = true
        let opened = false
        if (tab.kind === "trash") {
            opened = files.fileBrowserController.showTrash()
        } else if (tab.kind === "mounted") {
            opened = files.fileBrowserController.openLocation(tab.root, tab.label)
            if (opened && tab.path && tab.path !== tab.root) {
                opened = files.fileBrowserController.navigateTo(tab.path)
            }
        } else {
            opened = files.fileBrowserController.goHome()
            if (opened && tab.path && tab.path !== files.fileBrowserController.homePath) {
                opened = files.fileBrowserController.navigateTo(tab.path)
            }
        }
        files.switchingTabs = false
        if (opened) {
            files.clearSelection()
            files.syncActiveTab()
            files.syncLocationField()
        }
    }

    function syncLocationField() {
        if (files.fileBrowserController && !locationField.activeFocus) {
            locationField.text = files.fileBrowserController.displayPath
        }
    }

    function navigateFromLocationField() {
        if (!files.fileBrowserController || files.showingTrash) {
            return
        }
        let requestedPath = String(locationField.text || "").trim()
        if (requestedPath === "~") {
            requestedPath = files.fileBrowserController.homePath
        } else if (requestedPath.indexOf("~/") === 0) {
            requestedPath = files.fileBrowserController.homePath + requestedPath.substring(1)
        }
        if (files.fileBrowserController.navigateTo(requestedPath)) {
            files.clearSelection()
        }
        files.syncLocationField()
    }

    function presentWindow() {
        if (files.visibility === Window.Minimized) {
            files.showNormal()
        } else {
            files.show()
        }
        Qt.callLater(function() {
            files.raise()
            files.requestActivate()
        })
    }

    function openWithSearch(query) {
        files.openBrowser()
        const requestedQuery = String(query || "").trim()
        searchField.text = requestedQuery
        if (files.fileBrowserController) {
            files.fileBrowserController.setSearchQuery(requestedQuery)
        }
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    function setGridView(enabled) {
        if (files.state) {
            files.state.setFilesGridView(enabled)
        } else {
            files.gridView = enabled
        }
    }

    function launchFilePath(path) {
        if (!files.applicationLauncher || !path) {
            return false
        }

        const preferredDesktopId = files.applicationLauncher.preferredApplicationForFile(path)
        if (preferredDesktopId
                && files.applicationLauncher.launchApplicationWithFile(preferredDesktopId, path)) {
            files.clearSelection()
            files.hide()
            return true
        }

        const compatibleApplications = files.applicationLauncher.applicationsForFile(path)
        if (compatibleApplications.length !== 1) {
            return false
        }

        const desktopId = compatibleApplications[0].desktopId
        if (!files.applicationLauncher.launchApplicationWithFile(desktopId, path)) {
            return false
        }

        files.clearSelection()
        files.hide()
        return true
    }

    function openPath(path, isDirectory, isLaunchable) {
        if (!files.fileBrowserController || !path) {
            return
        }

        if (isDirectory) {
            if (!files.fileBrowserController.navigateTo(path)) {
                return
            }
            files.clearSelection()
            show()
            raise()
            requestActivate()
            return
        }

        if (files.launchFilePath(path)) {
            return
        }

        // Keep desktop files in the same user-visible Open With flow as Files.
        // The launchable flag is retained for the application-discovery slice,
        // where verified desktop-entry execution will be added.
        files.openAssociationForPath(path)
    }

    function sortModeIndex() {
        if (!files.fileBrowserController) {
            return 0
        }
        const index = files.sortModes.indexOf(files.fileBrowserController.sortMode)
        return index >= 0 ? index : 0
    }

    function setSortMode(index) {
        if (!files.fileBrowserController || index < 0 || index >= files.sortModes.length) {
            return
        }
        files.clearSelection()
        files.fileBrowserController.setSortMode(files.sortModes[index])
    }

    function openDesktopEntry(path, isDirectory, isLaunchable) {
        if (!files.fileBrowserController || !path) {
            return
        }

        if (isDirectory) {
            files.openPath(path, true, false)
            return
        }

        if (files.launchFilePath(path)) {
            return
        }

        // Keep files without a unique association in the same Open With flow
        // as Files. The launchable flag is reserved for application-entry
        // execution once desktop-entry support is added.
        files.openAssociationForPath(path)
    }

    function openVolume(path, label) {
        if (!files.fileBrowserController
                || !files.fileBrowserController.openLocation(path, label)) {
            return
        }
        files.clearSelection()
        files.presentWindow()
    }

    function openSidebarItem(item) {
        if (!files.fileBrowserController || !item) {
            return
        }

        if (item.kind === "trash") {
            files.openTrash()
            return
        }
        if (item.kind === "home") {
            if (files.fileBrowserController.goHome()) {
                files.clearSelection()
            }
            return
        }

        const childPath = files.fileBrowserController.homeChildPath(item.relativePath)
        if (childPath.length > 0 && files.fileBrowserController.navigateTo(childPath)) {
            files.clearSelection()
        } else if (childPath.length === 0) {
            // Let the controller provide its normal availability/boundary error.
            files.fileBrowserController.navigateTo(
                files.fileBrowserController.homePath + "/" + item.relativePath)
        }
    }

    function sidebarItemAvailable(item) {
        if (!files.fileBrowserController || !item) {
            return false
        }
        return item.kind === "home"
            || item.kind === "trash"
            || files.fileBrowserController.homeChildPath(item.relativePath).length > 0
    }

    function openNameDialog(mode) {
        if (!files.fileBrowserController || (mode === "rename" && !files.hasSelection)) {
            return
        }
        nameDialog.mode = mode
        nameDialog.originalPath = mode === "rename" ? files.selectedPath : ""
        nameField.text = mode === "rename" ? files.selectedName : ""
        nameDialog.open()
    }

    function openSelectedEntry() {
        if (!files.fileBrowserController || files.showingTrash || !files.hasSelection) {
            return
        }
        const entry = files.selectedEntry
        if (files.isApplicationBundle(entry)) {
            files.openBundleInstallDialog(files.selectedPath)
            return
        }
        if (entry.isDirectory) {
            if (files.fileBrowserController.openEntry(files.selectedPath)) {
                files.clearSelection()
            }
            return
        }

        if (files.launchFilePath(files.selectedPath)) {
            return
        }
        files.openAssociationDialog()
    }

    function isApplicationBundle(entry) {
        return !!entry && entry.isDirectory
            && String(entry.name || "").endsWith(".app")
    }

    function entryIconName(entry) {
        if (files.isApplicationBundle(entry)) {
            return "software"
        }
        return entry && entry.isDirectory ? "files" : "editor"
    }

    function openBundleInstallDialog(path) {
        bundleInstallDialog.itemPath = path
        if (!path) {
            bundleInstallDialog.details = ({
                valid: false,
                validationError: "Northstar could not determine which application to install."
            })
        } else if (!files.applicationLauncher) {
            bundleInstallDialog.details = ({
                valid: false,
                validationError: "The Northstar application installer is unavailable."
            })
        } else {
            bundleInstallDialog.details = files.applicationLauncher.applicationBundleDetails(path)
        }
        bundleInstallDialog.operationComplete = false
        bundleInstallDialog.operationError = false
        bundleInstallStatus.text = ""
        bundleInstallDialog.show()
        bundleInstallDialog.raise()
        bundleInstallDialog.requestActivate()
    }

    function openAssociationDialog() {
        if (!files.hasSelection || files.selectedEntry.isDirectory) {
            return
        }
        files.openAssociationForPath(files.selectedPath)
    }

    function previewSelectedEntry() {
        if (!files.hasSelection || !files.previewWindow || !files.previewWindow.presentPath) {
            return
        }
        const navigationRoot = files.fileBrowserController
            ? files.fileBrowserController.locationRoot : ""
        files.previewWindow.presentPath(files.selectedPath, navigationRoot, files)
    }

    function restorePreviewFocus() {
        fileView.forceListFocus()
    }

    function openAssociationForPath(path) {
        if (!files.fileBrowserController || !path) {
            return
        }

        const normalizedPath = String(path).replace(/\\/g, "/")
        associationDialog.itemPath = path
        associationDialog.itemName = normalizedPath.slice(normalizedPath.lastIndexOf("/") + 1)
        associationDialog.extension = files.fileExtension(path)
        associationDialog.showAllApplications = associationDialog.extension.length === 0
        associationDialog.preferredDesktopId = files.applicationLauncher
            ? files.applicationLauncher.preferredApplicationForFile(path) : ""
        associationDialog.rememberChoice.checked = false
        if (files.applicationLauncher) {
            files.applicationLauncher.setApplicationQuery("")
        }
        show()
        raise()
        requestActivate()
        associationDialog.open()
    }

    function launchSelectedFile(desktopId) {
        if (!files.applicationLauncher || !associationDialog.itemPath) {
            return
        }
        if (files.applicationLauncher.launchApplicationWithFile(desktopId, associationDialog.itemPath)) {
            if (associationDialog.rememberChoice.checked) {
                files.applicationLauncher.setPreferredApplicationForFile(associationDialog.itemPath, desktopId)
                associationDialog.preferredDesktopId = desktopId
            }
            associationDialog.close()
            files.hide()
        }
    }

    function clearSelection() {
        files.selectedIndex = -1
    }

    function openTrashDialog() {
        if (!files.fileBrowserController || files.showingTrash || !files.hasSelection) {
            return
        }
        trashDialog.itemPath = files.selectedPath
        trashDialog.itemName = files.selectedName
        trashDialog.open()
    }

    function openTrash() {
        if (!files.fileBrowserController || !files.fileBrowserController.showTrash()) {
            return
        }
        files.clearSelection()
        files.presentWindow()
    }

    function openRestoreDialog() {
        if (!files.fileBrowserController || !files.showingTrash || !files.hasSelection) {
            return
        }
        restoreDialog.itemPath = files.selectedPath
        restoreDialog.itemName = files.selectedName
        restoreDialog.originalLocation = files.fileListOriginalLocation()
        restoreDialog.open()
    }

    function fileListOriginalLocation() {
        if (!files.selectedEntry) {
            return ""
        }
        return files.selectedEntry.originalLocation || "the original location"
    }

    function entrySummary(entry) {
        if (!entry) {
            return ""
        }
        return files.showingTrash
            ? entry.kind + " - " + (entry.originalLocation || "Original location unavailable")
            : files.fileBrowserController && files.fileBrowserController.searching
                ? entry.kind + " - " + (entry.searchLocation || "Home folder")
            : entry.kind + " - " + entry.modified
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

    function localFileUrl(path) {
        const normalizedPath = String(path || "").replace(/\\/g, "/")
        return "file://" + normalizedPath.split("/").map(function(segment) {
            return encodeURIComponent(segment)
        }).join("/")
    }

    function fileExtension(path) {
        const normalizedPath = String(path || "").replace(/\\/g, "/")
        const name = normalizedPath.slice(normalizedPath.lastIndexOf("/") + 1)
        const dot = name.lastIndexOf(".")
        if (dot <= 0 || dot >= name.length - 1) {
            return ""
        }
        return name.slice(dot + 1).toLowerCase()
    }

    function openEmptyTrashDialog() {
        if (!files.fileBrowserController || !files.showingTrash) {
            return
        }
        emptyTrashDialog.open()
    }

    function beginDrag(mouseX, mouseY) {
        if (files.maximized) {
            return
        }
        if (files.startSystemMove()) {
            files.dragging = false
            return
        }
        files.dragging = true
        files.dragOrigin = Qt.point(mouseX, mouseY)
        files.windowOrigin = Qt.point(files.x, files.y)
    }

    function updateDrag(mouseX, mouseY) {
        if (!files.dragging || files.maximized) {
            return
        }
        const deltaX = mouseX - files.dragOrigin.x
        const deltaY = mouseY - files.dragOrigin.y
        const maxX = files.screenX + files.screenWidth - files.width
        const maxY = files.screenY + files.screenHeight - files.height
        files.x = Math.max(files.screenX, Math.min(maxX, files.windowOrigin.x + deltaX))
        files.y = Math.max(files.screenY + files.panelHeight, Math.min(maxY, files.windowOrigin.y + deltaY))
    }

    function endDrag() {
        files.dragging = false
    }

    function beginResize(mouseX, mouseY) {
        if (files.maximized) {
            return
        }
        files.resizeOrigin = Qt.point(mouseX, mouseY)
        files.resizeSize = Qt.size(files.width, files.height)
    }

    function updateResize(mouseX, mouseY) {
        if (files.maximized) {
            return
        }
        const deltaX = mouseX - files.resizeOrigin.x
        const deltaY = mouseY - files.resizeOrigin.y
        files.width = Math.min(files.screenX + files.screenWidth - files.x,
                               Math.max(files.minimumSurfaceWidth, files.resizeSize.width + deltaX))
        files.height = Math.min(files.screenY + files.screenHeight - files.y,
                                Math.max(files.minimumSurfaceHeight, files.resizeSize.height + deltaY))
    }

    // Asking the compositor rather than placing itself. A Wayland client
    // cannot set its own position, so the previous approach set a full-screen
    // width while the window stayed where it was, and it overflowed the screen
    // by however far in it had been.
    function toggleMaximize() {
        if (files.maximized) {
            files.showNormal()
            return
        }
        files.showMaximized()
    }

    NorthstarWindowFrame {
        anchors.fill: parent
        darkMode: lunar.darkMode

        FileBrowserSidebar {
            id: sidebar
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            anchors.left: parent.left
            anchors.leftMargin: 18
            anchors.top: parent.top
            anchors.topMargin: 18
            hostWindow: files
            palette: lunar
            visible: files.sidebarVisible
            width: files.sidebarWidth
            z: 2
        }

        Column {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.leftMargin: files.sidebarVisible ? files.sidebarWidth + 30 : 18
            anchors.right: parent.right
            anchors.rightMargin: 18
            anchors.top: parent.top
            anchors.topMargin: 18
            spacing: 12
            z: 1

            Item {
                id: titleBar
                height: 44
                width: parent.width

                NativeWindowMoveHandler {
                    enabled: false
                    window: files
                }

                NorthstarWindowTitleBar {
                    anchors.fill: parent
                    maximized: files.maximized
                    lunarPalette: lunar
                    subtitle: files.showingTrash
                        ? "Review and restore deleted items"
                        : files.fileBrowserController && files.fileBrowserController.searching
                            ? "Search results from your Northstar home folder"
                        : files.fileBrowserController && files.fileBrowserController.readOnlyLocation
                            ? "Browse a mounted volume (read-only)"
                        : "Browse your Northstar home folder"
                    title: "Files"
                    window: files
                    onMaximizeRequested: files.toggleMaximize()
                }

                Column {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    visible: false

                    Text {
                        color: files.surfaceForeground
                        font.bold: true
                    font.pixelSize: 24
                        text: "Files"
                    }

                    Text {
                        color: files.surfaceMuted
                        font.pixelSize: 12
                        text: files.showingTrash
                            ? "Review and restore deleted items"
                            : files.fileBrowserController && files.fileBrowserController.searching
                                ? "Search results from your Northstar home folder"
                            : files.fileBrowserController && files.fileBrowserController.readOnlyLocation
                                ? "Browse a mounted volume (read-only)"
                            : "Browse your Northstar home folder"
                    }
                }

                Row {
                    id: windowControls
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 7
                    visible: false

                    Rectangle {
                        color: minimizeMouse.containsMouse ? lunar.warning : files.surfaceRaised
                        height: 32
                        radius: 16
                        width: 32

                        Text {
                            anchors.centerIn: parent
                            color: files.surfaceForeground
                            font.bold: true
                            font.pixelSize: 14
                            text: "−"
                        }

                        MouseArea {
                            id: minimizeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: files.hide()
                        }
                    }

                    Rectangle {
                        color: maximizeMouse.containsMouse ? lunar.success : files.surfaceRaised
                        height: 32
                        radius: 16
                        width: 32

                        Text {
                            anchors.centerIn: parent
                            color: files.surfaceForeground
                            font.pixelSize: 13
                            text: files.maximized ? "❐" : "□"
                        }

                        MouseArea {
                            id: maximizeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: files.toggleMaximize()
                        }
                    }

                    Rectangle {
                        color: closeMouse.containsMouse ? lunar.danger : files.surfaceRaised
                        height: 32
                        radius: 16
                        width: 32

                        Text {
                            anchors.centerIn: parent
                            color: files.surfaceForeground
                            font.bold: true
                            font.pixelSize: 15
                            text: "×"
                        }

                        MouseArea {
                            id: closeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: files.hide()
                        }
                    }
                }
            }

            FileBrowserTabBar {
                id: tabBar
                hostWindow: files
                palette: lunar
                width: parent.width
            }

            Rectangle {
                id: locationsSurface
                color: lunar.field
                border.color: lunar.borderSoft
                border.width: 1
                height: 58
                radius: lunar.radiusMedium
                width: parent.width

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: files.surfaceMuted
                        font.bold: true
                        font.pixelSize: 12
                        text: "Locations"
                        width: 68
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        color: !files.showingTrash && files.fileBrowserController
                            && files.fileBrowserController.homeLocation
                            ? files.surfaceAccent : homeLocationMouse.containsMouse
                                ? files.surfaceAccent : files.surfaceBackground
                        border.color: files.surfaceMuted
                        border.width: 1
                        height: 40
                        radius: 6
                        width: 92

                        Row {
                            anchors.centerIn: parent
                            spacing: 6

                            NorthstarIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                height: 24
                                width: 24
                                iconName: "desktop"
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                color: files.surfaceForeground
                                font.pixelSize: 11
                                text: "Home"
                            }
                        }

                        MouseArea {
                            id: homeLocationMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (files.fileBrowserController.goHome()) {
                                    files.clearSelection()
                                }
                            }
                        }
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        color: files.showingTrash ? files.surfaceAccent
                            : trashLocationMouse.containsMouse ? files.surfaceAccent : files.surfaceBackground
                        border.color: files.surfaceMuted
                        border.width: 1
                        height: 40
                        radius: 6
                        width: 92

                        Row {
                            anchors.centerIn: parent
                            spacing: 6

                            NorthstarIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                height: 24
                                width: 24
                                iconName: "trash"
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                color: files.surfaceForeground
                                font.pixelSize: 11
                                text: "Trash"
                            }
                        }

                        MouseArea {
                            id: trashLocationMouse
                            anchors.fill: parent
                            enabled: !!files.fileBrowserController
                            hoverEnabled: true
                            onClicked: files.openTrash()
                        }
                    }

                    Item {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 42
                        width: Math.max(120, parent.width - 270)

                        ListView {
                            id: volumeList
                            anchors.fill: parent
                            clip: true
                            spacing: 8
                            orientation: ListView.Horizontal
                            model: files.volumeController ? files.volumeController.volumes : []

                            delegate: Rectangle {
                                required property var modelData

                                color: volumeMouse.containsMouse
                                    ? files.surfaceAccent : files.surfaceBackground
                                border.color: files.surfaceMuted
                                border.width: 1
                                height: 42
                                radius: 6
                                width: 136

                                Row {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 6

                                    NorthstarIcon {
                                        anchors.verticalCenter: parent.verticalCenter
                                        height: 24
                                        width: 24
                                        iconName: modelData.isSystem ? "desktop" : "files"
                                    }

                                    Column {
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 1
                                        width: parent.width - 30

                                        Text {
                                            color: files.surfaceForeground
                                            elide: Text.ElideRight
                                            font.pixelSize: 11
                                            text: modelData.name || modelData.path
                                            width: parent.width
                                        }

                                        Text {
                                            color: files.surfaceMuted
                                            elide: Text.ElideRight
                                            font.pixelSize: 9
                                            text: modelData.readOnly
                                                ? "Read-only"
                                                : (modelData.capacityLabel || "Mounted volume")
                                            width: parent.width
                                        }
                                    }
                                }

                                MouseArea {
                                    id: volumeMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: files.openVolume(modelData.path, modelData.name)
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            color: files.surfaceMuted
                            font.pixelSize: 11
                            text: "No mounted volumes"
                            visible: volumeList.count === 0
                        }
                    }
                }
            }

            Row {
                id: navigationRow
                spacing: 8
                width: parent.width

                Rectangle {
                    color: backMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    radius: 5
                    width: files.isApplicationBundle(files.selectedEntry) ? 96 : 70

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Up"
                    }

                    MouseArea {
                        id: backMouse
                        anchors.fill: parent
                        enabled: !!files.fileBrowserController
                            && !files.showingTrash
                            && !files.fileBrowserController.searching
                            && files.fileBrowserController.canNavigateUp
                        hoverEnabled: true
                        onClicked: {
                            if (files.fileBrowserController.navigateUp()) {
                                files.clearSelection()
                            }
                        }
                    }
                }

                Rectangle {
                    color: refreshMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    radius: 5
                    width: 86

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Refresh"
                    }

                    MouseArea {
                        id: refreshMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            files.clearSelection()
                            if (files.volumeController) {
                                files.volumeController.refresh()
                            }
                            files.fileBrowserController.refresh()
                        }
                    }
                }

                Rectangle {
                    color: files.gridView || tilesMouse.containsMouse
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    radius: 5
                    width: 60

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.bold: files.gridView
                        font.pixelSize: 12
                        text: "Tiles"
                    }

                    MouseArea {
                        id: tilesMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: files.setGridView(true)
                    }
                }

                Rectangle {
                    color: !files.gridView || listViewMouse.containsMouse
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    radius: 5
                    width: 58

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.bold: !files.gridView
                        font.pixelSize: 12
                        text: "List"
                    }

                    MouseArea {
                        id: listViewMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: files.setGridView(false)
                    }
                }

                AuroraComboBox {
                    id: sortSelector
                    anchors.verticalCenter: parent.verticalCenter
                    currentIndex: files.sortModeIndex()
                    height: 34
                    model: ["Name", "Type", "Size", "Modified"]
                    width: 112

                    onActivated: files.setSortMode(index)
                }

                Rectangle {
                    id: sortOrderButton
                    anchors.verticalCenter: parent.verticalCenter
                    color: sortOrderMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    radius: 5
                    width: 36

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.bold: true
                        font.pixelSize: 15
                        text: files.fileBrowserController && files.fileBrowserController.sortAscending
                            ? "↑" : "↓"
                    }

                    MouseArea {
                        id: sortOrderMouse
                        anchors.fill: parent
                        enabled: !!files.fileBrowserController
                        hoverEnabled: true
                        onClicked: {
                            files.clearSelection()
                            files.fileBrowserController.toggleSortOrder()
                        }
                    }
                }

                TextField {
                    id: locationField
                    anchors.verticalCenter: parent.verticalCenter
                    background: Rectangle {
                        color: files.surfaceBackground
                        border.color: locationField.activeFocus ? files.surfaceAccent : files.surfaceMuted
                        border.width: 1
                        radius: 6
                    }
                    color: files.surfaceMuted
                    enabled: !!files.fileBrowserController && !files.showingTrash
                    font.pixelSize: 13
                    text: files.fileBrowserController ? files.fileBrowserController.displayPath : "~"
                    placeholderText: "Enter a location"
                    selectByMouse: true
                    width: Math.max(80, parent.width - 462)

                    onAccepted: files.navigateFromLocationField()
                    onActiveFocusChanged: {
                        if (!activeFocus) {
                            files.syncLocationField()
                        }
                    }
                }
            }

            Row {
                id: searchRow
                spacing: 8
                width: parent.width

                TextField {
                    id: searchField
                    background: Rectangle {
                        color: files.surfaceBackground
                        border.color: files.surfaceAccent
                        border.width: 1
                        radius: 6
                    }
                    color: files.surfaceForeground
                    enabled: !!files.fileBrowserController && files.fileBrowserController.homeLocation
                        && !files.showingTrash
                    height: 36
                    placeholderText: files.fileBrowserController && files.fileBrowserController.readOnlyLocation
                        ? "Return Home to search"
                        : "Search the Northstar home folder"
                    placeholderTextColor: files.surfaceMuted
                    selectByMouse: true
                    width: parent.width - clearSearchButton.width - searchHint.implicitWidth - (2 * parent.spacing)

                    onTextChanged: {
                        if (files.fileBrowserController
                                && text !== files.fileBrowserController.searchQuery) {
                            searchDebounceTimer.restart()
                        }
                    }

                    onAccepted: searchDebounceTimer.restart()
                }

                Rectangle {
                    id: clearSearchButton
                    color: clearSearchMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 36
                    opacity: searchField.text.length > 0 ? 1 : 0.55
                    radius: 6
                    width: 62

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Clear"
                    }

                    MouseArea {
                        id: clearSearchMouse
                        anchors.fill: parent
                        enabled: searchField.text.length > 0
                        hoverEnabled: true
                        onClicked: searchField.text = ""
                    }
                }

                Text {
                    id: searchHint
                    anchors.verticalCenter: parent.verticalCenter
                    color: files.surfaceMuted
                    font.pixelSize: 11
                    text: searchDebounceTimer.running
                        ? "Searching..."
                        : files.fileBrowserController && files.fileBrowserController.searching
                            ? files.fileBrowserController.entries.length + " result(s)"
                        : files.fileBrowserController && files.fileBrowserController.readOnlyLocation
                            ? "Home-only search"
                            : "Home search"
                }
            }

            Row {
                id: transferRow
                spacing: 8
                width: parent.width

                AuroraButton {
                    text: "Copy"
                    enabled: files.hasSelection && !files.showingTrash
                    onClicked: files.fileBrowserController.copyEntry(files.selectedPath)
                }

                AuroraButton {
                    text: "Cut"
                    enabled: files.hasSelection && !files.showingTrash
                        && files.fileBrowserController && files.fileBrowserController.homeLocation
                    onClicked: files.fileBrowserController.cutEntry(files.selectedPath)
                }

                AuroraButton {
                    text: files.fileBrowserController && files.fileBrowserController.clipboardName
                        ? "Paste " + files.fileBrowserController.clipboardName : "Paste"
                    enabled: files.fileBrowserController && files.fileBrowserController.canPaste
                    onClicked: files.fileBrowserController.pasteClipboard()
                }

                AuroraButton {
                    text: files.fileBrowserController && files.fileBrowserController.undoLabel
                        ? files.fileBrowserController.undoLabel : "Undo"
                    enabled: files.fileBrowserController && files.fileBrowserController.canUndo
                    onClicked: files.fileBrowserController.undoLastTransfer()
                }

                ProgressBar {
                    anchors.verticalCenter: parent.verticalCenter
                    from: 0
                    indeterminate: files.fileBrowserController
                        ? files.fileBrowserController.transferActive : false
                    to: 100
                    value: files.fileBrowserController ? files.fileBrowserController.transferProgress : 0
                    visible: indeterminate || value > 0
                    width: visible ? 100 : 0
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: files.surfaceMuted
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: files.fileBrowserController && files.fileBrowserController.transferStatus
                        ? files.fileBrowserController.transferStatus
                        : "Copy or move one selected item"
                    width: Math.max(80, parent.width - 390)
                }
            }

            Row {
                id: actionRow
                spacing: 8
                width: parent.width

                Rectangle {
                    color: files.hasSelection && quickLookMouse.containsMouse
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.hasSelection ? 1 : 0.55
                    radius: 5
                    width: 92

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Quick Look"
                    }

                    MouseArea {
                        id: quickLookMouse
                        anchors.fill: parent
                        enabled: files.hasSelection
                        hoverEnabled: true
                        onClicked: files.previewSelectedEntry()
                    }
                }

                Rectangle {
                    color: files.hasSelection && !files.showingTrash && openMouse.containsMouse
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.hasSelection && !files.showingTrash ? 1 : 0.55
                    radius: 5
                    width: 70

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: files.isApplicationBundle(files.selectedEntry) ? "Install App" : "Open"
                    }

                    MouseArea {
                        id: openMouse
                        anchors.fill: parent
                        enabled: files.hasSelection && !files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openSelectedEntry()
                    }
                }

                Rectangle {
                    color: files.hasSelection && !files.showingTrash && !files.selectedEntry.isDirectory
                        && openWithMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.hasSelection && !files.showingTrash && !files.selectedEntry.isDirectory
                        ? 1 : 0.55
                    radius: 5
                    width: 100

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Open With..."
                    }

                    MouseArea {
                        id: openWithMouse
                        anchors.fill: parent
                        enabled: files.hasSelection && !files.showingTrash
                            && !files.selectedEntry.isDirectory
                        hoverEnabled: true
                        onClicked: files.openAssociationDialog()
                    }
                }

                Rectangle {
                    color: newFileMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.fileBrowserController && files.fileBrowserController.homeLocation
                        && !files.showingTrash ? 1 : 0.55
                    radius: 5
                    width: 82

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "New File"
                    }

                    MouseArea {
                        id: newFileMouse
                        anchors.fill: parent
                        enabled: !!files.fileBrowserController && files.fileBrowserController.homeLocation
                            && !files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openNameDialog("file")
                    }
                }

                Rectangle {
                    color: newFolderMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.fileBrowserController && files.fileBrowserController.homeLocation
                        && !files.showingTrash ? 1 : 0.55
                    radius: 5
                    width: 96

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "New Folder"
                    }

                    MouseArea {
                        id: newFolderMouse
                        anchors.fill: parent
                        enabled: !!files.fileBrowserController && files.fileBrowserController.homeLocation
                            && !files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openNameDialog("create")
                    }
                }

                Rectangle {
                    color: files.hasSelection && !files.showingTrash && renameMouse.containsMouse
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.hasSelection && files.fileBrowserController
                        && files.fileBrowserController.homeLocation && !files.showingTrash ? 1 : 0.55
                    radius: 5
                    width: 76

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Rename"
                    }

                    MouseArea {
                        id: renameMouse
                        anchors.fill: parent
                        enabled: files.hasSelection && files.fileBrowserController
                            && files.fileBrowserController.homeLocation && !files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openNameDialog("rename")
                    }
                }

                Rectangle {
                    color: files.hasSelection && !files.showingTrash && deleteMouse.containsMouse
                        ? "#c34f65" : files.surfaceRaised
                    height: 34
                    opacity: files.hasSelection && files.fileBrowserController
                        && files.fileBrowserController.homeLocation && !files.showingTrash ? 1 : 0.55
                    radius: 5
                    width: 78

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Delete"
                    }

                    MouseArea {
                        id: deleteMouse
                        anchors.fill: parent
                        enabled: files.hasSelection && files.fileBrowserController
                            && files.fileBrowserController.homeLocation && !files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openTrashDialog()
                    }
                }

                Rectangle {
                    color: files.hasSelection && files.showingTrash && restoreMouse.containsMouse
                        ? files.surfaceAccent : files.surfaceRaised
                    height: 34
                    opacity: files.hasSelection && files.showingTrash ? 1 : 0.55
                    radius: 5
                    visible: files.showingTrash
                    width: 86

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Restore"
                    }

                    MouseArea {
                        id: restoreMouse
                        anchors.fill: parent
                        enabled: files.hasSelection && files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openRestoreDialog()
                    }
                }

                Rectangle {
                    color: files.showingTrash && emptyTrashMouse.containsMouse ? "#c34f65" : files.surfaceRaised
                    height: 34
                    opacity: files.showingTrash ? 1 : 0.55
                    radius: 5
                    visible: files.showingTrash
                    width: 100

                    Text {
                        anchors.centerIn: parent
                        color: files.surfaceForeground
                        font.pixelSize: 12
                        text: "Empty Trash"
                    }

                    MouseArea {
                        id: emptyTrashMouse
                        anchors.fill: parent
                        enabled: files.showingTrash
                        hoverEnabled: true
                        onClicked: files.openEmptyTrashDialog()
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: files.surfaceMuted
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    text: files.showingTrash
                        ? "Select an item to restore it."
                        : "Select an item to open, rename, or delete."
                    width: Math.max(80, parent.width - (files.showingTrash ? 214 : 434))
                }
            }

            FileBrowserFileView {
                id: fileView
                hostWindow: files
                height: Math.max(160, parent.height - titleBar.height - tabBar.height
                    - navigationRow.height - locationsSurface.height - searchRow.height
                    - transferRow.height - actionRow.height - footerText.implicitHeight - 72)
                width: parent.width
            }

            Text {
                id: footerText
                color: files.fileBrowserController && files.fileBrowserController.errorMessage.length > 0
                    ? "#c34f65" : files.surfaceMuted
                elide: Text.ElideRight
                font.pixelSize: 12
                text: files.fileBrowserController && files.fileBrowserController.errorMessage.length > 0
                    ? files.fileBrowserController.errorMessage
                    : files.fileBrowserController && files.fileBrowserController.searching
                        ? "Search results are scoped to the Northstar home folder."
                    : files.fileBrowserController && files.fileBrowserController.readOnlyLocation
                        ? "Mounted volumes are read-only. Return Home to create, rename, or delete files."
                    : "Select an item and press Space for Quick Look, choose Open, or drag it onto an app."
                width: parent.width
            }
        }
    }

    Window {
        id: bundleInstallDialog
        objectName: "bundleInstallDialog"
        color: "transparent"
        flags: Qt.Dialog | Qt.FramelessWindowHint
        modality: Qt.WindowModal
        transientParent: files
        visible: false
        property string itemPath: ""
        property var details: ({})
        property bool operationComplete: false
        property bool operationError: false
        title: details.valid ? details.displayName : "Northstar application"
        width: Math.min(520, files.width - 48)
        height: Math.min(files.screen.height - 64, Math.max(300, installDialogContent.implicitHeight + 40))
        x: files.x + (files.width - width) / 2
        y: files.y + (files.height - height) / 2

        Rectangle {
            anchors.fill: parent
            color: files.surfaceBackground
            border.color: lunar.borderSoft
            border.width: 1
            radius: lunar.radiusLarge

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
                    color: files.surfaceForeground
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
                    color: files.surfaceMuted
                    text: bundleInstallDialog.details.valid
                        ? "Version " + bundleInstallDialog.details.version
                            + "  -  " + bundleInstallDialog.details.bundleIdentifier
                        : (bundleInstallDialog.details.validationError
                            || "Northstar could not validate this application.")
                    width: parent.width
                    wrapMode: Text.WrapAnywhere
                }

                Rectangle {
                    color: lunar.raised
                    radius: lunar.radiusMedium
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

                        Text { color: files.surfaceForeground; text: "Source: " + (bundleInstallDialog.details.source || ""); width: parent.width; wrapMode: Text.WrapAnywhere }
                        Text { color: files.surfaceForeground; text: "Package: " + (bundleInstallDialog.details.package || ""); width: parent.width; wrapMode: Text.WrapAnywhere }
                        Text { color: files.surfaceForeground; text: "Revision: " + (bundleInstallDialog.details.revision || ""); width: parent.width; wrapMode: Text.WrapAnywhere }
                        Text { color: files.surfaceForeground; visible: !!bundleInstallDialog.details.webUrl; text: "Website: " + (bundleInstallDialog.details.webUrl || ""); width: parent.width; wrapMode: Text.WrapAnywhere }
                        Text { color: files.surfaceForeground; visible: !!bundleInstallDialog.details.webUrl; text: "Origin: " + (bundleInstallDialog.details.webOrigin || ""); width: parent.width; wrapMode: Text.WrapAnywhere }
                        Text { color: files.surfaceMuted; visible: !!bundleInstallDialog.details.webUrl; text: bundleInstallDialog.details.webNotice || ""; width: parent.width; wrapMode: Text.WordWrap }
                        Text {
                            color: files.surfaceMuted
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
                    color: bundleInstallDialog.details.alreadyInstalled ? lunar.danger : files.surfaceMuted
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
                    color: bundleInstallDialog.operationError ? lunar.danger : lunar.success
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
                                if (!files.applicationLauncher) {
                                    bundleInstallStatus.text = "The Northstar application installer is unavailable."
                                    bundleInstallDialog.operationError = true
                                    return
                                }
                                const succeeded = files.applicationLauncher.installApplicationBundle(
                                    bundleInstallDialog.itemPath)
                                bundleInstallDialog.operationError = files.applicationLauncher.applicationBundleError()
                                bundleInstallStatus.text = files.applicationLauncher.applicationBundleStatusMessage()
                                if (succeeded) {
                                    bundleInstallDialog.operationComplete = true
                                    bundleInstallDialog.details = files.applicationLauncher.applicationBundleDetails(
                                        bundleInstallDialog.itemPath)
                                    if (files.applicationLauncher) {
                                        files.applicationLauncher.refreshApplications()
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

    Dialog {
        id: associationDialog
        property string itemPath: ""
        property string itemName: ""
        property string extension: ""
        property string preferredDesktopId: ""
        property bool showAllApplications: false

        title: "Open with an application"
        modal: true
        padding: 16
        standardButtons: Dialog.NoButton
        width: Math.min(520, files.width - 48)
        x: (files.width - width) / 2
        y: (files.height - height) / 2

        onOpened: {
            associationSearch.text = ""
            if (files.applicationLauncher) {
                files.applicationLauncher.setApplicationQuery("")
            }
            associationSearch.forceActiveFocus()
        }

        onClosed: {
            associationDialog.showAllApplications = false
            associationSearch.text = ""
            if (files.applicationLauncher) {
                files.applicationLauncher.setApplicationQuery("")
            }
        }

        background: Rectangle {
            color: files.surfaceBackground
            border.color: files.surfaceAccent
            border.width: 1
            radius: 8
        }

        contentItem: Column {
            spacing: 10
            width: associationDialog.width - (2 * associationDialog.padding)

            Text {
                color: files.surfaceForeground
                text: "Choose how Northstar should open \"" + associationDialog.itemName + "\"."
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Text {
                color: files.surfaceMuted
                elide: Text.ElideRight
                text: associationDialog.preferredDesktopId.length > 0
                    ? "Current default: " + associationDialog.preferredDesktopId
                    : "No Northstar default is saved for this file type."
                width: parent.width
            }

            TextField {
                id: associationSearch
                background: Rectangle {
                    color: files.surfaceBackground
                    border.color: files.surfaceMuted
                    border.width: 1
                    radius: 6
                }
                color: files.surfaceForeground
                placeholderText: "Search applications"
                placeholderTextColor: files.surfaceMuted
                selectByMouse: true
                width: parent.width

                onTextChanged: {
                    if (files.applicationLauncher
                            && files.applicationLauncher.applicationQuery !== text) {
                        files.applicationLauncher.setApplicationQuery(text)
                    }
                }
            }

            Row {
                spacing: 10
                width: parent.width

                Text {
                    color: files.surfaceMuted
                    elide: Text.ElideRight
                    text: associationDialog.showAllApplications
                        ? "Showing all registered applications."
                        : "Showing applications that declare support for this file type."
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width - showAllApplicationsButton.width - parent.spacing
                }

                AuroraButton {
                    id: showAllApplicationsButton
                    text: "Show All"
                    visible: !associationDialog.showAllApplications
                    onClicked: associationDialog.showAllApplications = true
                }
            }

            Rectangle {
                color: files.surfaceBackground
                border.color: files.surfaceMuted
                border.width: 1
                height: 220
                width: parent.width

                ListView {
                    id: associationList
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    model: files.applicationLauncher
                        ? files.applicationLauncher.applicationQuery.length > 0
                            ? files.applicationLauncher.matchingApplications
                            : associationDialog.showAllApplications
                                ? files.applicationLauncher.applications
                                : files.applicationLauncher.applicationsForFile(associationDialog.itemPath)
                        : []

                    delegate: Rectangle {
                        required property var modelData

                        color: associationMouse.containsMouse ? files.surfaceAccent : files.surfaceRaised
                        height: 52
                        radius: 6
                        width: associationList.width - 12

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            Item {
                                anchors.verticalCenter: parent.verticalCenter
                                height: 34
                                width: 34

                                NorthstarIcon {
                                    anchors.fill: parent
                                    visible: modelData.sourceType !== "bundle" || !modelData.iconSource
                                    iconName: files.applicationIconName(modelData)
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

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                width: parent.width - 44

                                Text {
                                    color: files.surfaceForeground
                                    elide: Text.ElideRight
                                    font.pixelSize: 13
                                    text: modelData.name
                                    width: parent.width
                                }

                                Text {
                                    color: files.surfaceMuted
                                    elide: Text.ElideRight
                                    font.pixelSize: 10
                                    text: modelData.genericName || modelData.desktopId
                                    width: parent.width
                                }
                            }
                        }

                        MouseArea {
                            id: associationMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: files.launchSelectedFile(modelData.desktopId)
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }

                Text {
                    anchors.centerIn: parent
                    color: files.surfaceMuted
                    text: associationDialog.showAllApplications
                        ? "No registered applications were found."
                        : "No compatible applications found. Use Show All to choose manually."
                    visible: associationList.count === 0
                }
            }

            CheckBox {
                id: rememberChoice
                text: associationDialog.extension.length > 0
                    ? "Remember this choice for ." + associationDialog.extension + " files"
                    : "Remember this choice for files of this type"
                visible: associationList.count > 0
                width: parent.width

                contentItem: Text {
                    color: files.surfaceForeground
                    leftPadding: rememberChoice.indicator.width + 8
                    text: rememberChoice.text
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Row {
                spacing: 8
                width: parent.width

                AuroraButton {
                    text: "Forget Default"
                    visible: associationDialog.preferredDesktopId.length > 0
                    onClicked: {
                        if (files.applicationLauncher
                                && files.applicationLauncher.clearPreferredApplicationForFile(associationDialog.itemPath)) {
                            associationDialog.preferredDesktopId = ""
                        }
                    }
                }

                AuroraButton {
                    text: "Use System Default"
                    enabled: !!files.fileBrowserController
                    onClicked: {
                        if (files.fileBrowserController.openEntry(associationDialog.itemPath)) {
                            associationDialog.close()
                            files.hide()
                        }
                    }
                }

                AuroraButton {
                    text: "Cancel"
                    onClicked: associationDialog.close()
                }
            }
        }
    }

    Dialog {
        id: pasteConflictDialog

        title: "An item already exists"
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel
        width: Math.min(460, files.width - 48)
        x: (files.width - width) / 2
        y: (files.height - height) / 2

        background: Rectangle {
            color: files.surfaceBackground
            border.color: lunar.warning
            border.width: 1
            radius: 8
        }

        contentItem: Column {
            spacing: 12
            width: pasteConflictDialog.width - (2 * pasteConflictDialog.padding)

            Text {
                color: files.surfaceForeground
                text: "A file or folder named \""
                    + (files.fileBrowserController ? files.fileBrowserController.conflictName : "item")
                    + "\" is already in this location. Keep both items with a safe copy name?"
                wrapMode: Text.WordWrap
                width: parent.width
            }

            AuroraButton {
                text: "Keep Both"
                onClicked: {
                    if (files.fileBrowserController.pasteClipboard("keepBoth")) {
                        pasteConflictDialog.close()
                        files.clearSelection()
                    }
                }
            }
        }

        onRejected: {
            if (files.fileBrowserController) {
                files.fileBrowserController.cancelConflict()
            }
        }
    }

    Dialog {
        id: nameDialog
        property string mode: "create"
        property string originalPath: ""

        title: mode === "rename" ? "Rename item"
            : mode === "file" ? "Create file" : "Create folder"
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: Math.min(420, files.width - 48)
        x: (files.width - width) / 2
        y: (files.height - height) / 2

        background: Rectangle {
            color: files.surfaceBackground
            border.color: files.surfaceAccent
            border.width: 1
            radius: 8
        }

        contentItem: Column {
            spacing: 10
            width: nameDialog.width - (2 * nameDialog.padding)

            Text {
                color: files.surfaceForeground
                text: nameDialog.mode === "rename"
                    ? "Choose a new name for the selected item."
                    : nameDialog.mode === "file"
                        ? "Choose a name for the new empty file."
                        : "Choose a name for the new folder."
                wrapMode: Text.WordWrap
                width: parent.width
            }

            TextField {
                id: nameField
                width: parent.width
                placeholderText: "Name"
                selectByMouse: true
                onAccepted: nameDialog.accept()
            }
        }

        onOpened: {
            nameField.forceActiveFocus()
            nameField.selectAll()
        }

        onAccepted: {
            const succeeded = mode === "rename"
                ? files.fileBrowserController.renameEntry(originalPath, nameField.text)
                : mode === "file"
                    ? files.fileBrowserController.createFile(nameField.text)
                    : files.fileBrowserController.createFolder(nameField.text)
            if (succeeded) {
                files.clearSelection()
            } else {
                Qt.callLater(function() { nameDialog.open() })
            }
        }
    }

    Dialog {
        id: trashDialog
        property string itemPath: ""
        property string itemName: ""

        title: "Delete item?"
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: Math.min(420, files.width - 48)
        x: (files.width - width) / 2
        y: (files.height - height) / 2

        background: Rectangle {
            color: files.surfaceBackground
            border.color: files.surfaceAccent
            border.width: 1
            radius: 8
        }

        contentItem: Text {
            color: files.surfaceForeground
            text: "Move \"" + trashDialog.itemName
                + "\" to the Northstar Trash? You can restore it later."
            wrapMode: Text.WordWrap
            width: trashDialog.width - (2 * trashDialog.padding)
        }

        onAccepted: {
            if (files.fileBrowserController.moveToTrash(itemPath)) {
                files.clearSelection()
            } else {
                Qt.callLater(function() { trashDialog.open() })
            }
        }
    }

    Dialog {
        id: restoreDialog
        property string itemPath: ""
        property string itemName: ""
        property string originalLocation: ""

        title: "Restore item?"
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: Math.min(440, files.width - 48)
        x: (files.width - width) / 2
        y: (files.height - height) / 2

        background: Rectangle {
            color: files.surfaceBackground
            border.color: files.surfaceAccent
            border.width: 1
            radius: 8
        }

        contentItem: Text {
            color: files.surfaceForeground
            text: "Restore \"" + restoreDialog.itemName + "\" to "
                + restoreDialog.originalLocation + "?"
            wrapMode: Text.WordWrap
            width: restoreDialog.width - (2 * restoreDialog.padding)
        }

        onAccepted: {
            if (files.fileBrowserController.restoreEntry(itemPath)) {
                files.clearSelection()
            } else {
                Qt.callLater(function() { restoreDialog.open() })
            }
        }
    }

    Dialog {
        id: emptyTrashDialog

        title: "Empty Trash?"
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: Math.min(420, files.width - 48)
        x: (files.width - width) / 2
        y: (files.height - height) / 2

        background: Rectangle {
            color: files.surfaceBackground
            border.color: "#c34f65"
            border.width: 1
            radius: 8
        }

        contentItem: Text {
            color: files.surfaceForeground
            text: "This permanently removes every item currently in the Northstar Trash."
            wrapMode: Text.WordWrap
            width: emptyTrashDialog.width - (2 * emptyTrashDialog.padding)
        }

        onAccepted: {
            if (files.fileBrowserController.emptyTrash()) {
                files.clearSelection()
            } else {
                Qt.callLater(function() { emptyTrashDialog.open() })
            }
        }
    }

    Rectangle {
        id: resizeHandle
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: files.surfaceAccent
        height: 18
        opacity: 0.85
        visible: false
        width: 18
        z: 10

        Text {
            anchors.centerIn: parent
            color: files.surfaceBackground
            font.pixelSize: 12
            rotation: 45
            text: "..."
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            onPressed: files.beginResize(mouse.x, mouse.y)
            onPositionChanged: files.updateResize(mouse.x, mouse.y)
        }
    }


    // Every window that can be maximised can also be stranded beyond reach,
    // so each one carries the same way back.
    NorthstarWindowRecovery {
        id: filesRecovery
        window: files
        panelHeight: files.panelHeight
        desktopMargin: files.desktopMargin
        screenX: files.screenX
        screenY: files.screenY
        screenWidth: files.screenWidth
        screenHeight: files.screenHeight
        minimumSurfaceWidth: files.minimumSurfaceWidth
        minimumSurfaceHeight: files.minimumSurfaceHeight
    }

    NativeWindowResizeHandler {
        resizingEnabled: !files.maximized
        window: files
    }
}
