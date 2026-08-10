import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: quickLook
    objectName: "quickLookWindow"

    LunarPalette {
        id: lunar
        darkMode: quickLook.state ? quickLook.state.darkMode : true
    }

    property var previewController
    property var state
    property var targetScreen
    property int panelHeight: 44
    property int desktopMargin: 24
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property int screenHeight: targetScreen ? targetScreen.geometry.height : 800
    property int minimumSurfaceWidth: 520
    property int minimumSurfaceHeight: 360
    property bool maximized: false
    property var returnFocusWindow: null
    property point normalGeometryPosition: Qt.point(0, 0)
    property size normalGeometrySize: Qt.size(0, 0)

    visible: false
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.NonModal
    title: previewController && previewController.title.length > 0
        ? previewController.title + " — Quick Look" : "Northstar Quick Look"

    minimumWidth: minimumSurfaceWidth
    minimumHeight: minimumSurfaceHeight
    width: Math.min(720, Math.max(minimumSurfaceWidth, screenWidth - 120))
    height: Math.min(560, Math.max(minimumSurfaceHeight, screenHeight - panelHeight - 100))
    x: screenX + Math.max(desktopMargin, (screenWidth - width) / 2)
    y: screenY + panelHeight + Math.max(desktopMargin, (screenHeight - panelHeight - height) / 2)

    function presentPath(path, navigationRoot, originWindow) {
        if (!quickLook.previewController || !path) {
            return false
        }
        quickLook.returnFocusWindow = originWindow || null
        quickLook.previewController.previewPath(path, navigationRoot || "")
        quickLook.showNormal()
        quickLook.show()
        quickLook.raise()
        quickLook.requestActivate()
        return quickLook.previewController.status === "ready"
    }

    function restoreOriginFocus() {
        const origin = quickLook.returnFocusWindow
        if (!origin || !origin.visible) {
            return
        }
        origin.raise()
        origin.requestActivate()
        if (origin.restorePreviewFocus) {
            Qt.callLater(function() { origin.restorePreviewFocus() })
        }
    }

    onVisibleChanged: {
        if (!visible) {
            quickLook.restoreOriginFocus()
        }
    }

    function toggleMaximize() {
        if (quickLook.maximized) {
            quickLook.x = quickLook.normalGeometryPosition.x
            quickLook.y = quickLook.normalGeometryPosition.y
            quickLook.width = quickLook.normalGeometrySize.width
            quickLook.height = quickLook.normalGeometrySize.height
            quickLook.maximized = false
            return
        }
        quickLook.normalGeometryPosition = Qt.point(quickLook.x, quickLook.y)
        quickLook.normalGeometrySize = Qt.size(quickLook.width, quickLook.height)
        quickLook.x = quickLook.screenX
        quickLook.y = quickLook.screenY + quickLook.panelHeight
        quickLook.width = quickLook.screenWidth
        quickLook.height = Math.max(quickLook.minimumSurfaceHeight,
                                    quickLook.screenHeight - quickLook.panelHeight)
        quickLook.maximized = true
    }

    Shortcut {
        sequence: "Escape"
        enabled: quickLook.visible
        onActivated: quickLook.hide()
    }

    NorthstarWindowFrame {
        anchors.fill: parent
        darkMode: lunar.darkMode

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            NorthstarWindowTitleBar {
                lunarPalette: lunar
                maximized: quickLook.maximized
                subtitle: quickLook.previewController
                    ? quickLook.previewController.subtitle : ""
                title: quickLook.previewController && quickLook.previewController.title.length > 0
                    ? quickLook.previewController.title : "Quick Look"
                window: quickLook
                width: parent.width
                onMaximizeRequested: quickLook.toggleMaximize()
            }

            Rectangle {
                color: lunar.background
                border.color: lunar.borderSoft
                border.width: 1
                height: Math.max(220, parent.height - 118)
                radius: lunar.radiusLarge
                width: parent.width

                Item {
                    anchors.fill: parent
                    anchors.margins: 16

                    ScrollView {
                        anchors.fill: parent
                        clip: true
                        visible: quickLook.previewController
                            && quickLook.previewController.kind === "text"

                        TextArea {
                            color: lunar.foreground
                            font.family: "monospace"
                            font.pixelSize: 13
                            readOnly: true
                            selectByMouse: true
                            text: quickLook.previewController
                                ? quickLook.previewController.textContent : ""
                            wrapMode: TextEdit.Wrap
                            background: null
                        }
                    }

                    Image {
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectFit
                        mipmap: true
                        smooth: true
                        source: quickLook.previewController
                            ? quickLook.previewController.imageDataUrl : ""
                        visible: quickLook.previewController
                            && quickLook.previewController.kind === "image"
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 12
                        visible: quickLook.previewController
                            && (quickLook.previewController.kind === "metadata"
                                || quickLook.previewController.kind === "error")
                        width: Math.min(440, parent.width - 32)

                        NorthstarIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            height: 72
                            iconName: quickLook.previewController
                                && quickLook.previewController.kind === "error"
                                ? "warning" : "file"
                            width: 72
                        }

                        Text {
                            color: quickLook.previewController
                                && quickLook.previewController.kind === "error"
                                ? lunar.danger : lunar.foreground
                            font.bold: true
                            font.pixelSize: 18
                            horizontalAlignment: Text.AlignHCenter
                            text: quickLook.previewController
                                && quickLook.previewController.kind === "error"
                                ? "Preview unavailable" : "File information"
                            width: parent.width
                        }

                        Text {
                            color: lunar.muted
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            text: quickLook.previewController
                                ? quickLook.previewController.message : ""
                            width: parent.width
                            wrapMode: Text.WordWrap
                        }
                    }

                    Column {
                        anchors.fill: parent
                        spacing: 10
                        visible: quickLook.previewController
                            && quickLook.previewController.kind === "folder"

                        Row {
                            spacing: 12
                            width: parent.width

                            NorthstarIcon {
                                height: 48
                                iconName: "files"
                                width: 48
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                color: lunar.muted
                                font.pixelSize: 13
                                text: quickLook.previewController
                                    ? (quickLook.previewController.message.length > 0
                                        ? quickLook.previewController.message
                                        : "Folder contents") : ""
                                width: parent.width - 60
                                wrapMode: Text.WordWrap
                            }
                        }

                        ListView {
                            clip: true
                            height: parent.height - 60
                            model: quickLook.previewController
                                ? quickLook.previewController.details : []
                            spacing: 5
                            width: parent.width

                            delegate: Rectangle {
                                required property string modelData
                                color: lunar.raised
                                height: 34
                                radius: lunar.radiusSmall
                                width: ListView.view.width

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.right: parent.right
                                    anchors.rightMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: lunar.foreground
                                    elide: Text.ElideMiddle
                                    font.pixelSize: 12
                                    text: modelData
                                }
                            }
                        }
                    }
                }
            }

            Row {
                spacing: 12
                width: parent.width

                Text {
                    color: lunar.muted
                    elide: Text.ElideMiddle
                    font.pixelSize: 11
                    text: quickLook.previewController ? quickLook.previewController.path : ""
                    width: Math.max(80, parent.width - closeButton.width - 12)
                }

                Rectangle {
                    id: closeButton
                    color: closeMouse.containsMouse ? lunar.accentSoft : lunar.raised
                    height: 32
                    radius: lunar.radiusSmall
                    width: 74

                    Text {
                        anchors.centerIn: parent
                        color: lunar.foreground
                        font.pixelSize: 12
                        text: "Close"
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: quickLook.hide()
                    }
                }
            }
        }
    }

    NativeWindowResizeHandler {
        resizingEnabled: !quickLook.maximized
        window: quickLook
    }
}
