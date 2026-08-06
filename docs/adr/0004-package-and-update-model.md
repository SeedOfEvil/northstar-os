# ADR 0004: FreeBSD package and update model

Status: Accepted

## Context

The desktop must install and update without copying development files into the base system. Users need signed artifacts and a rollback path, while FreeBSD already provides `pkg`, Ports, Poudriere, official base updates, and ZFS boot environments.

## Decision

Use the official FreeBSD update mechanism for the base system and kernel. Use pinned FreeBSD packages for third-party desktop dependencies. Build Northstar components in a FreeBSD Ports overlay with Poudriere and publish them through a signed project `pkg` repository with development and stable channels. Create a ZFS boot environment with `bectl` before every major upgrade.

## Consequences

Northstar inherits existing package and boot-environment behavior and does not need a new package manager. Repository signing, package provenance, channel compatibility, and clean-jail builds become release requirements. Packaged-base preview features are not required for the first release.

## Alternatives considered

- A custom package manager: rejected because it would duplicate `pkg` and weaken FreeBSD integration.
- Copying application files into `/usr`: rejected because it is not transactional or reproducible.
- Making packaged-base support mandatory: deferred until it is mature enough for the project's needs.

## Validation

M4 must prove signed installation, N-1 to N upgrade, pre-upgrade boot-environment creation, rollback to the prior shell/package set, and preservation of home data.
