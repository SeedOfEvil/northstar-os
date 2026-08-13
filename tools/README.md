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

On FreeBSD, `make package` creates Northstar's native package with CPack.
`tools/publish-development-repository.sh` atomically publishes immutable
package inputs through external catalogue and manifest signers. Its resolved
input, key-custody, provenance, and validation contract is documented in
`docs/M4_SIGNED_DEVELOPMENT_CHANNEL.md`.

`tools/audit-validation-deployment.sh` is the read-only guard for the canonical
Proxmox validation deployment. It verifies the root-owned schema-2 manifest,
clean checkout and exact commit, canonical build, signed repository digests,
package artifact, active `pkg` configuration, installed development shell, and
declared retention boundaries. Run `make validation-deployment-audit` before
and after every VM validation handoff. It reports historical state but never
moves or removes it; cleanup remains an explicit, reviewed quarantine step.

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

Release packaging must compile the same reviewed source for the system-owned
prefix instead of copying the developer build. Use an empty staging root:

```sh
WAYFIRE_PREFIX=/usr/local/libexec/northstar-wayfire-nested \
WAYFIRE_DESTDIR=/path/to/new/staging-root \
sh tools/build-nested-wayfire.sh
```

Pass the resulting
`/path/to/new/staging-root/usr/local/libexec/northstar-wayfire-nested` tree to
`image/scripts/package-nested-wayfire.sh`. This keeps compiled plugin paths and
package paths identical and prevents release sessions from depending on a
particular `/home` directory.

After installing the Northstar binaries to `/usr/local`, the fallback login
policy can be enabled with:

```sh
make disable-console-autostart
sudo sh tools/install-sddm-fallback.sh --enable
```

The installer configures the branded SDDM theme and `Northstar (Proxmox X11
fallback)` session but does not configure autologin.
