# A software inventory that says something — 2026-08-19

PR #111, verified on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2, Qt 6.11.1) at
commit `2d14e26`, built in `/home/northstar/builds/pr111-2d14e26` with the
project's canonical `-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.

## What was wrong

Reported from the machine: the list is full of things that make no sense, the
scrolling is useless, and nothing can be updated or installed.

Three of those are answered here. The fourth is an architecture question and
is deliberately not.

Measured on the validation machine:

| | |
| --- | --- |
| Packages the Software Center listed | 481 |
| Installed because someone asked for them | 36 |
| Installed as dependencies of something else | 445 |
| Updates available while the window said nothing | 3 |

The window ran `pkg query -a` and listed the result flat. Nobody chose
`libXfont2`; it arrived with something else. At 64 pixels a row, 481 rows is
about 31,000 pixels of travel, and the `ListView` had no scrollbar attached,
so there was no indication of position or of how much was left.

## What this changes

The inventory query now asks pkg for the automatic flag, and the window opens
on the 36 packages someone actually asked for. Dependencies and the full list
are one click away rather than the default. A row says what it is: an update
with the version it would move to, a dependency, or a package whose origin has
left the ports tree.

That last state is real here. `northstar-0.1.4` reports as
`orphaned: x11/northstar` — the installed Northstar package's origin no longer
exists, which is the packaging lane standing still made visible. It is shown
as no longer packaged rather than as current, because it cannot be updated at
all and saying it is up to date would be false.

## The freeze that was not repeated

Reading the inventory and checking for updates look like the same operation
and are not:

| Command | Time |
| --- | --- |
| `pkg query -a '%n\|%v\|%c'` | 0.06 s |
| `pkg query -a '%n\|%v\|%c\|%a'` | 0.04 s |
| `pkg version -vRL=` | **6.72 s** |

`PackageCatalog::refresh()` waits for its process on the thread that draws the
shell. Adding the update check to it would have frozen the desktop for nearly
seven seconds, which is the defect PR #108 shipped and had to fix after a
walkthrough found it.

So the scan runs on its own from the start, and the surface says updates have
not been checked for yet rather than implying everything is current. Until it
has run, "no updates" is not a claim this build is entitled to make.

## A pre-existing test that caught a real mistake

The first version split each inventory line on every separator. A case already
in the suite proved that wrong:

```
Actual   (packages.constFirst().comment)      : "first"
Expected (QStringLiteral("first|with a pipe")): "first|with a pipe"
```

pkg comments may contain the separator. The automatic flag is the last field
and is only ever `0` or `1`, which is what makes it safe to recognise from the
end of the line without a comment that ends in a separator being mistaken for
it. A case was added for exactly that shape.

## Automated evidence

- Clean build, 0 errors.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr111-2d14e26 --output-on-failure`
  — **37/37 suites passed, 0 failed**.
- Three new cases in `northstar-packagecatalog`: requested packages separated
  from dependencies including a line with no flag at all and a comment
  containing separators, update availability and orphans read from real
  `pkg version` output, and lines that say nothing useful ignored.
- The real inventory on this machine parses as `requested=36
  dependencies=445 total=481`, matching what pkg itself reports.
- `northstar-shell --qml-self-test` on the installed binary — exit 0, no QML
  warnings.
- The suite was re-run after `cmake --install` into `~/.local` and still
  reported 37/37.
- `git diff --check` — exit 0.

## Not claimed

**Nothing here installs, updates, or removes a package.** The Software Center
remains a read-only inventory. It now reports what could be updated; acting on
that is the next slice, and installing arbitrary third-party packages is an
architecture question the project has not answered — `northstar-update-helper`
states in its own header that "package installation/upgrade is deliberately
outside this first contract".

The update scan reads whatever repository catalogue is on the machine. If that
catalogue is stale the answer is stale, and fetching a fresh one needs
privilege this surface does not have. A failed scan says so rather than
reporting that everything is current.

The new filter and update controls are not covered by the QML surface tests
from PR #109, which drive the settings control only.

## Window recovery: attempted here, and not achieved

This branch also tried to fix a defect the walkthrough found: a maximised
window could not be moved or minimised, and its controls sat off the visible
area. A shared `NorthstarWindowRecovery` component was added to the four
windows that can be maximised, giving each an Escape key and clamping its
geometry back within the screen when opened.

**It does not work on this compositor, and the reason is instructive.**

The session runs Wayfire, and the shell's windows are native Wayland surfaces:
`xwininfo` against Xwayland finds no Northstar window at all, and the session
status file records `wayland_display=wayland-1`. A Wayland client **cannot set
its own position**. Every `window.x = ...` in the maximise path and in the
clamping is silently ignored by the compositor; only the size takes effect.

So maximising sets the width to the full screen while the window stays where
the compositor put it, and it overflows by however far in it was. That is the
reported defect, and the clamping written to cure it is inert for the same
reason the defect exists.

Two further faults were found the same way and are also unfixed here:

| Action | Result | Cause |
| --- | --- | --- |
| Maximise | Window overflows the screen, controls unreachable | Client-set position ignored on Wayland |
| Escape | Nothing happens | `Shortcut` needs the window to hold keyboard focus, which this compositor does not reliably grant |
| Minimise | Window disappears with no handle | `showMinimized()` iconifies it and the dock lists no shell windows |

All three share one cause: three of the four windows manage their own geometry
as though the shell owned window management, when the compositor does. Quick
Look asks the compositor instead — `showMaximized()`, and `maximized` derived
from `visibility === Window.Maximized` — and is the only one of the four with
none of these faults.

The component added here is kept because it is the right place for the real
fix, not because it currently delivers one. **Nothing about window stranding
should be read as solved by this pull request.** It is the subject of the
next.

The mistake worth recording is mine: the handler was written against X11
semantics without checking what the session actually runs, and the session
status file had said `wayland-1` throughout.

## Interactive acceptance

Accepted by Hector on 2026-08-19 for the inventory: the Installed tile reads
481, the status line reads "36 requested, 445 installed as dependencies", the
filter row offers Installed by you (36), Updates, and Everything (481), and
the update state reads "Updates have not been checked for yet" until a scan is
run.

Accepted with the window-management faults above explicitly outstanding and
carried to the next change.

Status: **accepted**.
