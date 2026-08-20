# Window management belongs to the compositor — 2026-08-19

PR #112, verified on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2, Qt 6.11.1) at
commit `d986f70`, built in `/home/northstar/builds/pr112-d986f70` with the
project's canonical `-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.

## Three faults, one cause

Found during the PR #111 walkthrough, reported one at a time:

| Action | What happened |
| --- | --- |
| Maximise | The window overflowed the screen and its controls could not be reached |
| Escape | Nothing |
| Minimise | The window disappeared with no way to bring it back |

All three came from the shell doing window management the compositor owns.

## Maximise

`toggleMaximize()` set the window's own `x`, `y`, `width`, and `height`. The
session is Wayfire, and the shell's windows are native Wayland surfaces —
`xwininfo` against Xwayland finds no Northstar window at all. **A Wayland
client cannot set its own position.** The size assignments applied and the
position assignments were silently ignored, so the window grew to the full
screen width while staying where it was, and overflowed by however far in it
had been. That is what carried the controls off the screen.

It now asks the compositor with `showMaximized()` and `showNormal()`, and
reads the state back with `visibility === Window.Maximized` rather than
keeping a parallel `bool` that can disagree with reality. Quick Look already
did exactly this, and was the only one of the four windows without the fault —
the other three each carried their own copy of the wrong approach.

## Minimise

`showMinimized()` iconifies through the compositor, which is correct. The
window then had nowhere to be restored from, because the dock excluded it:

```cpp
if (viewId < 0 || pid <= 0 || pid == shellPid || ...) continue;
```

That exclusion is meant to keep the panel, dock, and background out of the
task list. Measured against the compositor with the Software Center open:

```
pid=73056 id=3 role=desktop-environment  app-id=northstar-background-0  title=layer-shell
pid=73056 id=4 role=desktop-environment  app-id=northstar-shell-0       title=layer-shell
pid=73056 id=5 role=desktop-environment  app-id=northstar-dock-0        title=layer-shell
pid=73056 id=8 role=toplevel             app-id=northstar-shell         title=Northstar Software
```

All four belong to the shell process. The **role** is what separates the
desktop's own surfaces from a window someone can point at, and the role check
was already there and already correct. The process check added nothing except
removing Settings, Files, and the Software Center from the only place a
minimised window can be recovered from.

The process check is gone. The role check stands on its own, which is what it
was always doing the useful half of.

## Escape

Left as it is, and not claimed as a fix. `Shortcut` requires the window to
hold keyboard focus, which this compositor does not reliably grant to a shell
window — the same condition that made the project build a compositor-bound
global shortcut path and a control socket in the first place. It works when
the window is focused and does nothing when it is not.

With maximise fixed the window keeps its own controls on screen, and with the
dock listing it a minimised window has a handle, so Escape is no longer the
only way back. Making a keyboard escape work regardless of focus means going
through the compositor's shortcut binding, which is a different piece of work.

## Automated evidence

- Clean build, 0 errors.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr112-d986f70 --output-on-failure`
  — **37/37 suites passed, 0 failed**.
- A new `northstar-windowcontroller` case builds the four views exactly as the
  compositor reported them above, using the test process's own pid, and
  asserts the toplevel is listed while the three desktop-environment surfaces
  are not.
- `northstar-shell --qml-self-test` on the installed binary — exit 0, no QML
  warnings.
- The compositor probe output above, taken from the running session.
- `git diff --check` — exit 0.

### A test that was named for something it never did

`refreshFiltersDesktopAndShellViews` has existed since the dock was written
and its name claims it covers shell views. It builds its fixtures with
arbitrary process ids, so the `pid == shellPid` branch was never once
exercised. The behaviour that broke had a test named after it and no test
covering it.

## Not claimed

Nothing here was verified interactively before handoff; the three faults are
compositor behaviour and cannot be observed from a headless run. The unit test
proves the dock's filtering rule against recorded compositor output, not that
the compositor does what the recording says on the next run.

The Escape key is unchanged and still depends on window focus.

Whether a window restored from the dock returns to a sensible size and place
is the compositor's decision now rather than the shell's, which is the point,
but it also means the answer can differ on another compositor.

## Interactive acceptance

Accepted by Hector on 2026-08-19: "working now fixed". The walkthrough covered
maximising to exactly the screen with its controls reachable, un-maximising
back to a normal window, minimising into the dock and restoring from it, the
same three on Files and Settings, and the panel, dock, and desktop background
staying out of the dock as entries.

Status: **accepted**.
