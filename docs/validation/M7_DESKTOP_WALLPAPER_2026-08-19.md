# Desktop wallpaper — 2026-08-19

PR #106, verified on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2, Qt 6.11.1, Clang
19.1.7) at commit `821752b`, built in `/home/northstar/builds/pr106-821752b`
with the project's canonical `-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.

The desktop background was a generated gradient with the Northstar mark on it,
and nothing in the product could change it. This slice makes it a picture the
user chooses.

## What the surface offers

Right-click the desktop and choose **Change Desktop Background...**, or open
Settings and find **Desktop background** under Appearance. Both open the same
picker: browse to a picture, click it, and it applies immediately. Five fits —
fill, fit, stretch, centre, tile — and a way back to the built-in background.
The choice survives logout.

## The part that needed the care

A wallpaper is a path into a filesystem the user controls. Between one login
and the next the file can be moved, deleted, replaced by something that is not
a picture, or replaced by a picture far too large to hold in memory. Every one
of those is ordinary, not adversarial.

So the controller reads the file rather than trusting its name. A text file
called `photo.png` is refused by `QImageReader::canRead`, which looks at
content. Three independent bounds apply: 64 MB on disk, 16384 pixels on a
side, and 40 megapixels in total. Both dimension checks exist because either
alone lets something through — a very long thin picture passes a megapixel
test, and a modest square one can still be far more than a desktop needs. The
decoded copy is what the shell holds, so a small file is not by itself a safe
one.

When a stored picture no longer resolves, the desktop falls back to the
built-in background **and says why**, rather than presenting an empty screen
with no explanation. Refreshing the desktop revalidates, so a picture deleted
during a running session is reported instead of lingering as a texture whose
file is gone.

## Two new control kinds, and why they are not dead controls

Settings declares every control against the accessor that backs it, and
refuses to register one whose accessors cannot do what its kind promises. A
picture and a fit are neither toggles nor sliders, so the catalog gained two
kinds and the same discipline had to extend to them:

- A **choice** carries the values it accepts. Registration is refused if the
  list is empty or any option has no value, and a value not on the list is
  rejected by the catalog before the controller is ever called.
- A **path** hands its value straight through, because whether a path is
  usable depends on the file behind it and only the controller can judge that.

That second one exposed a gap in the surface. A refused write could previously
only be reported as "could not be changed", while the controller already knew
the picture was a text file. The new `writeFailureReason` accessor lets the
surface repeat the controller's own reason.

`registersNoDeadControls` failed on the first run of this branch, exactly as it
should have: it enumerates the kinds it knows and had never heard of these two.
Extending it to accept them would not have been enough, because a choice can be
well formed and still be dead — offering nothing, or reading back a value it
does not offer. Both are now checked for every declared entry, along with a
path entry having something to show when nothing is chosen.

## What is deliberately not here

**One wallpaper covers every display.** Per-display backgrounds are out of this
slice on purpose: multi-display is itself an open M6 hardware gate, so the
behaviour could not be validated on the lane this was built on. Building it
here would have meant shipping something whose correctness nobody could check.

## Automated evidence

- Clean build, all 402 targets, 0 errors.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr106-821752b --output-on-failure`
  — **35/35 suites passed, 0 failed**.
- `northstar-wallpapercontroller` is new: 15 cases covering restore across
  sessions, the file-URL and plain-path forms, refusal of a missing file, a
  directory, an oversized picture, and a text file wearing a `.png` name,
  fallback when a stored picture is gone, revalidation in both directions,
  fit-mode round-tripping, and the browser's listing and navigation.
- The suite was re-run **after** `cmake --install` into `~/.local` and still
  reported 35/35. This check exists because PR #103 installed a helper during
  its own verification and silently broke two suites that read the real
  filesystem; the runbook now requires it.
- `env QT_QPA_PLATFORM=offscreen northstar-shell --qml-self-test` on the
  installed binary — exit 0, no QML warnings.
- `git diff --check` — exit 0.

### A note on one test

`picturesShortcutLandsSomewhereReadable` asserts that the Pictures shortcut
leaves the browser in a readable directory, not that it lands in `~/Pictures`.
Whether that folder exists is a property of the machine, and DEV01 did not have
one until this handoff created it. Asserting the destination would have made
the test pass or fail on the machine rather than the code.

## Not claimed

The size and megapixel bounds are exercised only through the per-side check,
which can be triggered with a wide one-pixel-tall picture costing nothing to
build. Constructing a genuine 64 MB or 40-megapixel input to test the other two
would cost far more than the branch is worth; those two limits are read from
the same accessors and are asserted by construction, not by execution.

Rendering quality — how a photograph actually looks at each fit — is a visual
judgement, made in the interactive walkthrough below, on the Proxmox
scfb/pixman lane. That remains supplemental product evidence and is not
physical GPU acceptance.

## Interactive acceptance

Handed off at 1280×800 over noVNC with the shell installed beneath
`/home/northstar/.local` and three sample pictures of different aspect ratios
in `~/Pictures`, plus one text file named `not-a-picture.png`.

Accepted by Hector on 2026-08-19: "It is indeed all working". The walkthrough
covered the built-in background before any picture was set, applying a picture
from the desktop context menu, refusal of the text file wearing a `.png` name,
all five fits on pictures of two different shapes, the same controls in
Settings and their agreement with the desktop, search reaching both entries,
persistence across a logout, and the fallback after deleting the picture in
use and refreshing the desktop.

Status: **accepted**.
