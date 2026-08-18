# Shell shortcut focus investigation — 2026-08-18

PR #98, validated on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2, Proxmox/noVNC).

This document records an investigation that **did not fix its original target**.
It is written up in full because the diagnosis is solid, the constraint it
uncovered is architectural, and the next attempt should not have to rediscover
any of it.

## The reported defect

`Ctrl+K` opened unified search only once per shell start. After the overlay was
closed it never reopened. Found while checking PR #97.

## What was ruled out

The overlay QML is **not** at fault. This PR adds `northstar-shell
--qml-self-test`, which drives `openSearch` → `closeSearch` → `openSearch` and
asserts window visibility at each step. It passes, so the surface always
reshowed correctly whenever `openSearch()` was actually called. The shortcut
was not firing.

An early hypothesis, that closing the overlay killed key delivery for every
`Qt.ApplicationShortcut`, was recorded as disproven in
[`M7_SHELL_PALETTE_AND_SHORTCUT_FIXES_2026-08-18.md`](M7_SHELL_PALETTE_AND_SHORTCUT_FIXES_2026-08-18.md)
on the basis that other shortcuts kept working. **That note is wrong and is
corrected here.** `wayfire.ini` contains exactly one compositor-level binding,
`<super> KEY_ENTER` for a terminal. Every other shortcut is a Qt application
shortcut registered by the shell, and all of them die at the same moment. The
shortcut that appeared to keep working was the compositor one.

## Confirmed cause

`layershellsurface.cpp` configures:

| Surface | Keyboard interactivity |
| --- | --- |
| Desktop background | `None` |
| Dock | `None` |
| Panel | `OnDemand` |

All three also set `setActivateOnShow(false)`.

The panel is therefore the only shell surface that can hold keyboard focus, and
an on-demand layer surface receives focus only when it asks for it or the user
interacts with it. The shell's shortcuts are `Qt.ApplicationShortcut`, which
fire only while the application has a focused window.

Unified search, Files, Settings, and Quick Look are ordinary toplevels owned by
the shell process. Opening one gives the shell process focus and the shortcuts
work. Closing it leaves the compositor with nothing to hand focus back to,
because the panel will not take focus unaided. The shell then has **no focused
window**, receives no key events, and every shortcut it registers is dead.

The user confirmed each step:

> from a fresh desktop first Ctrl+K works; if I open something and close it, it
> stops working from the desktop; if I open manually and click on something
> then Ctrl+K works again

and then, decisively:

> even if I open the text editor, or files or any other app and then close em
> the same thing happens; if I go back in focus to an open app and use
> Ctrl+K it works

## What was attempted, and what each attempt proved

1. **`requestActivate()` on the panel when a transient surface hides.** Ignored
   for an on-demand layer surface. The user still had to click the panel.
2. **Toggle keyboard interactivity to exclusive, then back on the next
   event-loop turn.** Also ineffective. Layer-shell requests take effect on the
   next surface commit, so both requests coalesced into one and the compositor
   never saw the exclusive state.
3. **Hold exclusive interactivity until the panel actually becomes active**,
   releasing on `QWindow::activeChanged` with a 400 ms fallback. This is what
   the branch carries. It **still does not restore the shortcut.**

## Conclusion

The approach is wrong, not the implementation. A global desktop shortcut must
not depend on client keyboard focus. A layer-shell panel is not designed to
hold focus, so shortcuts registered as `Qt.ApplicationShortcut` inside it can
never be reliably global. Further client-side attempts are not worth making.

The correct fix is a compositor-level binding that reaches the running shell
over IPC, which works regardless of which client holds focus. The project
already has a precedent for this shape of control channel: `SessionController`
writes a line to an owner-only control file and signals the supervisor.
Applying it here means a small control path the shell watches, and a
`wayfire.ini` binding that writes to it. That changes session configuration and
is a separate, deliberate piece of work.

## What this branch keeps, and why

The original target is **not fixed**. Two things are kept because they stand on
their own.

### The shell self-test

`northstar-shell --qml-self-test`, registered as a ctest gate. The shell was
the only major surface with no self-test, which is why this defect had no
automated coverage at all, and why passing an unsupported flag to the binary
previously hung for ten minutes instead of failing.

### Focus handling after a transient surface closes

All nine transient surfaces now ask for keyboard focus to return to the panel
when they stop being visible, however they were dismissed. This does not
restore the shortcut, but the user reports that general interaction improved
noticeably as a result:

> whatever you did before this made all the menus and apps and clicks on
> desktop work better than ever

That is a subjective observation from interactive use, not a measurement, and
it is recorded as such. It is kept on that basis and on the user's explicit
instruction to keep it.

**Known risk.** While a reclaim is in flight the panel holds exclusive keyboard
interactivity, for at most 400 ms and normally only until the panel becomes
active. In that window keystrokes go to the panel rather than to an application.
The user has not observed lost keystrokes in interactive use. Anyone who does
should treat this as the first suspect.

## Automated evidence

- Clean FreeBSD build, all 378 targets.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr98-27c8ac9 --output-on-failure`
  — **32/32 suites passed, 0 failed**. The count rises from 31 with the new
  shell self-test.
- `sh tests/unit/test-qml-surfaces.sh` — passed.
- `git diff --check` — exit 0.
- The installed shell starts with **zero `qrc:` diagnostics of any kind**.

The offscreen platform has no compositor focus model, so none of the above
proves anything about focus behavior. Every focus claim in this document comes
from interactive observation on the live session.

`make validation-deployment-audit` is **not claimed to pass**; DEV01's
root-owned manifest still describes PR74/r78, unchanged from PR #95, #96, and
#97.

## Still open

`Ctrl+K` does not reopen unified search after any shell window has been opened
and closed. Workarounds available today: click the panel, or focus any shell
window. Tracked for the compositor-level fix described above.
