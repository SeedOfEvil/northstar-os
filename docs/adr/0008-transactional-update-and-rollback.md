# ADR 0008: Transactional package update and boot-environment rollback

Status: Accepted

## Context

The verified-plan broker can prove repository trust and stage a bounded
request, but a safe update also needs privileged orchestration, package target
restriction, failure handling, and an explicit rollback path. The desktop must
not pass command text, repository names, package names, or filesystem paths
across that privilege boundary.

## Decision

Install `northstar-update-transaction` at a fixed root-owned path and authorize
only that executable through PolicyKit. It generates the installed-package
snapshot, asks `northstar-update-broker` to independently reverify the signed
publication, consumes only the broker's bounded package targets, and invokes
`northstar-update-helper` to create the exact named ZFS boot environment before
running a repository-scoped `pkg upgrade`.

The runner verifies every target version after mutation. A package failure or
version mismatch immediately activates the pre-update boot environment for the
next reboot and records `rollback-scheduled`. A successful transaction records
the same environment as rollback eligible. Explicit rollback accepts no boot
environment name from the desktop; it uses only the root-owned transaction
state. `/home` remains outside the root boot environment and is never mutated
by the runner.

## Consequences

Software Center may enable update and rollback controls only when repository
trust, publication signature, catalogue digest, preview, ZFS tools, the fixed
transaction executable, broker, and PolicyKit are all available. Applying or
rolling back requires a confirmation dialog and administrator authentication.
Rollback takes effect after reboot.

Repository enrollment, production signing-key custody, unattended updates,
and automatic reboot are separate decisions.

## Validation

The deterministic root-isolated smoke proves create-before-pkg ordering,
bounded package verification, explicit rollback, injected-failure rollback,
and home-sentinel preservation. The release gate additionally performs an
actual N-1 to N package transaction and `bectl` activation on disposable ZFS
boot environments.
