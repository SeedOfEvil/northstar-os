# M4 transactional update and rollback validation - 2026-08-10

## Scope

PR74 connects the independently verified update broker to a fixed root-only
transaction runner, ZFS boot-environment helper, repository-scoped package
mutation, post-update verification, failure recovery, and explicit rollback.

## Deterministic evidence

- QML surface contracts pass with verified-update and rollback controls.
- The root-isolated transaction smoke creates the bounded boot environment
  before package mutation.
- Successful target versions are verified and recorded as rollback eligible.
- An injected package failure activates the pre-update environment and records
  rollback as scheduled.
- The home-data sentinel survives successful, failed, and rollback paths.

## Native immutable evidence

Pending full archive build, CTest, native N-1 to N package transaction, real
`bectl` activation/reboot rollback, and final VM deployment.

## Manual noVNC acceptance

Pending user validation of the verified update confirmation, PolicyKit prompt,
post-update state, rollback confirmation, reboot-required messaging, window
movement, both themes, and 1280x800 clipping.
