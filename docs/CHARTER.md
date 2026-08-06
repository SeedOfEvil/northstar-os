# Northstar project charter

## Mission

Northstar will build an open-source, FreeBSD-native desktop operating system that feels coherent and approachable while remaining technically and legally independent from proprietary operating systems.

## Product statement

> A polished, Mac-inspired Unix desktop built on FreeBSD, not an open-source clone of Apple's proprietary operating system.

Northstar is the planning codename. The final product name, visual identity, and public organization are not fixed by this charter.

## What success means

A first alpha is successful when an ordinary supported amd64 UEFI computer or QEMU VM can install and boot a root-on-ZFS system, log in to one intentional Wayland desktop, launch native Qt, Xwayland, and browser applications, manage files, change settings, update through signed packages, roll back a failed upgrade, collect diagnostics, survive a shell restart, and shut down cleanly.

## Principles

1. **FreeBSD remains upstream.** Use the upstream GENERIC kernel, base system, ZFS, drivers, networking, and official update path wherever possible.
2. **Coherence is a product requirement.** The top bar, dock, launcher, settings, notifications, search, applications, defaults, and documentation must feel like one system.
3. **Protocols over compositor internals.** The shell targets Wayland protocols and keeps Wayfire-specific behavior behind a replaceable platform interface.
4. **Existing applications matter.** Standard FreeBSD packages, `.desktop` entries, Qt applications, Wayland applications, and X11 applications remain first-class inputs.
5. **Reproducibility is part of correctness.** Every build records exact upstream inputs, resolved package versions, checksums, and the project commit.
6. **Rollback is a user feature.** Major upgrades create a ZFS boot environment before changing the system.
7. **Security is designed in.** Desktop services run unprivileged by default; privileged operations use narrow authorization and protected build infrastructure.
8. **Legal independence is visible.** Northstar uses original assets and clearly differentiated branding. It does not ship Apple-owned assets or promise macOS binary compatibility.

## Initial scope

The first supported lane is amd64 on UEFI systems with root-on-ZFS. The initial graphics lane is QEMU plus tested Intel and AMD hardware. Wayfire is the bootstrap compositor; the shell must not make it irreplaceable. Qt 6, C++20, QML, D-Bus, `pkg`, Ports, Poudriere, and `bectl` are the project building blocks.

The first Northstar-owned surfaces are:

- top menu bar and dock;
- launcher and application overview;
- session lifecycle and diagnostics;
- settings, notifications, search, and file associations;
- file manager, terminal, software, and other project-owned applications;
- a project-defined `.app` presentation format that complements, rather than replaces, FreeBSD packages.

## Explicit non-goals

The project will not begin with a custom kernel, custom compositor, new package manager, fully custom graphical installer, macOS binary compatibility, SwiftUI implementation, Metal compatibility, Apple Silicon support, or universal global menus for every third-party toolkit.

## Governance and change control

The repository is intended to be a public GitHub monorepo with protected `main`, pull requests for all changes, required checks, signed release tags, and a project board organized by milestone and area. Architectural changes require an ADR. Release infrastructure and signing keys remain outside pull-request execution paths.

## Licence and assets

Original Northstar code and documentation use the BSD-2-Clause licence. Each external dependency keeps its own licence and notices. Contributors must not add proprietary or Apple-owned icons, fonts, sounds, logos, trademarks, or copied interface assets.

## Current status

This charter is the PR 1 foundation. M0 is not complete until the repeatable FreeBSD host bootstrap and its native validation gate pass.
