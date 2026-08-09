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
- records non-secret lifecycle messages;
- preserves terminal failure state and the failure event when startup or the
  bounded shell-restart policy fails, instead of overwriting it as a normal
  stop during cleanup;
- refuses duplicate sessions without clobbering the active session's status or
  control contract.

While the shell is supervised, the supervisor writes a user-private
`session.status` contract below its state directory and exports the status and
control paths to the shell. The shell's Settings > Session page reads that
contract to show the session state, Wayland display, owned process IDs, and
restart count. A confirmed End Northstar Session request writes an
`end-session` marker and signals the exact parent supervisor; the supervisor's
existing cleanup path then terminates only the compositor and shell it owns.

The descriptor, SDDM integration boundary, opt-in nested wrapper, and explicit
Proxmox X11 fallback session are now present. The first branded SDDM greeter is also present under
`config/sddm/northstar`; it delegates authentication, session selection, and
power actions to SDDM and uses the official Northstar logo. The project does
not yet perform the native DRM/KMS login acceptance or supervise the full
service set. The fallback policy can be enabled on the development VM after a
system-prefix install and recovery-console check; native display-manager
acceptance remains separate.

The shell launch slice is documented in [`docs/M2_LAUNCHES.md`](M2_LAUNCHES.md).
It records the identity and PID of launched applications in a user-private log,
presents short success/failure feedback, and exposes a bounded in-shell
Notification Center without expanding supervisor ownership to those
applications.

The system menu now exposes a confirmed `Log Out of Northstar` action while a
supervised session is running. It reuses the exact-parent supervisor request
and therefore ends only Northstar's shell/compositor. In an unmanaged shell,
the same menu entry is labeled `Quit Northstar Shell` and retains the local
development fallback without claiming to perform a session logout.

The system menu also exposes confirmed `Restart FreeBSD` and `Shut Down
FreeBSD` actions. They call the fixed-argument user-local
`northstar-power` boundary, which is authorized by the current development
VM's non-interactive sudo policy. The shell never constructs or executes an
arbitrary privileged command. Production packaging must replace that wrapper
with a root-owned helper and a narrow authorization rule for only the restart
and shutdown operations.

For the current Proxmox console lane, the opt-in console-login hook can start
the supervised nested session automatically after a local `ttyv0` through
`ttyv7` login:

```sh
make install-console-autostart
```

The hook does not run for SSH or serial logins and can be disabled without
removing the profile integration:

```sh
make disable-console-autostart
```

This is a development-console convenience, not the final graphical-login
policy. The production path remains the standard Wayland descriptor selected
through the branded SDDM greeter, followed by a separately validated autologin
policy and lock screen.

The greeter can be previewed without changing the active display manager:

```sh
sddm-greeter --test-mode --theme "$PWD/config/sddm/northstar"
```

Do not enable SDDM while the console `startx` session is active. First disable
the console hook, preserve a recovery console, and validate the greeter and
the selected `northstar.desktop` session manually. The current Proxmox
basic-VGA lane remains insufficient for native Wayland/DRM acceptance.

## Deterministic validation

The fake-compositor/fake-shell test runs as an unprivileged user and verifies
bounded shell restart, compositor cleanup, duplicate-session rejection, the
D-Bus wrapper handoff, preservation of an unrelated sentinel process, and
failure-status retention:

```sh
make test
```

`make test` also covers the power-controller contract, console autostart
installer, SDDM theme boundary, and Proxmox fallback environment, stages the
built files into a temporary prefix, and verifies that the installed session
descriptors launch only unprivileged Northstar entry points.

The current shell and compositor can be selected without changing source:

```sh
NORTHSTAR_SESSION_COMPOSITOR="$HOME/.local/wayfire-nested/bin/wayfire" \
NORTHSTAR_SESSION_SHELL="$PWD/build/src/shell/northstar-shell" \
    sh src/session/northstar-session
```

Use the live-session smoke commands for the current nested Wayland console;
do not replace the working session until display-manager integration is
implemented and separately validated.
