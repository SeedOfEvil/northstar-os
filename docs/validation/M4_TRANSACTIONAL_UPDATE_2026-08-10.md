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

- The separate commit archive build completed all 262 Ninja steps on
  NSTAR-DEV01. All 22 CTest targets, QML contracts, broker smoke, and
  root-isolated transaction smoke passed after correcting one stale status-text
  assertion.
- CPack generated native `northstar-0.0.9` and `northstar-0.1.0` packages with
  origin `x11/northstar` from the same install tree.
- A signed revision-74 repository published `northstar-0.1.0`; catalogue digest
  `b4742125b3c00208d73cb7b0b6ef2bde6563fa710b4aa8f004b544fccf467b04`
  and signing fingerprint
  `4c33c5b07f363e1211a4f89a9ead9c9cbb8a39185ed7e0ed1f52608f5607a924`
  were independently verified by the broker.
- The protected transaction created
  `northstar-before-development-r74-1707062` before upgrading the real system
  package from `0.0.9` to `0.1.0` and verifying the target version.
- Explicit rollback activated that environment. After reboot, `pkg info`
  reported `northstar-0.0.9` and the home sentinel remained unchanged.
- Reactivating `default` and rebooting restored `northstar-0.1.0`; the home
  sentinel still remained unchanged.
- The proven disposable boot environment was then destroyed and the VM reset
  to `northstar-0.0.9`, leaving a real signed `0.1.0` update pending for manual
  Software Center acceptance.

## Manual noVNC acceptance

Pending user validation of the verified update confirmation, PolicyKit prompt,
post-update state, rollback confirmation, reboot-required messaging, window
movement, both themes, and 1280x800 clipping.
