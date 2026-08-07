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

The shell creates one reserved top panel surface per connected display. The
panel contains the Northstar title, active-window placeholder, clock, an
Applications menu action, a searchable application overview, pinned
Terminal/Firefox launch buttons, and a light/dark token toggle. The system menu
and application overview are independent desktop windows rather than popups
constrained to the panel surface, so they can extend into the desktop area.
The shell runs unprivileged and does not own or terminate launched
applications.

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

The native FreeBSD build and the `northstar-shellstate`/application-catalog Qt
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
- complete the remaining launcher/session integration before M1 is marked
  complete.
