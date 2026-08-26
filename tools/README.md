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
make alpha-readiness ALPHA_OUTPUT=/tmp/northstar-alpha-readiness.conf
make alpha-matrix MATRIX_LANE=vm MATRIX_OUTPUT=/tmp/northstar-alpha-matrix.conf
make platform-evidence PLATFORM_OUTPUT=/tmp/northstar-platform-evidence.conf
```

`collect-alpha-readiness.sh` adds the M6 schema-1 hardware/session inventory.
It reports normalized capability classes and counts without serials, MAC
addresses, raw PCI data, command lines, environment values, or user paths.
Ordinary collection succeeds for `ready`, `supplemental`, and `blocked`
systems; pass `--require-ready` only when gating a physical Intel/AMD candidate.
The standard diagnostics bundle includes the same record as
`alpha-readiness.conf`.

`run-alpha-matrix.sh` joins readiness with fixed operator observations without
performing the observed actions. Use `MATRIX_TEMPLATE` to create the mode-0600
template, fill only `pass`, `fail`, `pending`, or `deferred`, and pass it back
through `MATRIX_OBSERVATIONS`. Unknown, missing, or duplicate fields are
rejected. `MATRIX_REQUIRE_PASS=1` is reserved for a complete physical lane.
Physical passes also require the schema-1 record from
`collect-platform-evidence.sh` through `PLATFORM_EVIDENCE`.

`collect-platform-evidence.sh` passively inventories normalized networking,
audio, input, ACPI, and suspend-command capabilities. It never sends traffic,
plays audio, changes volume, injects input, or suspends the host. Its fixed
six-field operator template records the visible connectivity, playback,
volume, keyboard, pointer, and suspend/resume results. VM records remain
supplemental and cannot satisfy `PLATFORM_REQUIRE_PASS=1`.
The standard diagnostics bundle includes the passive record as
`platform-evidence.conf`; operator observations are never collected
implicitly.

`build-alpha-evidence-bundle.sh` atomically joins readiness, platform, and
matrix evidence into one five-file directory with a schema-1 summary and
SHA-256 manifest. It verifies digests and cross-record lane/status agreement
without mutation. VM bundles remain supplemental; `--require-pass` succeeds
only for a complete physical Intel or AMD bundle. See
`docs/M6_PHYSICAL_HARDWARE_OPERATOR_RUNBOOK.md` for the operator sequence.

On FreeBSD, `make package` creates Northstar's native package with CPack.
`tools/publish-development-repository.sh` atomically publishes immutable
package inputs through external catalogue and manifest signers. Publication
requires the canonical repository-root `VERSION` file and rejects Northstar
package metadata that disagrees with it. Its resolved
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
