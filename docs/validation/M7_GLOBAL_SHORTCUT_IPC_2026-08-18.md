# Global shortcut over a shell control socket — 2026-08-18

PR #99, validated on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2, Proxmox/noVNC).

This closes the `Ctrl+K` defect recorded as still open in
[`M7_SHELL_SHORTCUT_FOCUS_2026-08-18.md`](M7_SHELL_SHORTCUT_FOCUS_2026-08-18.md).

## What changed

`Ctrl+K` is no longer a shell shortcut. `config/wayfire-nested.ini` binds
`<ctrl> KEY_K` to `northstar-shell-command toggle-search`, which sends one
command to a user-private control socket the shell listens on. The compositor
delivers the key regardless of which client holds focus, which is the property
the previous three client-side attempts could not achieve.

The compositor configuration is project-owned and `test-nested-wayfire-session.sh`
already compares the installed copy against the template byte for byte, so the
binding is versioned and tested rather than user state.

The in-shell `Ctrl+K` shortcut is kept as a fallback for sessions whose
`wayfire.ini` predates this change. The compositor binding consumes the key
first, so the two cannot both fire.

## Security shape

- The socket is created with `QLocalServer::UserAccessOption` inside
  `XDG_RUNTIME_DIR`. A test asserts it carries no group or other access.
- The server acts only on a fixed set of command names and never executes text
  it is given. An arbitrary string such as `rm -rf /` is refused exactly like
  any other unrecognised name and is never passed on to the shell.
- Overlong and truncated requests are refused rather than buffered.
- A socket left behind by a shell that was killed abruptly is reclaimed on the
  next start. The socket is not removed on abrupt termination, which is why
  that reclaim exists and is tested.

## Three deployment defects found during validation

The first version of this change installed cleanly, passed every automated
gate, and did nothing at all. Each of the following was independently
sufficient to break the feature silently, and each was found by inspecting the
running session rather than by any test.

1. **The command was not on the compositor's PATH.** The PATH export was first
   added to `northstar-session-x11`, but the display manager resolves that
   entry point by name and found a stale system-wide copy at
   `/usr/local/bin/northstar-session-x11`, dated 2026-08-10, which shadowed the
   freshly installed one. The export now lives in `northstar-session`, the
   supervisor, which always runs from the project's own installation whichever
   entry point started it. The stale entry point still exports an absolute
   `NORTHSTAR_SESSION_SHELL`, so the supervisor derives the correct directory.
2. **The client could not name the socket.** The shell names it after
   `WAYLAND_DISPLAY` so two sessions on one account do not collide, but the
   compositor runs binding commands without `WAYLAND_DISPLAY` set. The client
   therefore looked for `northstar-shell.sock` while the shell had created
   `northstar-shell-wayland-1.sock`. Clients now discover the socket in the
   runtime directory when the derived name is absent, and refuse to guess when
   more than one is present.
3. **Deploying the configuration replaced `~/.xinitrc`.** Running the session
   installer with `--force` to install the new `wayfire.ini` also replaced
   `.xinitrc` with the unsupervised variant. SDDM login is unaffected because
   it runs `northstar-session-x11`, but the `startx` path was degraded. It was
   restored from the installer's own backup.

The stale `/usr/local/bin/northstar-session-x11` was left in place. It is no
longer able to break this feature, and removing a system-wide file is a
separate deliberate operation.

## Automated evidence

- Clean FreeBSD build, all 388 targets.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr99-1480ec4 --output-on-failure`
  — **33/33 suites passed, 0 failed**. The count rises from 32 with the new
  socket suite.
- `northstar-shellcommandserver` — **9 cases passed, 0 failed, 0 skipped**:
  user-private permissions, delivery, repeat delivery, unknown-command refusal,
  overlong refusal, truncated-request handling, stale-socket reclaim, per-display
  naming, and discovery without `WAYLAND_DISPLAY` including the refusal to guess
  between two sessions.
- `sh tests/unit/test-qml-surfaces.sh` and `sh tests/unit/test-session-entrypoint.sh`
  — passed. The surface contract now pins the compositor binding, the session
  PATH export, and the client-side socket resolution.
- `git diff --check` — exit 0.

## Evidence against the live session

Automated gates could not have caught any of the three deployment defects, so
the decisive checks were run against the running session.

- Using the compositor's exact environment, the installed CLI finds and drives
  the running shell, exit status 0. Before the fixes, the identical invocation
  reported
  `No Northstar shell is listening on /var/run/user/1001/northstar-shell.sock`,
  and `command -v northstar-shell-command` reported not found on that PATH.
- The control socket is present in the session runtime directory as
  `srwx------ northstar-shell-wayland-1.sock`.

## Interactive acceptance

After a fresh login the user reported:

> the control+k works everytime

This covers repeated open and close, and reaching search from the bare desktop
after other windows have been opened and closed, which is the scenario that
failed in every previous attempt.

`Super+Enter`, listed in the handoff checklist to confirm the pre-existing
terminal binding still worked, was not exercised. noVNC generally does not pass
the Super key through to the guest, so that item was not testable in this lane
and should not appear in future checklists for it.

## Not claimed

`make validation-deployment-audit` is not claimed to pass. DEV01's root-owned
manifest still describes PR74/r78, unchanged from PR #95 through #98.

Physical Intel/AMD DRM/KMS acceptance, native compositor quality, and
multi-display evidence remain deferred. The Proxmox scfb/pixman VM remains
supplemental product evidence only.
