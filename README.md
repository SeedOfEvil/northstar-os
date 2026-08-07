# Northstar

Northstar is a planning codename for an open-source, FreeBSD-native desktop operating system. It aims to provide one coherent, approachable desktop experience while remaining technically and legally independent from proprietary operating systems.

> A polished, Mac-inspired Unix desktop built on FreeBSD, not an open-source clone of Apple's proprietary operating system.

## Project status

This repository has completed the M0 tooling and supplemental Proxmox basic-VGA
validation lane. The first M1 shell slice now builds and runs natively on the
validation VM, including the initial standard `.desktop` application catalog
and searchable application overview;
direct DRM/KMS acceptance, multi-display coverage, and the full desktop session
remain ahead. It is not yet an installable operating system.

The first implementation target is a repeatable custom shell on a stock FreeBSD installation:

```text
FreeBSD 15.1 amd64
  -> Wayfire session
  -> Northstar Qt top bar and dock
  -> QTerminal and Firefox launch
  -> shell restart without terminating applications
```

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

Directories remain intentionally scaffolded ahead of implementation, while
the M1 shell seed is now the first implemented source component.

## Development interface

The stable contributor commands are being introduced incrementally. The command names are reserved now so documentation, CI, and local workflows can converge on one interface:

```sh
make check-host
make bootstrap NORTHSTAR_USER=<development-user>
make configure
make build
make test
make run-shell
make package
make vm-smoke
make nested-wayfire
make nested-wayfire-session
make image
make diagnostics OUTPUT=/tmp/northstar-m0-diagnostics
```

The M0 commands are now functional. `make nested-wayfire-session` is the
optional Proxmox basic-VGA lane: it builds a user-local Wayfire compatibility
binary and installs the session files needed by `startx`; it does not replace
the package-managed compositor or satisfy the direct DRM/KMS gate. The
configure, build, shell, package, and image targets remain guarded until their
milestones are implemented. See
[`docs/M0_BOOTSTRAP.md`](docs/M0_BOOTSTRAP.md) for the native FreeBSD
bootstrap and [`docs/M0_PROXMOX.md`](docs/M0_PROXMOX.md) for the local Proxmox
media and VM runbook.

## Scope guardrails

Northstar will build its own shell, services, and applications on top of FreeBSD. It will not begin by forking the kernel, writing a compositor, replacing `pkg`, or implementing macOS binary compatibility. It will not ship Apple-owned icons, fonts, sounds, branding, or other proprietary assets.

## Contributing

Read [`CONTRIBUTING.md`](CONTRIBUTING.md), [`docs/CHARTER.md`](docs/CHARTER.md), and [`docs/QUALITY_GATES.md`](docs/QUALITY_GATES.md) before opening a change. All architectural changes require an ADR or an update to an existing ADR.

## Licence

Original Northstar code and documentation are released under the BSD-2-Clause licence. See [`LICENSE`](LICENSE) and [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) for the project and upstream licence policy.
