import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: settings
    objectName: "settingsWindow"

    LunarPalette {
        id: lunar
        darkMode: settings.state ? settings.state.darkMode : true
    }

    property var state
    property var desktopLayoutController
    property var launcherController
    property var sessionController
    property var settingsCatalog
    property var wallpaperController
    property bool hasCatalog: settingsCatalog !== null && settingsCatalog !== undefined
    property bool hasSessionController: sessionController !== null && sessionController !== undefined
    property bool sessionFailed: settings.hasSessionController
        && settings.sessionController.state === "failed"
    property var targetScreen
    property int panelHeight: 44
    property int desktopMargin: 24
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property int minimumSurfaceWidth: 720
    property int minimumSurfaceHeight: 440
    // Read from the compositor rather than tracked here. A shell-held
    // copy can disagree with the real state, and on Wayland the
    // compositor is the only thing that knows.
    property bool maximized: visibility === Window.Maximized
    property bool dragging: false
    property point dragOrigin: Qt.point(0, 0)
    property point windowOrigin: Qt.point(0, 0)
    property point resizeOrigin: Qt.point(0, 0)
    property size resizeSize: Qt.size(0, 0)
    property color surfaceBackground: lunar.panelStrong
    property color surfaceForeground: lunar.foreground
    property color surfaceMuted: lunar.muted
    property color surfaceAccent: lunar.accent
    property color surfaceRaised: lunar.raised

    property string selectedSection: settings.hasCatalog ? settings.settingsCatalog.selectedSection : ""
    property bool searching: settings.hasCatalog && settings.settingsCatalog.searching

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.NonModal
    title: "Northstar Settings"

    minimumWidth: settings.minimumSurfaceWidth
    minimumHeight: settings.minimumSurfaceHeight
    width: Math.min(960, Math.max(minimumSurfaceWidth, screenWidth - (desktopMargin * 2)))
    height: Math.min(screenHeight - panelHeight - desktopMargin, Math.max(minimumSurfaceHeight, 720))
    x: screenX + Math.max(desktopMargin, (screenWidth - width) / 2)
    y: screenY + panelHeight + desktopMargin

    function openSettings() {
        settingsRecovery.restoreToReach()
        show()
        raise()
        requestActivate()
        if (sessionController) {
            sessionController.refresh()
        }
        if (settings.hasCatalog) {
            settings.settingsCatalog.refresh()
        }
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    function selectSection(sectionId) {
        if (!settings.hasCatalog) {
            return
        }
        settings.settingsCatalog.clearQuery()
        searchField.text = ""
        settings.settingsCatalog.setSelectedSection(sectionId)
    }

    // Destructive entries are confirmed here rather than in each delegate, so
    // every one of them gets the same explicit confirmation.
    function activateEntry(entry) {
        if (!settings.hasCatalog || !entry.available) {
            return
        }
        if (entry.destructive) {
            confirmDialog.entryId = entry.id
            confirmDialog.entryTitle = entry.title
            confirmDialog.entryDescription = entry.description
            confirmDialog.hidesWindow = entry.id === "session.restartshell"
                || entry.id === "session.end"
            confirmDialog.open()
            return
        }
        settings.settingsCatalog.invoke(entry.id)
    }

    // Asking the compositor rather than placing itself. A Wayland client
    // cannot set its own position, so the previous approach set a full-screen
    // width while the window stayed where it was, and it overflowed the screen
    // by however far in it had been.
    function toggleMaximize() {
        if (settings.maximized) {
            settings.showNormal()
            return
        }
        settings.showMaximized()
    }

    function beginDrag(mouseX, mouseY) {
        if (settings.maximized) {
            return
        }
        if (settings.startSystemMove()) {
            settings.dragging = false
            return
        }
        settings.dragging = true
        settings.dragOrigin = Qt.point(mouseX, mouseY)
        settings.windowOrigin = Qt.point(settings.x, settings.y)
    }

    function updateDrag(mouseX, mouseY) {
        if (!settings.dragging || settings.maximized) {
            return
        }
        const deltaX = mouseX - settings.dragOrigin.x
        const deltaY = mouseY - settings.dragOrigin.y
        const maxX = settings.screenX + settings.screenWidth - settings.width
        const maxY = settings.screenY + settings.screenHeight - settings.height
        settings.x = Math.max(settings.screenX, Math.min(maxX, settings.windowOrigin.x + deltaX))
        settings.y = Math.max(settings.screenY + settings.panelHeight, Math.min(maxY, settings.windowOrigin.y + deltaY))
    }

    function endDrag() {
        settings.dragging = false
    }

    function beginResize(mouseX, mouseY) {
        if (settings.maximized) {
            return
        }
        settings.resizeOrigin = Qt.point(mouseX, mouseY)
        settings.resizeSize = Qt.size(settings.width, settings.height)
    }

    function updateResize(mouseX, mouseY) {
        if (settings.maximized) {
            return
        }
        const deltaX = mouseX - settings.resizeOrigin.x
        const deltaY = mouseY - settings.resizeOrigin.y
        settings.width = Math.min(settings.screenX + settings.screenWidth - settings.x,
                                  Math.max(settings.minimumSurfaceWidth, settings.resizeSize.width + deltaX))
        settings.height = Math.min(settings.screenY + settings.screenHeight - settings.y,
                                   Math.max(settings.minimumSurfaceHeight, settings.resizeSize.height + deltaY))
    }

    Timer {
        interval: 1000
        repeat: true
        running: settings.visible && settings.hasSessionController
        onTriggered: settings.sessionController.refresh()
    }

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: settings.visible
        onActivated: {
            searchField.forceActiveFocus()
            searchField.selectAll()
        }
    }


    Dialog {
        id: confirmDialog
        modal: true
        title: "Confirm"
        standardButtons: Dialog.Cancel | Dialog.Ok
        width: 440
        x: (settings.width - width) / 2
        y: (settings.height - height) / 2

        property string entryId: ""
        property string entryTitle: ""
        property string entryDescription: ""
        property bool hidesWindow: false

        contentItem: Column {
            spacing: 10
            width: confirmDialog.width - 32

            Text {
                color: settings.surfaceForeground
                font.bold: true
                font.pixelSize: 14
                text: confirmDialog.entryTitle
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Text {
                color: settings.surfaceMuted
                font.pixelSize: 12
                text: confirmDialog.entryDescription
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }

        onAccepted: {
            const performed = settings.settingsCatalog.invoke(confirmDialog.entryId)
            if (performed && confirmDialog.hidesWindow) {
                settings.hide()
            }
        }
    }

    NorthstarWindowFrame {
        anchors.fill: parent
        darkMode: lunar.darkMode

        Item {
            anchors.fill: parent
            anchors.margins: 22

            Item {
                id: titleBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 48

                NativeWindowMoveHandler {
                    enabled: false
                    window: settings
                }

                NorthstarWindowTitleBar {
                    anchors.fill: parent
                    maximized: settings.maximized
                    lunarPalette: lunar
                    subtitle: "Northstar desktop preferences"
                    title: "Settings"
                    window: settings
                    onMaximizeRequested: settings.toggleMaximize()
                }
            }

            // --- Search --------------------------------------------------------

            Rectangle {
                id: searchBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: titleBar.bottom
                anchors.topMargin: 14
                border.color: searchField.activeFocus ? lunar.accent : lunar.borderSoft
                border.width: 1
                color: lunar.field
                height: 46
                radius: lunar.radiusMedium

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: settings.surfaceMuted
                        font.pixelSize: 15
                        text: "⌕"
                    }

                    TextField {
                        id: searchField
                        anchors.verticalCenter: parent.verticalCenter
                        background: null
                        color: settings.surfaceForeground
                        placeholderText: "Search settings"
                        placeholderTextColor: settings.surfaceMuted
                        selectByMouse: true
                        width: parent.width - 200
                        onTextChanged: {
                            if (settings.hasCatalog) {
                                settings.settingsCatalog.setQuery(text)
                            }
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: settings.surfaceMuted
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                        text: settings.searching && settings.hasCatalog
                            ? (settings.settingsCatalog.resultCount === 1
                                ? "1 result" : settings.settingsCatalog.resultCount + " results")
                            : ""
                        width: 84
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        color: clearSearchMouse.containsMouse ? lunar.raisedHover : "transparent"
                        height: 26
                        radius: 13
                        visible: settings.searching
                        width: 26

                        Text {
                            anchors.centerIn: parent
                            color: settings.surfaceForeground
                            font.pixelSize: 13
                            text: "×"
                        }

                        MouseArea {
                            id: clearSearchMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                searchField.text = ""
                                settings.settingsCatalog.clearQuery()
                            }
                        }
                    }
                }
            }

            // --- Status --------------------------------------------------------

            Rectangle {
                id: statusArea
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                color: statusLine.text.length > 0 ? lunar.raised : "transparent"
                height: statusLine.text.length > 0 ? statusLine.implicitHeight + 12 : 20
                radius: lunar.radiusSmall

                Text {
                    id: statusLine
                    anchors.fill: parent
                    anchors.margins: statusLine.text.length > 0 ? 6 : 0
                    color: settings.hasCatalog && settings.settingsCatalog.statusIsError
                        ? lunar.danger : settings.surfaceMuted
                    font.pixelSize: 12
                    text: settings.hasCatalog ? settings.settingsCatalog.statusMessage : ""
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                }
            }

            // --- Sections and results -------------------------------------------

            Row {
                anchors.bottom: statusArea.top
                anchors.bottomMargin: 8
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: searchBar.bottom
                anchors.topMargin: 14
                spacing: 18

                Rectangle {
                    id: sectionSidebar
                    border.color: lunar.borderSoft
                    border.width: 1
                    color: lunar.panel
                    height: parent.height
                    opacity: settings.searching ? 0.5 : 1
                    radius: lunar.radiusLarge
                    width: 196

                    ListView {
                        id: sectionList
                        anchors.fill: parent
                        anchors.margins: 10
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true
                        model: settings.hasCatalog ? settings.settingsCatalog.sections : []
                        spacing: 4

                        ScrollBar.vertical: ScrollBar {
                            id: sectionScrollBar
                            active: sectionList.contentHeight > sectionList.height
                            policy: ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            required property var modelData

                            color: !settings.searching && settings.selectedSection === modelData.id
                                ? lunar.accentSoft
                                : sectionMouse.containsMouse ? lunar.raisedHover : "transparent"
                            height: 40
                            radius: lunar.radiusSmall
                            width: sectionList.width - sectionScrollBar.width - 6

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.right: sectionCount.left
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                color: !settings.searching && settings.selectedSection === modelData.id
                                    ? settings.surfaceForeground : settings.surfaceMuted
                                elide: Text.ElideRight
                                font.bold: !settings.searching && settings.selectedSection === modelData.id
                                font.pixelSize: 13
                                text: modelData.label
                            }

                            Text {
                                id: sectionCount
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                color: settings.surfaceMuted
                                font.pixelSize: 11
                                text: modelData.count
                            }

                            MouseArea {
                                id: sectionMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: settings.selectSection(modelData.id)
                            }
                        }
                    }
                }

                Rectangle {
                    color: settings.surfaceRaised
                    height: parent.height
                    radius: lunar.radiusLarge
                    width: parent.width - sectionSidebar.width - parent.spacing

                    Text {
                        anchors.centerIn: parent
                        color: settings.surfaceMuted
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        text: "No setting matches \"" + (settings.hasCatalog
                            ? settings.settingsCatalog.query : "") + "\"."
                        visible: settings.searching && settings.hasCatalog
                            && settings.settingsCatalog.resultCount === 0
                        width: parent.width - 60
                        wrapMode: Text.WordWrap
                    }

                    ListView {
                        id: entryList
                        anchors.fill: parent
                        anchors.margins: 16
                        clip: true
                        model: settings.hasCatalog ? settings.settingsCatalog.entries : []
                        spacing: 10

                        ScrollBar.vertical: ScrollBar {
                            id: entryScrollBar
                            active: entryList.contentHeight > entryList.height
                            policy: ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            required property var modelData

                            border.color: lunar.borderSoft
                            border.width: 1
                            color: settings.surfaceBackground
                            height: entryBody.implicitHeight + 26
                            radius: lunar.radiusMedium
                            width: entryList.width - entryScrollBar.width - 6

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 14

                                Column {
                                    id: entryBody
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 3
                                    width: parent.width - entryControl.width - parent.spacing

                                    Text {
                                        color: settings.surfaceForeground
                                        elide: Text.ElideRight
                                        font.bold: true
                                        font.pixelSize: 14
                                        text: modelData.title
                                        width: parent.width
                                    }

                                    Text {
                                        color: settings.surfaceMuted
                                        font.pixelSize: 12
                                        text: modelData.description
                                        visible: text.length > 0
                                        width: parent.width
                                        wrapMode: Text.WordWrap
                                    }

                                    // Searching crosses sections, so each result
                                    // says where it actually lives.
                                    Text {
                                        color: settings.surfaceAccent
                                        font.pixelSize: 11
                                        text: modelData.sectionLabel
                                        visible: settings.searching
                                        width: parent.width
                                    }

                                    Text {
                                        color: lunar.warning
                                        font.pixelSize: 11
                                        text: modelData.unavailableReason
                                        visible: !modelData.available
                                            && modelData.unavailableReason.length > 0
                                        width: parent.width
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                SettingsEntryControl {
                                    id: entryControl
                                    anchors.verticalCenter: parent.verticalCenter
                                    catalog: settings.settingsCatalog
                                    entry: modelData
                                    foregroundColor: settings.surfaceForeground
                                    mutedColor: settings.surfaceMuted
                                    onActionRequested: (requested) => settings.activateEntry(requested)
                                    onPathChooseRequested: settingsWallpaperPicker.openAt()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Settings offers the same picker the desktop does. It is declared once
    // here and reached by every path entry, because this build has exactly
    // one picture-valued setting.
    WallpaperPicker {
        id: settingsWallpaperPicker

        surfaceAccent: settings.surfaceAccent
        surfaceBackground: settings.surfaceBackground
        surfaceForeground: settings.surfaceForeground
        surfaceMuted: settings.surfaceMuted
        surfaceRaised: settings.surfaceRaised
        wallpaper: settings.wallpaperController
        width: Math.min(620, settings.width - 48)
        x: (settings.width - width) / 2
        y: Math.max(24, (settings.height - height) / 2)
    }


    // Every window that can be maximised can also be stranded beyond reach,
    // so each one carries the same way back.
    NorthstarWindowRecovery {
        id: settingsRecovery
        window: settings
        panelHeight: settings.panelHeight
        desktopMargin: settings.desktopMargin
        screenX: settings.screenX
        screenY: settings.screenY
        screenWidth: settings.screenWidth
        screenHeight: settings.screenHeight
        minimumSurfaceWidth: settings.minimumSurfaceWidth
        minimumSurfaceHeight: settings.minimumSurfaceHeight

        // Clearing a search is a better answer to Escape than closing the
        // window someone is searching in.
        handledLocally: settings.searching
        onEscapePressed: {
            searchField.text = ""
            settings.settingsCatalog.clearQuery()
        }
    }

    NativeWindowResizeHandler {
        resizingEnabled: !settings.maximized
        window: settings
    }
}
