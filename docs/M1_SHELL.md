# M1 shell seed

The first M1 implementation slice is a native `northstar-shell` Qt 6/C++20
process. Its compositor-sensitive code is isolated in
`src/shell/layershellsurface.*`, which uses the FreeBSD-packaged
`LayerShellQt::Interface` target. QML remains responsible for the visual
surface and design tokens rather than Wayfire-specific behavior.

## Build and run

On a bootstrapped FreeBSD 15.1 development host:

```sh
make configure
make build
make test
make run-shell
```

With the shell already running in the active session, the repeatable runtime
checks are:

```sh
make shell-smoke
make shell-restart-smoke
```

The restart check only terminates the exact PID in
`/tmp/northstar-shell-m1-live.pid` and verifies that existing QTerminal
clients remain present.

For the current supplemental Proxmox session, run the shell from the active
Wayland environment:

```sh
export XDG_RUNTIME_DIR=/var/run/xdg/northstar
export WAYLAND_DISPLAY=wayland-1
export DISPLAY=:3
export XAUTHORITY="$HOME/.Xauthority"
export QT_QPA_PLATFORM=wayland
export MOZ_ENABLE_WAYLAND=1
make run-shell
```

The shell creates one desktop-background surface, one reserved top panel, and
one reserved bottom dock surface per connected display. The background surface
uses the official Northstar logo from the installed branding path and stays
behind regular applications. The panel contains the Northstar title,
active-window placeholder, clock, an Applications menu action, and a
light/dark token toggle. The dock contains pinned Terminal/Firefox launch
shortcuts, a live list of mapped application views, refresh, focus, and
minimize/restore controls. Wayfire IPC is optional at runtime; without an IPC
socket, pinned launches still work and the dock reports that compositor window
control is unavailable.

The system menu and application overview are independent desktop windows rather
than popups constrained to the panel surface, so they can extend into the
desktop area. The Settings surface is a non-modal, movable, resizable desktop
window with minimize, maximize/restore, and close controls. Its functional
Appearance, Session, and About Northstar pages adapt to the resized surface.
Appearance controls the shared dark/light tokens; Session reports the active
desktop, display size, and application count and can refresh the catalog; About
reports the shell name and version.

The shell runs unprivileged. It only asks the compositor to focus or minimize a
specific view ID returned by Wayfire IPC; it never signals or terminates a
launched application process.

`src/launcher/applicationcatalog.*` discovers standard `.desktop` entries in
the XDG user and system application directories. User entries take precedence
over system entries with the same desktop id. Hidden, non-display, non-
application, desktop-specific, and unparseable entries are excluded. Launch
arguments are tokenized and passed directly to `QProcess`; the catalog never
routes an entry through a shell, and document/URI field codes are left out
until a caller supplies that context.

The overview filters by application name, generic name, category, or desktop
id. The first matching entry can be launched from the search field, while a
selected row launches the same direct `QProcess` command used by the menu.

## Current evidence and open gate

The native FreeBSD build and the shell/application-catalog/window-controller Qt
tests pass on
`NSTAR-DEV01`. `make shell-smoke` and `make shell-restart-smoke` pass in the
live single-display nested Wayland session; the restart check leaves the
existing QTerminal client running.

This is an implementation slice, not an M1 completion claim. Multi-display
testing is intentionally deferred for the current development lane and
remains a release acceptance gate. The remaining acceptance work is:

- verify one correctly reserved surface on each connected display, including
  a multi-monitor run;
- exercise the Terminal and Firefox buttons from the shell in the visible
  console; their program mapping is covered by the Qt test;
- validate the bottom dock against the live Wayfire IPC socket and exercise
  focus and minimize/restore for QTerminal and Firefox;
- complete the remaining launcher/session integration before M1 is marked
  complete.
