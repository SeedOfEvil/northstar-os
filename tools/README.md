# Contributor tools

M0 provides `check-host.sh`, `bootstrap-dev.sh`, `collect-diagnostics.sh`,
`build-nested-wayfire.sh`, `install-nested-wayfire-session.sh`, and
`install-console-autostart.sh`, and `install-sddm-fallback.sh`. They use
POSIX `/bin/sh`, avoid secrets, and document manual recovery. The M2
`src/session/northstar-session` supervisor and the standard Wayland session
descriptor are now installable through `make install-user`. The SDDM fallback
installer is an explicit root-only opt-in for the current Proxmox basic-VGA
lane; it preserves a separate native Wayland entry. The console autostart
installer is limited to local virtual-console logins and preserves the user's
existing profile.
`install-nested-wayfire-session.sh --supervised` is an explicit,
backup-preserving way to rehearse the same supervisor from a future fresh
`startx` session.

Use the repository Make targets for the stable interface:

```sh
make check-host
make bootstrap NORTHSTAR_USER=<development-user>
make diagnostics OUTPUT=/tmp/northstar-m0-diagnostics
```

On the Proxmox basic-VGA validation lane, the stock FreeBSD Wayfire package
cannot start without a DRM render node. After installing the optional build
dependencies documented in `docs/M0_PROXMOX.md`, use:

```sh
make nested-wayfire-session
```

This builds the patched binary under the development user's home directory and
installs the user-level `.xinitrc` and `wayfire.ini` needed by `startx`. Existing
files are preserved; use `sh tools/install-nested-wayfire-session.sh --force`
only when you have reviewed the backup behavior.

After installing the Northstar binaries to `/usr/local`, the fallback login
policy can be enabled with:

```sh
make disable-console-autostart
sudo sh tools/install-sddm-fallback.sh --enable
```

The installer configures the branded SDDM theme and `Northstar (Proxmox X11
fallback)` session but does not configure autologin.
