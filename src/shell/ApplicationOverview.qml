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
    property var state
    property var targetScreen
    property int panelHeight: 44
    property int desktopMargin: 24
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property color surfaceBackground: lunar.panelStrong
    property color surfaceForeground: lunar.foreground
    property color surfaceMuted: lunar.muted
    property color surfaceAccent: lunar.accent

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.NonModal
    title: "Northstar Applications"

    width: Math.min(820, screenWidth - 80)
    height: Math.min(520, screenHeight - panelHeight - 80)
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

    Rectangle {
        anchors.fill: parent
        color: overview.surfaceBackground
        border.color: lunar.border
        border.width: 1
        radius: lunar.radiusPanel

        gradient: Gradient {
            GradientStop { position: 0.0; color: lunar.panelStrong }
            GradientStop { position: 1.0; color: lunar.panel }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16

            Row {
                width: parent.width
                spacing: 10

                Item {
                    id: overviewDragHandle
                    height: 42
                    width: parent.width - closeButton.width - parent.spacing

                    Column {
                        anchors.fill: parent
                        spacing: 2

                        Text {
                            color: overview.surfaceForeground
                            font.bold: true
                            font.pixelSize: 20
                            text: "Applications"
                        }

                        Text {
                            color: overview.surfaceMuted
                            font.pixelSize: 12
                            text: "Launch your Northstar applications"
                        }
                    }

                    NativeWindowMoveHandler {
                        window: overview
                    }
                }

                Button {
                    id: closeButton
                    text: "Close"
                    onClicked: overview.visible = false
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
                    cellHeight: 112
                    cellWidth: 124
                    clip: true
                    model: applicationLauncher ? applicationLauncher.matchingApplications : []

                    delegate: Rectangle {
                        required property var modelData

                        color: applicationDropArea.containsDrag || applicationMouse.containsMouse
                            ? lunar.raisedHover : "transparent"
                        height: 102
                        radius: lunar.radiusMedium
                        width: 112
                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 5

                            Item {
                                anchors.horizontalCenter: parent.horizontalCenter
                                height: 44
                                width: 44

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
                                font.pixelSize: 11
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
                            hoverEnabled: true
                            onClicked: {
                                applicationLauncher.launchApplication(modelData.desktopId)
                                overview.visible = false
                            }
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

                Rectangle {
                    color: pinnedTerminalMouse.containsMouse ? lunar.raisedHover : lunar.raised
                    height: 36
                    radius: 12
                    width: 112

                    Row {
                        anchors.centerIn: parent
                        spacing: 7

                        NorthstarIcon { height: 22; iconName: "terminal"; width: 22 }
                        Text { color: overview.surfaceForeground; font.pixelSize: 11; text: "Terminal" }
                    }

                    MouseArea {
                        id: pinnedTerminalMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            overview.applicationLauncher.launchTerminal()
                            overview.hide()
                        }
                    }
                }

                Rectangle {
                    color: pinnedBrowserMouse.containsMouse ? lunar.raisedHover : lunar.raised
                    height: 36
                    radius: 12
                    width: 104

                    Row {
                        anchors.centerIn: parent
                        spacing: 7

                        NorthstarIcon { height: 22; iconName: "browser"; width: 22 }
                        Text { color: overview.surfaceForeground; font.pixelSize: 11; text: "Firefox" }
                    }

                    MouseArea {
                        id: pinnedBrowserMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            overview.applicationLauncher.launchBrowser()
                            overview.hide()
                        }
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: overview.surfaceMuted
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignRight
                    text: "Tip: drag a file onto a compatible app"
                    width: parent.width - 294
                }
            }
        }
    }
}
