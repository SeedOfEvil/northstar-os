import QtQuick
import QtTest
import Northstar.Ui 1.0

// Drives the settings control the way the Settings window does and asserts
// what it puts on screen.
//
// Every case here corresponds to something that reached interactive
// acceptance because the controller suites could not see it. A controller
// test proves the catalog holds the right value; only this layer can say
// whether the control shows it.
TestCase {
    id: testCase
    name: "SettingsEntryControl"
    when: windowShown
    width: 400
    height: 200

    // Stands in for the catalog: records writes and can refuse them the way
    // a controller does when it rejects a value.
    QtObject {
        id: stubCatalog
        property bool accept: true
        property var written: []

        function setValue(id, value) {
            if (!accept) {
                return false
            }
            written.push({ id: id, value: value })
            return true
        }
    }

    Component {
        id: controlComponent
        SettingsEntryControl {
            catalog: stubCatalog
        }
    }

    function makeChoice(value, allowsUnset) {
        return {
            id: "datetime.timezone",
            kind: "choice",
            available: true,
            allowsUnset: allowsUnset === true,
            value: value,
            options: [
                { value: "Canada/Atlantic", label: "Canada/Atlantic" },
                { value: "Canada/Mountain", label: "Canada/Mountain" },
                { value: "Canada/Pacific", label: "Canada/Pacific" }
            ]
        }
    }

    function init() {
        stubCatalog.accept = true
        stubCatalog.written = []
    }

    // The defect that shipped twice: the control opened reading "Not set"
    // whatever the entry held, because currentIndex was bound to an
    // expression the model was not part of.
    function test_choice_shows_the_value_it_was_given() {
        const control = createTemporaryObject(controlComponent, testCase,
                                              { entry: makeChoice("Canada/Mountain") })
        verify(control)
        compare(control.displayedText, "Canada/Mountain")
    }

    // Rebuilding the entry is what the catalog does on every refresh.
    function test_choice_follows_a_rebuilt_entry() {
        const control = createTemporaryObject(controlComponent, testCase,
                                              { entry: makeChoice("Canada/Mountain") })
        control.entry = makeChoice("Canada/Pacific")
        compare(control.displayedText, "Canada/Pacific")
    }

    // A genuinely unset choice says so rather than rendering an empty box.
    function test_choice_reports_an_unset_value() {
        const control = createTemporaryObject(controlComponent, testCase,
                                              { entry: makeChoice("", true) })
        compare(control.displayedText, "Not set")
    }

    // A value the entry does not offer must not be drawn as a selection.
    // This is what a timezone looked like while another region was browsed.
    function test_choice_does_not_invent_a_selection() {
        const entry = makeChoice("Europe/London", true)
        const control = createTemporaryObject(controlComponent, testCase, { entry: entry })
        compare(control.displayedText, "Not set")
        verify(control.displayedText !== "Canada/Atlantic")
    }

    function test_choice_writes_the_chosen_value() {
        const control = createTemporaryObject(controlComponent, testCase,
                                              { entry: makeChoice("Canada/Atlantic") })
        const combo = control.children[0].item
        verify(combo)
        combo.currentIndex = 2
        combo.activated(2)
        compare(stubCatalog.written.length, 1)
        compare(stubCatalog.written[0].id, "datetime.timezone")
        compare(stubCatalog.written[0].value, "Canada/Pacific")
    }

    // A refused write must not leave the rejected pick on screen, because the
    // system is not in that state.
    function test_choice_snaps_back_when_the_write_is_refused() {
        const control = createTemporaryObject(controlComponent, testCase,
                                              { entry: makeChoice("Canada/Atlantic") })
        stubCatalog.accept = false
        const combo = control.children[0].item
        combo.currentIndex = 2
        combo.activated(2)
        compare(control.displayedText, "Canada/Atlantic")
    }

    function test_toggle_shows_and_writes_its_state() {
        const control = createTemporaryObject(controlComponent, testCase, {
            entry: { id: "appearance.dark", kind: "toggle", available: true, value: true }
        })
        compare(control.displayedText, "On")

        const box = control.children[0].item
        box.toggle()
        box.toggled()
        compare(stubCatalog.written.length, 1)
        compare(stubCatalog.written[0].value, false)
    }

    function test_toggle_snaps_back_when_the_write_is_refused() {
        const control = createTemporaryObject(controlComponent, testCase, {
            entry: { id: "appearance.dark", kind: "toggle", available: true, value: true }
        })
        stubCatalog.accept = false
        const box = control.children[0].item
        box.toggle()
        box.toggled()
        compare(control.displayedText, "On")
    }

    // An unavailable control is shown, but cannot be operated.
    function test_unavailable_controls_cannot_be_operated() {
        const entry = makeChoice("Canada/Mountain")
        entry.available = false
        const control = createTemporaryObject(controlComponent, testCase, { entry: entry })
        compare(control.controlEnabled, false)
    }

    function test_path_shows_its_empty_label_then_its_value() {
        const control = createTemporaryObject(controlComponent, testCase, {
            entry: {
                id: "appearance.wallpaper",
                kind: "path",
                available: true,
                value: "",
                emptyLabel: "Built-in Northstar background"
            }
        })
        compare(control.displayedText, "Built-in Northstar background")

        control.entry = {
            id: "appearance.wallpaper",
            kind: "path",
            available: true,
            value: "/home/northstar/Pictures/aurora.png",
            emptyLabel: "Built-in Northstar background"
        }
        compare(control.displayedText, "/home/northstar/Pictures/aurora.png")
    }

    function test_path_asks_the_window_to_choose() {
        const control = createTemporaryObject(controlComponent, testCase, {
            entry: {
                id: "appearance.wallpaper",
                kind: "path",
                available: true,
                value: "",
                emptyLabel: "none"
            }
        })
        const asked = signalSpy.createObject(testCase,
                                             { target: control, signalName: "pathChooseRequested" })
        control.children[0].item.children[1].clicked()
        compare(asked.count, 1)
    }

    // Actions are handed to the window rather than performed here, because
    // destructive ones are confirmed there.
    function test_action_is_handed_to_the_window() {
        const control = createTemporaryObject(controlComponent, testCase, {
            entry: {
                id: "session.restart",
                kind: "action",
                available: true,
                actionLabel: "Restart shell"
            }
        })
        compare(control.displayedText, "Restart shell")

        const asked = signalSpy.createObject(testCase,
                                             { target: control, signalName: "actionRequested" })
        control.children[0].item.clicked()
        compare(asked.count, 1)
        compare(stubCatalog.written.length, 0)
    }

    function test_info_shows_the_value_as_read() {
        const control = createTemporaryObject(controlComponent, testCase, {
            entry: {
                id: "datetime.ntpstate",
                kind: "info",
                available: true,
                value: "Enabled but not running"
            }
        })
        compare(control.displayedText, "Enabled but not running")
    }

    function test_slider_reads_out_its_value_and_says_when_unavailable() {
        const control = createTemporaryObject(controlComponent, testCase, {
            entry: {
                id: "sound.volume",
                kind: "slider",
                available: true,
                value: 40,
                minimum: 0,
                maximum: 100,
                unit: "%"
            }
        })
        compare(control.displayedText, "40%")

        control.entry = {
            id: "sound.volume",
            kind: "slider",
            available: false,
            value: 40,
            minimum: 0,
            maximum: 100,
            unit: "%"
        }
        compare(control.displayedText, "—")
    }

    Component {
        id: signalSpy
        SignalSpy {}
    }
}
