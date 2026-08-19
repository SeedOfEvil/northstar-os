import QtQuick
import QtQuick.Controls

// Renders one settings entry as the control its kind calls for.
//
// This lives in the shared UI module rather than inside the Settings window
// so it can be driven directly by a test. Three defects reached interactive
// acceptance because nothing exercised this layer: a choice that opened
// reading "Not set" whatever its value, a value the control declined to
// offer, and a refused write left on screen as though it had taken. All of
// them were in what the control displayed, not in what the catalog held, and
// a controller test cannot see the difference.
Item {
    id: control

    // The entry map as the catalog describes it.
    property var entry

    // Anything with setValue(id, value) returning bool. The real catalog, or
    // a stand-in.
    property var catalog

    property color foregroundColor: "#f2f4f8"
    property color mutedColor: "#9aa4b2"

    // Actions and file choosing are the window's business: one confirms
    // destructive entries, the other owns the picker.
    signal actionRequested(var entry)
    signal pathChooseRequested(var entry)

    // What the surface is currently showing, so a test can assert against the
    // same string a person would read.
    readonly property string displayedText: {
        if (!loader.item) {
            return ""
        }
        if (control.entry.kind === "choice") {
            return loader.item.displayText
        }
        if (control.entry.kind === "toggle") {
            return loader.item.text
        }
        if (control.entry.kind === "path" || control.entry.kind === "slider") {
            return loader.item.readout
        }
        if (control.entry.kind === "action") {
            return loader.item.text
        }
        return loader.item.text
    }

    readonly property bool controlEnabled: loader.item ? loader.item.enabled : false

    implicitHeight: 34
    implicitWidth: entry && entry.kind === "slider" ? 210
        : entry && entry.kind === "path" ? 230 : 150

    function writeValue(value) {
        return control.catalog ? control.catalog.setValue(control.entry.id, value) : false
    }

    // A rebuilt entry can carry a new value with an identical option list, in
    // which case the model never changes and nothing else would re-read the
    // selection. Syncing on the entry itself covers that; the surface
    // showing a stale selection is the same defect as showing none.
    onEntryChanged: {
        if (loader.item && loader.item.syncToEntry) {
            loader.item.syncToEntry()
        }
    }

    Loader {
        id: loader
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        sourceComponent: !control.entry ? null
            : control.entry.kind === "toggle" ? toggleControl
            : control.entry.kind === "slider" ? sliderControl
            : control.entry.kind === "choice" ? choiceControl
            : control.entry.kind === "path" ? pathControl
            : control.entry.kind === "action" ? actionControl
            : infoControl
    }

    Component {
        id: toggleControl

        CheckBox {
            checked: control.entry.value === true
            enabled: control.entry.available
            text: checked ? "On" : "Off"
            onToggled: {
                // A refused write must not leave the box showing a state the
                // system is not in.
                if (!control.writeValue(checked)) {
                    checked = control.entry.value === true
                }
            }
        }
    }

    Component {
        id: sliderControl

        Row {
            spacing: 10
            property alias readout: sliderReadout.text

            Slider {
                id: valueSlider
                anchors.verticalCenter: parent.verticalCenter
                enabled: control.entry.available
                from: control.entry.minimum
                to: control.entry.maximum
                stepSize: 1
                value: control.entry.value
                width: 150
                onMoved: {
                    if (!pressed) {
                        control.writeValue(Math.round(value))
                    }
                }
                onPressedChanged: {
                    if (!pressed) {
                        control.writeValue(Math.round(value))
                    }
                }
            }

            Text {
                id: sliderReadout
                anchors.verticalCenter: parent.verticalCenter
                color: control.foregroundColor
                font.pixelSize: 12
                horizontalAlignment: Text.AlignRight
                text: control.entry.available
                    ? Math.round(valueSlider.value) + control.entry.unit
                    : "—"
                width: 44
            }
        }
    }

    Component {
        id: actionControl

        Button {
            enabled: control.entry.available
            text: control.entry.actionLabel
            onClicked: control.actionRequested(control.entry)
        }
    }

    Component {
        id: choiceControl

        ComboBox {
            id: choiceBox
            enabled: control.entry.available
            model: control.entry.options
            textRole: "label"
            valueRole: "value"
            width: 150

            // An unset choice would otherwise render as an empty box that
            // looks broken rather than unanswered.
            displayText: currentIndex < 0 ? "Not set" : currentText

            // Not a binding on currentIndex. The answer depends on the model
            // as well as the value, and the model is not part of that
            // expression, so a binding evaluated before the model arrived
            // stayed at -1 and every control opened reading "Not set".
            function syncToEntry() {
                currentIndex = indexOfValue(control.entry.value)
            }

            Component.onCompleted: syncToEntry()
            onModelChanged: syncToEntry()

            onActivated: {
                // A refused write leaves the list showing the rejected pick,
                // so it is put back to what is actually in effect. On a write
                // that succeeds the catalog rebuilds this delegate instead.
                if (!control.writeValue(valueAt(currentIndex))) {
                    syncToEntry()
                }
            }
        }
    }

    Component {
        id: pathControl

        Row {
            spacing: 8
            property alias readout: pathReadout.text
            property alias enabled: chooseButton.enabled

            Text {
                id: pathReadout
                anchors.verticalCenter: parent.verticalCenter
                color: control.mutedColor
                elide: Text.ElideLeft
                font.pixelSize: 12
                horizontalAlignment: Text.AlignRight
                text: control.entry.value !== "" ? control.entry.value : control.entry.emptyLabel
                width: 130
            }

            Button {
                id: chooseButton
                anchors.verticalCenter: parent.verticalCenter
                enabled: control.entry.available
                text: "Choose..."
                onClicked: control.pathChooseRequested(control.entry)
            }
        }
    }

    Component {
        id: infoControl

        Text {
            color: control.foregroundColor
            elide: Text.ElideRight
            font.pixelSize: 12
            horizontalAlignment: Text.AlignRight
            text: control.entry.value
            width: 150
        }
    }
}
