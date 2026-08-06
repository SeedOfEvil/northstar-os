# ADR 0001: FreeBSD 15.1 base

Status: Accepted

## Context

Northstar needs a stable operating-system base with amd64 hardware support, UEFI boot, ZFS, networking, drivers, an established package ecosystem, and an upstream update path. Maintaining a kernel or base-system fork would consume the project before the desktop experience exists.

## Decision

Use FreeBSD 15.1-RELEASE as the initial base. Support amd64 and UEFI only for the first release lane. Use the upstream GENERIC kernel, root-on-ZFS, official FreeBSD release sets, and the official base update mechanism.

## Consequences

Northstar inherits FreeBSD's driver and base-system work and can stay close to upstream. The first hardware matrix is narrow, and the project must track FreeBSD support and package availability explicitly. Base changes require a strong reason and an architecture review.

## Alternatives considered

- Forking or heavily patching the FreeBSD kernel: rejected as unnecessary scope and maintenance burden.
- Supporting multiple architectures immediately: deferred until the amd64 lane is reliable.
- Building a new base system: rejected because it would duplicate FreeBSD infrastructure.

## Validation

M0 must verify the exact release and architecture on a clean VM, and later image work must record release-set checksums.
