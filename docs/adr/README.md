# Architecture decision records

ADRs capture decisions that affect long-lived boundaries, dependencies, compatibility, security, or release behavior.

Use this structure for new records:

```text
# ADR NNNN: Title

Status: Proposed | Accepted | Superseded

## Context
## Decision
## Consequences
## Alternatives considered
## Validation
```

Accepted records in the initial foundation:

- [0001: FreeBSD 15.1 base](0001-freebsd-15-base.md)
- [0002: Wayland and Wayfire bootstrap](0002-wayland-and-wayfire.md)
- [0003: Qt 6 shell toolkit](0003-qt6-shell.md)
- [0004: Package and update model](0004-package-and-update-model.md)
- [0005: Project application bundle layout](0005-project-app-bundle-layout.md)
- [0006: Narrow update-helper boundary](0006-update-helper-boundary.md)
- [0007: Independent verified-plan update broker](0007-update-broker-verification.md)
- [0008: Transactional package update and boot-environment rollback](0008-transactional-update-and-rollback.md)
- [0009: Image input and privileged builder boundary](0009-image-input-and-builder-boundary.md)
- [0010: One-time first-boot provisioning boundary](0010-first-boot-provisioning-boundary.md)
- [0011: Installer target selection before mutation](0011-installer-target-selection-boundary.md)
- [0012: Installer engine preflight and transaction staging](0012-installer-engine-preflight-boundary.md)
- [0013: Installer source trust and recoverable journal](0013-installer-source-trust-and-journal.md)
