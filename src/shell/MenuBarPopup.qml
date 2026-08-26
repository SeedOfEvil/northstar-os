import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

Window {
    id: popup

    LunarPalette {
        id: lunar
        darkMode: popup.darkMode
    }

    property bool darkMode: true
    property string menuName: ""
    property var menuItems: []
    property var ownerWindow
    property int panelHeight: 46
    property int screenX: 0
    property int screenY: 0
    property int screenWidth: 1920
    property int selectedIndex: firstEnabledIndex()
    readonly property int rowHeight: 31
    readonly property int separatorHeight: 9

    signal actionRequested(string action)
    signal dismissed()

    visible: false
    color: "transparent"
    flags: Qt.Popup | Qt.FramelessWindowHint
    modality: Qt.NonModal
    title: menuName.length > 0 ? menuName + " menu" : "Northstar menu"
    transientParent: ownerWindow
    width: 286
    height: menuHeight() + 16

    function itemEnabled(item) {
        return item && item.kind !== "separator" && item.enabled !== false
    }

    function firstEnabledIndex() {
        for (let index = 0; index < menuItems.length; ++index) {
            if (itemEnabled(menuItems[index]))
                return index
        }
        return -1
    }

    function menuHeight() {
        let result = 0
        for (let index = 0; index < menuItems.length; ++index)
            result += menuItems[index].kind === "separator" ? separatorHeight : rowHeight
        return result
    }

    function openMenu(name, items, anchorX) {
        menuName = name
        menuItems = items || []
        selectedIndex = firstEnabledIndex()
        x = Math.max(screenX + 6, Math.min(anchorX, screenX + screenWidth - width - 6))
        y = screenY + panelHeight + 3
        show()
        raise()
        requestActivate()
        menuList.forceActiveFocus()
    }

    function closeMenu() {
        if (!visible)
            return
        hide()
    }

    function moveSelection(direction) {
        if (menuItems.length === 0)
            return
        let index = selectedIndex
        for (let attempt = 0; attempt < menuItems.length; ++attempt) {
            index = (index + direction + menuItems.length) % menuItems.length
            if (itemEnabled(menuItems[index])) {
                selectedIndex = index
                menuList.positionViewAtIndex(index, ListView.Contain)
                return
            }
        }
    }

    function activateIndex(index) {
        if (index < 0 || index >= menuItems.length || !itemEnabled(menuItems[index]))
            return
        const action = menuItems[index].action || ""
        if (action.length === 0)
            return
        hide()
        actionRequested(action)
    }

    onVisibleChanged: if (!visible) dismissed()

    Rectangle {
        anchors.fill: parent
        antialiasing: true
        border.color: lunar.border
        border.width: 1
        color: lunar.panelStrong
        radius: 12

        gradient: Gradient {
            GradientStop { position: 0.0; color: lunar.panelStrong }
            GradientStop { position: 1.0; color: lunar.panel }
        }

        ListView {
            id: menuList
            anchors.fill: parent
            anchors.margins: 8
            clip: true
            currentIndex: popup.selectedIndex
            interactive: contentHeight > height
            model: popup.menuItems

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Down) {
                    popup.moveSelection(1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Up) {
                    popup.moveSelection(-1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    popup.activateIndex(popup.selectedIndex)
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape) {
                    popup.closeMenu()
                    event.accepted = true
                }
            }

            delegate: Item {
                id: menuItem
                required property int index
                required property var modelData
                readonly property bool separator: modelData.kind === "separator"
                readonly property bool available: popup.itemEnabled(modelData)

                height: separator ? popup.separatorHeight : popup.rowHeight
                width: menuList.width

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    color: lunar.borderSoft
                    height: 1
                    opacity: 0.72
                    visible: menuItem.separator
                }

                Rectangle {
                    anchors.fill: parent
                    color: menuItem.index === popup.selectedIndex && menuItem.available
                        ? lunar.accentSoft : "transparent"
                    radius: 7
                    visible: !menuItem.separator
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    color: menuItem.available ? lunar.foreground : lunar.muted
                    font.pixelSize: 13
                    text: menuItem.modelData.checked === true ? "✓" : ""
                    visible: !menuItem.separator
                    width: 18
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 34
                    anchors.right: shortcutLabel.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    color: menuItem.available ? lunar.foreground : lunar.muted
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    text: menuItem.separator ? "" : menuItem.modelData.label || ""
                }

                Text {
                    id: shortcutLabel
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    color: menuItem.available ? lunar.muted : Qt.darker(lunar.muted, 1.2)
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight
                    text: menuItem.separator ? "" : menuItem.modelData.shortcut || ""
                    width: Math.max(56, implicitWidth)
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: !menuItem.separator
                    hoverEnabled: true
                    onEntered: if (menuItem.available) popup.selectedIndex = menuItem.index
                    onClicked: popup.activateIndex(menuItem.index)
                }
            }
        }
    }
}
