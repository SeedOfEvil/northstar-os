import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: search
    objectName: "searchOverlay"

    LunarPalette {
        id: lunar
        darkMode: search.state ? search.state.darkMode : true
    }

    property var controller
    property var state
    property var targetScreen
    property int panelHeight: 44
    property int selectedIndex: controller && controller.results.length > 0 ? 0 : -1
    property int screenX: targetScreen ? targetScreen.geometry.x : 0
    property int screenY: targetScreen ? targetScreen.geometry.y : 0
    property int screenWidth: targetScreen ? targetScreen.geometry.width : 1280
    property color surfaceBackground: lunar.panelStrong
    property color surfaceForeground: lunar.foreground
    property color surfaceMuted: lunar.muted

    signal resultActivated()

    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint
    modality: Qt.NonModal
    title: "Northstar Search"
    width: Math.min(680, screenWidth - 40)
    height: Math.min(540, targetScreen ? targetScreen.geometry.height - panelHeight - 48 : 540)
    x: screenX + Math.round((screenWidth - width) / 2)
    y: screenY + panelHeight + 18

    function openSearch(initialQuery) {
        if (!controller) {
            return
        }
        controller.setQuery(String(initialQuery || ""))
        queryField.text = controller.query
        selectedIndex = controller.results.length > 0 ? 0 : -1
        show()
        raise()
        requestActivate()
        Qt.callLater(function() {
            queryField.forceActiveFocus()
            queryField.selectAll()
        })
    }

    function closeSearch() {
        hide()
        if (controller) {
            controller.clear()
        }
        queryField.text = ""
        selectedIndex = -1
    }

    function moveSelection(delta) {
        if (!controller || controller.results.length === 0) {
            selectedIndex = -1
            return
        }
        selectedIndex = Math.max(0, Math.min(controller.results.length - 1, selectedIndex + delta))
        resultsList.positionViewAtIndex(selectedIndex, ListView.Contain)
    }

    function activateSelection(index) {
        if (!controller || !controller.activateResult(index)) {
            return
        }
        closeSearch()
        resultActivated()
    }

    Connections {
        target: search.controller

        function onResultsChanged() {
            if (search.controller.results.length === 0) {
                search.selectedIndex = -1
            } else if (search.selectedIndex < 0
                       || search.selectedIndex >= search.controller.results.length) {
                search.selectedIndex = 0
            }
        }

        function onQueryChanged() {
            if (queryField.text !== search.controller.query) {
                queryField.text = search.controller.query
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: search.surfaceBackground
        radius: 22
        border.color: lunar.borderStrong
        border.width: 1

        gradient: Gradient {
            GradientStop { position: 0.0; color: lunar.panelStrong }
            GradientStop { position: 1.0; color: lunar.panel }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Rectangle {
                id: querySurface
                width: parent.width
                height: 52
                radius: 15
                color: lunar.field
                border.color: queryField.activeFocus ? lunar.accentBright : lunar.borderSoft
                border.width: 1

                NorthstarIcon {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    height: 24
                    iconName: "search"
                    width: 24
                }

                TextField {
                    id: queryField
                    anchors.fill: parent
                    leftPadding: 52
                    rightPadding: 78
                    color: search.surfaceForeground
                    font.pixelSize: 16
                    placeholderText: "Search apps, files, and actions"
                    placeholderTextColor: search.surfaceMuted
                    selectByMouse: true

                    background: Rectangle { color: "transparent" }

                    onTextChanged: {
                        if (search.controller && text !== search.controller.query) {
                            search.controller.setQuery(text)
                        }
                    }

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Down) {
                            search.moveSelection(1)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Up) {
                            search.moveSelection(-1)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            search.activateSelection(search.selectedIndex)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Escape) {
                            search.closeSearch()
                            event.accepted = true
                        }
                    }
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    color: search.surfaceMuted
                    font.pixelSize: 10
                    text: "ESC"
                }
            }

            Item {
                width: parent.width
                height: parent.height - querySurface.height - footer.height - (2 * parent.spacing)

                ListView {
                    id: resultsList
                    anchors.fill: parent
                    clip: true
                    currentIndex: search.selectedIndex
                    model: search.controller ? search.controller.results : []
                    spacing: 3

                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Item {
                        id: resultDelegate
                        required property int index
                        required property var modelData
                        readonly property bool beginsCategory: index === 0
                            || resultsList.model[index - 1].category !== modelData.category
                        width: resultsList.width
                        height: 62 + (beginsCategory ? 24 : 0)

                        Text {
                            visible: resultDelegate.beginsCategory
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.top: parent.top
                            color: search.surfaceMuted
                            font.bold: true
                            font.pixelSize: 10
                            text: resultDelegate.modelData.category.toUpperCase()
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 58
                            radius: 13
                            color: resultDelegate.index === search.selectedIndex
                                ? lunar.selection : resultMouse.containsMouse
                                    ? lunar.raisedHover : "transparent"

                            Item {
                                anchors.left: parent.left
                                anchors.leftMargin: 11
                                anchors.verticalCenter: parent.verticalCenter
                                height: 38
                                width: 38

                                Rectangle {
                                    anchors.fill: parent
                                    color: lunar.raised
                                    radius: 10
                                }

                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    asynchronous: true
                                    fillMode: Image.PreserveAspectFit
                                    mipmap: true
                                    smooth: true
                                    source: resultDelegate.modelData.iconSource || ""
                                    visible: String(source).length > 0
                                }

                                NorthstarIcon {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    iconName: resultDelegate.modelData.icon
                                    visible: !resultDelegate.modelData.iconSource
                                }
                            }

                            Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 60
                                anchors.right: activationHint.left
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 3

                                Text {
                                    width: parent.width
                                    color: search.surfaceForeground
                                    elide: Text.ElideRight
                                    font.bold: resultDelegate.index === search.selectedIndex
                                    font.pixelSize: 13
                                    text: resultDelegate.modelData.title
                                }
                                Text {
                                    width: parent.width
                                    color: search.surfaceMuted
                                    elide: Text.ElideMiddle
                                    font.pixelSize: 10
                                    text: resultDelegate.modelData.subtitle
                                }
                            }

                            Text {
                                id: activationHint
                                anchors.right: parent.right
                                anchors.rightMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                color: search.surfaceMuted
                                font.pixelSize: 10
                                text: resultDelegate.index === search.selectedIndex ? "ENTER" : ""
                                width: 44
                            }

                            MouseArea {
                                id: resultMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: search.selectedIndex = resultDelegate.index
                                onClicked: search.activateSelection(resultDelegate.index)
                            }
                        }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    visible: search.controller && search.controller.results.length === 0

                    NorthstarIcon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        height: 38
                        iconName: "search"
                        opacity: 0.65
                        width: 38
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: search.surfaceMuted
                        text: search.controller && search.controller.searching
                            ? "Searching your Northstar home folder..."
                            : "No matching apps, files, or actions"
                    }
                }
            }

            Row {
                id: footer
                width: parent.width
                height: 24
                spacing: 12

                Text {
                    color: search.surfaceMuted
                    font.pixelSize: 10
                    text: search.controller && search.controller.searching
                        ? "Searching files..." : "Up/Down select   Enter open   Esc close"
                }

                Item { width: Math.max(0, parent.width - 390); height: 1 }

                Text {
                    color: search.surfaceMuted
                    font.pixelSize: 10
                    text: "Home-scoped and command-safe"
                }
            }
        }
    }
}
