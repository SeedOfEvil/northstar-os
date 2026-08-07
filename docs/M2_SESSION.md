# M2 session supervisor foundation

`src/session/northstar-session` is the first M2 implementation slice. It is a
user-level POSIX supervisor with deliberately narrow ownership. The standard
Wayland session descriptor in `config/session/northstar.desktop` makes it the
display-manager entry point after installation:

```sh
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

System packaging will install the same descriptor below
`share/wayland-sessions` under the package prefix. The current Proxmox
`startx` lane remains unchanged until this entry point is exercised in a
separately controlled login session.

For an explicit nested-session rehearsal, install the user-local binaries and
then opt in to the supervised `.xinitrc`:

```sh
make install-user NORTHSTAR_PREFIX="$HOME/.local"
sh tools/install-nested-wayfire-session.sh --supervised --force
```

That command backs up an existing `.xinitrc` before replacing it. It is not a
live-session operation; use it only before a fresh console `startx` test. The
normal installer continues to launch nested Wayfire directly.

The supervisor:

- prepares `XDG_*` session variables and a private state/log directory;
- acquires an atomic per-user session lock so duplicate sessions are rejected;
- starts a D-Bus session unless an existing one is explicitly selected;
- starts the configured compositor, discovers its Wayland socket, and exports
  that socket to the shell;
- restarts a crashed shell up to a bounded limit;
- stops only the compositor and shell PIDs it started;
- records non-secret lifecycle messages.

The descriptor, SDDM integration boundary, and opt-in nested wrapper are now
present, but the project does not yet enable a display manager, perform
privileged logout/reboot/shutdown, or supervise the full service set. Those
are the remaining M2 acceptance slices.

## Deterministic validation

The fake-compositor/fake-shell test runs as an unprivileged user and verifies
bounded shell restart, compositor cleanup, duplicate-session rejection, the
D-Bus wrapper handoff, and preservation of an unrelated sentinel process:

```sh
make test
```

`make test` also stages the built files into a temporary prefix and verifies
that the installed session descriptor launches only the unprivileged
`northstar-session` entry point.

The current shell and compositor can be selected without changing source:

```sh
NORTHSTAR_SESSION_COMPOSITOR="$HOME/.local/wayfire-nested/bin/wayfire" \
NORTHSTAR_SESSION_SHELL="$PWD/build/src/shell/northstar-shell" \
    sh src/session/northstar-session
```

Use the live-session smoke commands for the current nested Wayland console;
do not replace the working session until display-manager integration is
implemented and separately validated.
