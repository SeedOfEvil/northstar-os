# Northstar

Northstar is a planning codename for an open-source, FreeBSD-native desktop operating system. It aims to provide one coherent, approachable desktop experience while remaining technically and legally independent from proprietary operating systems.

> A polished, Mac-inspired Unix desktop built on FreeBSD, not an open-source clone of Apple's proprietary operating system.

## Project status

**Pre-alpha.** Northstar runs as a complete system on its supported lane, and
the first alpha extends that lane to physical Intel and AMD hardware.

A pinned image assembles into a bootable UEFI root-on-ZFS system that installs
to a disk, completes first boot, logs in through a display manager, updates
through a signed repository, and rolls back a failed upgrade through ZFS boot
environments. That sequence is accepted end to end.

The desktop provides:

- a supervised session that survives a shell restart without closing your
  applications
- a top bar, dock, desktop icons, and application overview
- a file manager with tabs, background copy and move, conflict handling, and
  one-step undo
- a multi-document text editor with atomic saves and a private recent-file
  history
- searchable settings where every control is declared against the code that
  backs it
- unified search, Quick Look, and notifications that survive a restart
- a read-only software inventory over signed package provenance

**Supported today:** amd64 UEFI on QEMU and Proxmox.

[`docs/ROADMAP.md`](docs/ROADMAP.md) carries per-milestone status and what
comes next; `docs/validation/` carries the dated evidence behind each accepted
slice.

## Fixed baseline

| Area | Decision |
| --- | --- |
| Base OS | FreeBSD 15.1-RELEASE |
| Architecture | amd64 only |
| Firmware | UEFI only for the first supported release |
| Filesystem | Root-on-ZFS |
| Kernel | Upstream GENERIC; no project fork |
| Display | Wayland with Xwayland compatibility |
| Initial compositor | FreeBSD-packaged Wayfire |
| Shell | Qt 6, C++20, and QML |
| IPC | D-Bus behind project-owned typed interfaces |
| Packages | FreeBSD `pkg`, with a Ports overlay for Northstar components |
| Updates | Official FreeBSD base updates plus a signed Northstar repository |
| Rollback | ZFS boot environments through `bectl` |
| Project code licence | BSD-2-Clause |

The authoritative rationale for these choices is in [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) and [`docs/adr/`](docs/adr/).

## Repository layout

```text
docs/          Charter, architecture, roadmap, quality gates, and ADRs
src/           Northstar desktop services and shell implementation
apps/          Northstar desktop applications
config/        Session, compositor, D-Bus, and system defaults
packaging/     Ports, Poudriere, repository, and build manifests
image/         Image assembly and installer work
tests/         Unit, integration, VM, and screenshot tests
tools/         Contributor and diagnostic command wrappers
```

Every directory above is implemented. `docs/validation/` additionally holds
one dated evidence document per accepted change.

## Development interface

`make help` lists every target. The common ones:

```sh
make check-host
make bootstrap NORTHSTAR_USER=<development-user>
make configure
make build
make test
make run-shell
make package
make image
make validation-deployment-audit
make diagnostics OUTPUT=/tmp/northstar-diagnostics
```

These are functional, not reserved names. Two cautions worth knowing before
using them against a validation machine:

- `BUILD_DIR` defaults to `build` inside the checkout. Pass it explicitly when
  building somewhere else, or a target will create a tree in your working copy.
- `make install-user` depends on `build`, so it reconfigures and rebuilds
  `BUILD_DIR` with the Makefile's own flags. To install artifacts you have
  already tested, use `cmake --install <tree> --prefix "$HOME/.local"`.

`make nested-wayfire-session` is the optional Proxmox basic-VGA lane: it builds
a user-local Wayfire compatibility binary and installs the session files needed
by `startx`. It does not replace the package-managed compositor and does not
satisfy the direct DRM/KMS gate.

See [`docs/M0_BOOTSTRAP.md`](docs/M0_BOOTSTRAP.md) for the native FreeBSD
bootstrap, [`docs/M0_PROXMOX.md`](docs/M0_PROXMOX.md) for the Proxmox media and
VM runbook, and [`docs/VM_VALIDATION_DEPLOYMENT.md`](docs/VM_VALIDATION_DEPLOYMENT.md)
for the contract every validation-machine handoff follows.

## Scope guardrails

Northstar will build its own shell, services, and applications on top of FreeBSD. It will not begin by forking the kernel, writing a compositor, replacing `pkg`, or implementing macOS binary compatibility. It will not ship Apple-owned icons, fonts, sounds, branding, or other proprietary assets.

## Contributing

Read [`CONTRIBUTING.md`](CONTRIBUTING.md), [`docs/CHARTER.md`](docs/CHARTER.md), and [`docs/QUALITY_GATES.md`](docs/QUALITY_GATES.md) before opening a change. All architectural changes require an ADR or an update to an existing ADR.

## Licence

Original Northstar code and documentation are released under the BSD-2-Clause licence. See [`LICENSE`](LICENSE) and [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) for the project and upstream licence policy.
