import QtQuick
import QtQuick.Controls
import Northstar.Ui 1.0

ApplicationWindow {
    id: editor

    property var documentController: northstarTextEditorController
    LunarPalette { id: lunar; darkMode: northstarDarkMode }

    property color backgroundColor: lunar.background
    property color panelColor: lunar.panel
    property color raisedColor: lunar.raised
    property color foregroundColor: lunar.foreground
    property color mutedColor: lunar.muted
    property color accentColor: lunar.accent
    property bool syncingText: false
    property bool closeConfirmed: false
    property bool closingWindow: false
    property bool findVisible: false
    property int pendingCloseIndex: -1

    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    height: 700
    minimumHeight: 460
    minimumWidth: 820
    title: editor.documentController
        ? "Northstar Text Editor — " + editor.documentController.documentTitle
        : "Northstar Text Editor"
    visible: true
    width: 980

    Component.onCompleted: {
        raise()
        requestActivate()
    }

    // --- Document actions --------------------------------------------------

    function saveDocument() {
        if (!editor.documentController) {
            return false
        }
        if (editor.documentController.untitled) {
            editor.openSaveAsDialog()
            return false
        }
        if (editor.documentController.save()) {
            return true
        }
        if (editor.documentController.externallyModified) {
            externalChangeDialog.open()
        }
        return false
    }

    function openSaveAsDialog() {
        saveAsDialog.validationMessage = ""
        saveAsNameField.text = editor.documentController && !editor.documentController.untitled
            ? editor.documentController.documentTitle : ""
        saveAsDialog.open()
    }

    function openOpenDialog() {
        editor.documentController.refreshBrowse()
        openDialog.open()
    }

    function requestCloseTab(index) {
        if (!editor.documentController) {
            return
        }
        if (editor.documentController.closeDocument(index)) {
            return
        }
        editor.documentController.activateDocument(index)
        editor.pendingCloseIndex = index
        unsavedDialog.targetIndex = index
        unsavedDialog.open()
    }

    // Window close walks every unsaved document rather than asking once about
    // all of them, so the user decides per document exactly as on a tab close.
    function continueClosing() {
        if (!editor.closingWindow) {
            return
        }
        const nextDirty = editor.documentController.firstDirtyIndex
        if (nextDirty < 0) {
            editor.closingWindow = false
            editor.closeConfirmed = true
            editor.close()
            return
        }
        editor.documentController.activateDocument(nextDirty)
        editor.pendingCloseIndex = -1
        unsavedDialog.targetIndex = nextDirty
        unsavedDialog.open()
    }

    function toggleFind(showReplace) {
        editor.findVisible = true
        if (showReplace) {
            replaceField.forceActiveFocus()
        } else {
            findField.forceActiveFocus()
            findField.selectAll()
        }
    }

    function closeFind() {
        editor.findVisible = false
        textArea.forceActiveFocus()
    }

    onClosing: function(close) {
        if (editor.closeConfirmed || !editor.documentController
            || !editor.documentController.anyDirty) {
            close.accepted = true
            return
        }
        close.accepted = false
        editor.closingWindow = true
        editor.continueClosing()
    }

    // --- Keyboard ----------------------------------------------------------

    Shortcut {
        sequence: StandardKey.New
        onActivated: editor.documentController.newDocument()
    }
    Shortcut {
        sequence: StandardKey.Open
        onActivated: editor.openOpenDialog()
    }
    Shortcut {
        sequence: StandardKey.Save
        onActivated: editor.saveDocument()
    }
    Shortcut {
        sequence: StandardKey.SaveAs
        onActivated: editor.openSaveAsDialog()
    }
    Shortcut {
        sequence: StandardKey.Close
        onActivated: editor.requestCloseTab(editor.documentController.activeIndex)
    }
    Shortcut {
        sequence: StandardKey.Find
        onActivated: editor.toggleFind(false)
    }
    Shortcut {
        sequence: StandardKey.Replace
        onActivated: editor.toggleFind(true)
    }
    Shortcut {
        sequence: StandardKey.FindNext
        onActivated: editor.documentController.findNext()
    }
    Shortcut {
        sequence: StandardKey.FindPrevious
        onActivated: editor.documentController.findPrevious()
    }
    Shortcut {
        sequence: "Ctrl+Tab"
        onActivated: editor.documentController.activateDocument(
            (editor.documentController.activeIndex + 1) % editor.documentController.documentCount)
    }
    Shortcut {
        sequence: "Ctrl+Shift+Tab"
        onActivated: editor.documentController.activateDocument(
            (editor.documentController.activeIndex + editor.documentController.documentCount - 1)
                % editor.documentController.documentCount)
    }
    Shortcut {
        sequence: "Escape"
        enabled: editor.findVisible
        onActivated: editor.closeFind()
    }

    // --- Controller wiring -------------------------------------------------

    Connections {
        target: editor.documentController

        function onStateChanged() {
            if (editor.documentController && textArea.text !== editor.documentController.text) {
                editor.syncingText = true
                textArea.text = editor.documentController.text
                editor.syncingText = false
            }
        }

        function onSelectionRequested(anchor, cursor) {
            textArea.forceActiveFocus()
            textArea.select(anchor, cursor)
        }
    }

    background: NorthstarWindowFrame {
        darkMode: lunar.darkMode
    }

    // --- Surface -----------------------------------------------------------

    Item {
        anchors.fill: parent
        anchors.margins: 24

        NorthstarWindowTitleBar {
            id: titleBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            closeDestroysWindow: true
            iconSource: "qrc:/Northstar/TextEditor/northstar-text-editor.svg"
            maximized: editor.visibility === Window.Maximized
            lunarPalette: lunar
            subtitle: editor.documentController
                ? (editor.documentController.dirty
                    ? "Unsaved changes — " + editor.documentController.displayPath
                    : editor.documentController.displayPath)
                : "Northstar Text Editor"
            title: editor.documentController
                ? editor.documentController.documentTitle : "Untitled document"
            window: editor
            onMaximizeRequested: {
                if (editor.visibility === Window.Maximized) editor.showNormal()
                else editor.showMaximized()
            }
        }

        Row {
            id: toolbar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: titleBar.bottom
            anchors.topMargin: 6
            height: 36
            spacing: 8

            Button {
                text: "New"
                onClicked: editor.documentController.newDocument()
            }
            Button {
                text: "Open..."
                onClicked: editor.openOpenDialog()
            }
            Button {
                id: recentButton
                enabled: editor.documentController && editor.documentController.hasRecentFiles
                text: "Open Recent"
                onClicked: recentPopup.open()
            }
            Button {
                enabled: editor.documentController && editor.documentController.canSave
                text: "Save"
                onClicked: editor.saveDocument()
            }
            Button {
                text: "Save As..."
                onClicked: editor.openSaveAsDialog()
            }
            Button {
                text: editor.findVisible ? "Hide Find" : "Find..."
                onClicked: {
                    if (editor.findVisible) editor.closeFind()
                    else editor.toggleFind(false)
                }
            }
        }

        // --- Document tabs -------------------------------------------------

        Rectangle {
            id: tabBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: toolbar.bottom
            anchors.topMargin: 10
            border.color: lunar.borderSoft
            border.width: 1
            color: lunar.field
            height: 42
            radius: lunar.radiusMedium

            Row {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                ListView {
                    id: documentTabs
                    anchors.verticalCenter: parent.verticalCenter
                    clip: true
                    height: 34
                    model: editor.documentController ? editor.documentController.documents : []
                    orientation: ListView.Horizontal
                    spacing: 6
                    width: parent.width - newTabButton.width - parent.spacing

                    delegate: Rectangle {
                        required property var modelData
                        required property int index

                        color: index === editor.documentController.activeIndex
                            ? editor.accentColor
                            : tabMouse.containsMouse ? editor.raisedColor : lunar.panelStrong
                        height: 34
                        radius: 7
                        width: Math.min(210, Math.max(124, tabTitle.implicitWidth + 64))

                        Rectangle {
                            id: dirtyMarker
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            color: lunar.warning
                            height: 8
                            radius: 4
                            visible: modelData.dirty
                            width: 8
                        }

                        Text {
                            id: tabTitle
                            anchors.left: parent.left
                            anchors.leftMargin: modelData.dirty ? 24 : 12
                            anchors.right: closeTabButton.left
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            color: editor.foregroundColor
                            elide: Text.ElideRight
                            font.italic: modelData.missing
                            font.pixelSize: 11
                            text: modelData.title
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: editor.documentController.activateDocument(index)
                        }

                        Rectangle {
                            id: closeTabButton
                            anchors.right: parent.right
                            anchors.rightMargin: 5
                            anchors.verticalCenter: parent.verticalCenter
                            color: closeTabMouse.containsMouse ? lunar.danger : "transparent"
                            height: 22
                            radius: 11
                            width: 22

                            Text {
                                anchors.centerIn: parent
                                color: editor.foregroundColor
                                font.pixelSize: 12
                                text: "×"
                            }

                            MouseArea {
                                id: closeTabMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: function(mouse) {
                                    mouse.accepted = true
                                    editor.requestCloseTab(index)
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: newTabButton
                    anchors.verticalCenter: parent.verticalCenter
                    color: newTabMouse.containsMouse ? editor.accentColor : editor.raisedColor
                    height: 32
                    radius: 7
                    width: 38

                    Text {
                        anchors.centerIn: parent
                        color: editor.foregroundColor
                        font.pixelSize: 18
                        text: "+"
                    }

                    MouseArea {
                        id: newTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: editor.documentController.newDocument()
                    }
                }
            }
        }

        // --- Find and replace ----------------------------------------------

        Rectangle {
            id: findBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: tabBar.bottom
            anchors.topMargin: editor.findVisible ? 10 : 0
            border.color: lunar.borderSoft
            border.width: 1
            color: lunar.field
            height: editor.findVisible ? 52 : 0
            radius: lunar.radiusMedium
            visible: editor.findVisible

            // The buttons keep their natural size and the two entry fields
            // absorb whatever is left, so the row never overflows the window.
            readonly property int fixedWidth: previousMatchButton.width + nextMatchButton.width
                + replaceMatchButton.width + replaceAllButton.width + caseSensitiveBox.width
                + findSummaryLabel.width + (7 * findRow.spacing)
            readonly property int fieldWidth: Math.max(
                110, (findBar.width - 20 - findBar.fixedWidth) / 2)

            Row {
                id: findRow
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                TextField {
                    id: findField
                    anchors.verticalCenter: parent.verticalCenter
                    placeholderText: "Find"
                    selectByMouse: true
                    width: findBar.fieldWidth
                    onAccepted: editor.documentController.findNext()
                    onTextChanged: editor.documentController.setFindQuery(text)
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return && (event.modifiers & Qt.ShiftModifier)) {
                            editor.documentController.findPrevious()
                            event.accepted = true
                        }
                    }
                }

                TextField {
                    id: replaceField
                    anchors.verticalCenter: parent.verticalCenter
                    placeholderText: "Replace with"
                    selectByMouse: true
                    width: findBar.fieldWidth
                    onAccepted: editor.documentController.replaceCurrent()
                    onTextChanged: editor.documentController.setReplacementText(text)
                }

                Button {
                    id: previousMatchButton
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Prev"
                    onClicked: editor.documentController.findPrevious()
                }
                Button {
                    id: nextMatchButton
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Next"
                    onClicked: editor.documentController.findNext()
                }
                Button {
                    id: replaceMatchButton
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Replace"
                    onClicked: editor.documentController.replaceCurrent()
                }
                Button {
                    id: replaceAllButton
                    anchors.verticalCenter: parent.verticalCenter
                    text: "All"
                    onClicked: editor.documentController.replaceAll()
                }

                CheckBox {
                    id: caseSensitiveBox
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Aa"
                    onToggled: editor.documentController.setFindCaseSensitive(checked)
                }

                Text {
                    id: findSummaryLabel
                    anchors.verticalCenter: parent.verticalCenter
                    color: editor.mutedColor
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignRight
                    text: editor.documentController ? editor.documentController.findSummary : ""
                    width: 96
                }
            }
        }

        // --- Status --------------------------------------------------------

        Row {
            id: statusRow
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 34
            spacing: 12

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: editor.documentController && editor.documentController.statusIsError
                    ? lunar.danger : editor.mutedColor
                elide: Text.ElideRight
                font.pixelSize: 12
                text: editor.documentController ? editor.documentController.statusMessage : ""
                width: parent.width - savedStateLabel.width - closeButton.width - 24
            }

            Text {
                id: savedStateLabel
                anchors.verticalCenter: parent.verticalCenter
                color: editor.documentController && editor.documentController.dirty
                    ? lunar.warning : lunar.success
                font.bold: true
                font.pixelSize: 12
                text: editor.documentController
                    ? (editor.documentController.dirty ? "Modified" : "Saved") : ""
            }

            Button {
                id: closeButton
                text: "Close"
                onClicked: editor.close()
            }
        }

        // --- Editing surface -----------------------------------------------

        Rectangle {
            id: editorSurface
            anchors.bottom: statusRow.top
            anchors.bottomMargin: 10
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: findBar.bottom
            anchors.topMargin: 10
            border.color: editor.documentController && editor.documentController.missingOnDisk
                ? lunar.danger : lunar.borderSoft
            border.width: 1
            color: editor.raisedColor
            radius: 14

            ScrollView {
                anchors.fill: parent
                anchors.margins: 12
                clip: true

                TextArea {
                    id: textArea
                    color: editor.foregroundColor
                    font.family: "monospace"
                    font.pixelSize: 14
                    placeholderText: "Start typing..."
                    placeholderTextColor: editor.mutedColor
                    selectByMouse: true
                    selectionColor: editor.accentColor
                    text: editor.documentController ? editor.documentController.text : ""
                    wrapMode: TextArea.Wrap

                    onCursorPositionChanged: {
                        if (editor.documentController) {
                            editor.documentController.setCursorPosition(cursorPosition)
                        }
                    }

                    onTextChanged: {
                        if (!editor.syncingText && editor.documentController) {
                            editor.documentController.setText(text)
                        }
                    }
                }
            }
        }
    }

    // --- Open Recent -------------------------------------------------------

    Popup {
        id: recentPopup
        anchors.centerIn: parent
        modal: true
        padding: 10
        width: 460

        background: Rectangle {
            border.color: lunar.borderSoft
            border.width: 1
            color: lunar.panelStrong
            radius: lunar.radiusMedium
        }

        contentItem: Column {
            spacing: 8

            Text {
                color: editor.foregroundColor
                font.bold: true
                font.pixelSize: 14
                text: "Open Recent"
            }

            ListView {
                clip: true
                height: Math.min(260, contentHeight)
                model: editor.documentController ? editor.documentController.recentFiles : []
                spacing: 4
                width: recentPopup.width - (2 * recentPopup.padding)

                delegate: Rectangle {
                    required property var modelData
                    required property int index

                    color: recentMouse.containsMouse ? editor.raisedColor : "transparent"
                    height: 42
                    radius: 6
                    width: ListView.view.width

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.right: forgetButton.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 1

                        Text {
                            color: modelData.available ? editor.foregroundColor : lunar.danger
                            elide: Text.ElideRight
                            font.pixelSize: 12
                            text: modelData.available
                                ? modelData.name : modelData.name + " (missing)"
                            width: parent.width
                        }
                        Text {
                            color: editor.mutedColor
                            elide: Text.ElideLeft
                            font.pixelSize: 10
                            text: modelData.directory
                            width: parent.width
                        }
                    }

                    MouseArea {
                        id: recentMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            recentPopup.close()
                            editor.documentController.openRecent(index)
                        }
                    }

                    Button {
                        id: forgetButton
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Remove"
                        onClicked: editor.documentController.forgetRecent(index)
                    }
                }
            }

            Row {
                spacing: 8

                Button {
                    text: "Clear History"
                    onClicked: {
                        editor.documentController.clearRecentFiles()
                        recentPopup.close()
                    }
                }
                Button {
                    text: "Done"
                    onClicked: recentPopup.close()
                }
            }
        }
    }

    // --- Open ---------------------------------------------------------------

    Dialog {
        id: openDialog
        anchors.centerIn: parent
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel
        title: "Open a text file"
        width: 560

        contentItem: Column {
            spacing: 10
            width: openDialog.width - (2 * openDialog.padding)

            Row {
                spacing: 8
                width: parent.width

                Button {
                    text: "Home"
                    onClicked: editor.documentController.browseHome()
                }
                Button {
                    enabled: editor.documentController
                        && editor.documentController.browseCanNavigateUp
                    text: "Up"
                    onClicked: editor.documentController.browseUp()
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: editor.mutedColor
                    elide: Text.ElideLeft
                    font.pixelSize: 12
                    text: editor.documentController
                        ? editor.documentController.browseDisplayPath : ""
                    width: parent.width - 160
                }
            }

            ListView {
                clip: true
                height: 300
                model: editor.documentController ? editor.documentController.browseEntries : []
                spacing: 3
                width: parent.width

                delegate: Rectangle {
                    required property var modelData

                    color: browseMouse.containsMouse ? editor.raisedColor : "transparent"
                    height: 32
                    radius: 6
                    width: ListView.view.width

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.right: browseDetail.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        color: modelData.openable ? editor.foregroundColor : editor.mutedColor
                        elide: Text.ElideRight
                        font.bold: modelData.isDirectory
                        font.pixelSize: 12
                        text: modelData.isDirectory ? modelData.name + "/" : modelData.name
                    }

                    Text {
                        id: browseDetail
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        color: modelData.reason.length > 0 ? lunar.danger : editor.mutedColor
                        font.pixelSize: 10
                        text: modelData.reason.length > 0 ? modelData.reason : modelData.size
                    }

                    MouseArea {
                        id: browseMouse
                        anchors.fill: parent
                        enabled: modelData.openable
                        hoverEnabled: true
                        onClicked: {
                            if (modelData.isDirectory) {
                                editor.documentController.browseTo(modelData.path)
                            } else if (editor.documentController.openFile(modelData.path)) {
                                openDialog.close()
                            }
                        }
                    }
                }
            }

            Text {
                color: lunar.warning
                font.pixelSize: 11
                text: "This folder has more entries than the browser lists."
                visible: editor.documentController && editor.documentController.browseTruncated
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Text {
                color: editor.documentController && editor.documentController.statusIsError
                    ? lunar.danger : editor.mutedColor
                font.pixelSize: 11
                text: editor.documentController ? editor.documentController.statusMessage : ""
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
    }

    // --- Save As -------------------------------------------------------------

    Dialog {
        id: saveAsDialog
        anchors.centerIn: parent
        modal: true
        padding: 16
        standardButtons: Dialog.Cancel | Dialog.Ok
        title: "Save document as"
        width: 460

        property string validationMessage: ""

        contentItem: Column {
            spacing: 12
            width: saveAsDialog.width - (2 * saveAsDialog.padding)

            Text {
                color: editor.foregroundColor
                text: "Enter a file name, or a full path beginning with /."
                wrapMode: Text.WordWrap
                width: parent.width
            }

            TextField {
                id: saveAsNameField
                placeholderText: "File name"
                selectByMouse: true
                width: parent.width
                onAccepted: saveAsDialog.accept()
            }

            Text {
                color: editor.mutedColor
                elide: Text.ElideLeft
                font.pixelSize: 11
                text: "Saves in " + (editor.documentController
                    ? (editor.documentController.untitled
                        ? editor.documentController.defaultSaveDirectory
                        : editor.documentController.displayPath)
                    : "")
                width: parent.width
            }

            Text {
                color: lunar.danger
                text: saveAsDialog.validationMessage
                visible: text.length > 0
                wrapMode: Text.WordWrap
                width: parent.width
            }
        }

        onOpened: {
            saveAsNameField.forceActiveFocus()
            saveAsNameField.selectAll()
        }

        onAccepted: {
            const requested = editor.documentController.suggestedSavePath(saveAsNameField.text)
            if (requested.length === 0) {
                saveAsDialog.validationMessage = "Enter a file name."
                Qt.callLater(function() { saveAsDialog.open() })
                return
            }
            if (!editor.documentController.saveAs(requested)) {
                saveAsDialog.validationMessage = editor.documentController.statusMessage
                Qt.callLater(function() { saveAsDialog.open() })
                return
            }
            saveAsDialog.validationMessage = ""
            if (editor.closingWindow) {
                editor.continueClosing()
            } else if (editor.pendingCloseIndex >= 0) {
                editor.documentController.closeDocument(editor.pendingCloseIndex)
                editor.pendingCloseIndex = -1
            }
        }

        onRejected: {
            editor.closingWindow = false
            editor.pendingCloseIndex = -1
            saveAsDialog.validationMessage = ""
        }
    }

    // --- Unsaved changes ------------------------------------------------------

    Dialog {
        id: unsavedDialog
        anchors.centerIn: parent
        modal: true
        padding: 16
        standardButtons: Dialog.NoButton
        title: "Save changes?"
        width: 460

        property int targetIndex: -1

        contentItem: Column {
            spacing: 14
            width: unsavedDialog.width - (2 * unsavedDialog.padding)

            Text {
                color: editor.foregroundColor
                text: editor.documentController
                    ? "\"" + editor.documentController.documentTitle
                        + "\" has unsaved changes. Save them before closing it?"
                    : "This document has unsaved changes."
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Row {
                spacing: 8

                Button {
                    text: editor.documentController && editor.documentController.untitled
                        ? "Save As..." : "Save"
                    onClicked: {
                        unsavedDialog.close()
                        if (editor.documentController.untitled) {
                            editor.openSaveAsDialog()
                            return
                        }
                        if (editor.saveDocument()) {
                            if (editor.closingWindow) {
                                editor.continueClosing()
                            } else {
                                editor.documentController.closeDocument(unsavedDialog.targetIndex)
                            }
                        }
                    }
                }

                Button {
                    text: "Discard"
                    onClicked: {
                        unsavedDialog.close()
                        editor.documentController.discardDocument(unsavedDialog.targetIndex)
                        editor.continueClosing()
                    }
                }

                Button {
                    text: "Cancel"
                    onClicked: {
                        editor.closingWindow = false
                        editor.pendingCloseIndex = -1
                        unsavedDialog.close()
                    }
                }
            }
        }
    }

    // --- External modification ------------------------------------------------

    Dialog {
        id: externalChangeDialog
        anchors.centerIn: parent
        modal: true
        padding: 16
        standardButtons: Dialog.NoButton
        title: "This file changed on disk"
        width: 460

        contentItem: Column {
            spacing: 14
            width: externalChangeDialog.width - (2 * externalChangeDialog.padding)

            Text {
                color: editor.foregroundColor
                text: "Another program changed this file after it was opened here. "
                    + "Overwrite it with the version in this window, or reload the version on disk "
                    + "and lose the edits made here?"
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Row {
                spacing: 8

                Button {
                    text: "Overwrite"
                    onClicked: {
                        externalChangeDialog.close()
                        if (editor.documentController.saveOverwritingExternalChanges()) {
                            editor.continueClosing()
                        }
                    }
                }
                Button {
                    text: "Reload From Disk"
                    onClicked: {
                        externalChangeDialog.close()
                        editor.documentController.reloadDocument()
                    }
                }
                Button {
                    text: "Cancel"
                    onClicked: {
                        editor.closingWindow = false
                        externalChangeDialog.close()
                    }
                }
            }
        }
    }

    NativeWindowResizeHandler {
        resizingEnabled: editor.visibility !== Window.Maximized
        window: editor
    }
}
