# Testing what the controls put on screen — 2026-08-19

PR #109, verified on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2, Qt 6.11.1) at
commit `5df4bd8`, built in `/home/northstar/builds/pr109-5df4bd8` with the
project's canonical `-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.

## Why

Three defects reached interactive acceptance in two days, and every automated
suite passed through all three:

- a choice control that opened reading "Not set" whatever value it held
- a timezone the control declined to offer while another region was browsed
- a refused write left on screen as though it had taken

None of them were controller faults. The controllers held the right values
throughout; `/var/db/zoneinfo` held the right zone while the surface reported
none. A controller suite proves the catalog holds the right value and can say
nothing about whether the control displays it, so this whole class of defect
had no gate in front of it.

The cost was not hypothetical. The empty-choice defect was live in the
wallpaper **Background fit** control from PR #106 for a day before being
noticed in a different setting entirely, because nothing exercised the shared
control that both settings render through.

## What this adds

The control is extracted out of `SettingsWindow.qml` into
`SettingsEntryControl.qml`, which a Qt Quick Test drives directly against a
stand-in catalog. The window keeps what is genuinely its own: confirming
destructive actions, and owning the picker a path entry asks for. Both arrive
as signals rather than direct calls, which is what makes the control testable
without a window.

Sixteen cases across every control kind, asserting the string a person would
actually read rather than an internal index.

## Does it catch the defect it was written for

A suite that passes against the code that shipped proves nothing. The control
as it shipped was restored into the working tree and the new tests run against
it:

```
FAIL!  : SettingsEntryControl::test_choice_shows_the_value_it_was_given() Compared values are not the same
Totals: 15 passed, 1 failed
```

That is the headline defect, caught. The tree was restored afterwards and
confirmed clean.

Being precise about the other fifteen: they pass against the shipped control
because the control run reverted only the choice binding, and because several
describe behaviour this pull request introduces rather than restores. One case
is a genuine catch of a regression **in this branch**: the first version of
the fix re-read the selection only when the model changed, so an entry rebuilt
with a new value and an identical option list left a stale selection on
screen. `test_choice_follows_a_rebuilt_entry` failed, and the control now
syncs on the entry as well.

## Two faults the shell's own QML gate reported

Neither was in the tests, and both would have shipped without the gate added
in PR #101:

- `property alias enabled` on a `Row` shadows the property every `Item`
  already has, reported as overriding a base member. The row now sets its own
  enabled state and the button inherits it.
- The entry row measures its text column against the width of the control
  beside it, so removing the wrapper broke that measurement with
  `ReferenceError: entryControl is not defined`.

## A placement that did not work

The control was first put in the shared `Northstar.Ui` module, which is where
a reusable control belongs. The shell then could not resolve it: the generated
`qmldir` listed the type and the compiled unit existed in the static library,
but it was not linked into the binary, so the type was declared and absent at
once.

```
qrc:/Northstar/Shell/SettingsWindow.qml: SettingsEntryControl is not a type
```

Rather than work around static QML module linking, the control now sits in the
shell module beside the window that uses it, and the test reaches it by
directory import. These tests should not depend on how a module is linked into
a binary.

## Automated evidence

- Clean build, 0 errors.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr109-5df4bd8 --output-on-failure`
  — **37/37 suites passed, 0 failed**, the new suite being the thirty-seventh.
- The pre-fix control run recorded above.
- `northstar-shell --qml-self-test` on the installed binary — exit 0, no QML
  warnings.
- The suite was re-run after `cmake --install` into `~/.local` and still
  reported 37/37.
- `git diff --check` — exit 0.

## Not claimed

This tests one control in isolation, driven by a stand-in catalog. It does not
test the Settings window as a whole, the desktop, the dock, or any other
surface, and it does not prove the window wires the control to the real
catalog correctly — only that the control renders what it is given. The window
wiring is still covered by nothing but the QML warning gate and a person
looking at it.

Nor does it make interactive acceptance unnecessary. It closes the specific
gap that let three defects through, which is a narrower claim than the surface
being tested.

## Interactive acceptance

The change is structural: every Settings control is now rendered through an
extracted component. Nothing about the surface should look or behave
differently, which is exactly what needs confirming.

Accepted by Hector on 2026-08-19: "looks good". The walkthrough covered every
section reading its real value, changes still taking, values surviving a close
and reopen, and a refused wallpaper snapping back rather than leaving the
rejected choice on screen.

Status: **accepted**.
