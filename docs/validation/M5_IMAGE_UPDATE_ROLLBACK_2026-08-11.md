# M5 installed-image update and rollback validation - 2026-08-11

## Scope

PR77 validates update and rollback from the accepted PR76 QCOW2. The persistent
`NSTAR-DEV01` VM is limited to immutable source build, tests, package creation,
and signed candidate publication. All actual package mutation, injected
failure, boot-environment activation, and reboot evidence belongs to a fresh
disposable import of the accepted image.

## Deterministic evidence

- `tests/unit/test-image-update-rollback-gate.sh` simulates both reboot
  boundaries and passes failure recovery, successful update, explicit rollback,
  and home-sentinel preservation.
- The production driver requires root, FreeBSD, the installed-image marker,
  and a separate `/home` dataset.
- The full repository test suite, immutable FreeBSD archive build, and native
  package publication remain pending.

## Disposable-image evidence

Pending. Record the exact QCOW2 SHA-256, PR77 source commit, candidate package
and signed repository provenance, boot-environment transitions, package
versions, home-sentinel digest, and final `stage=passed` result here before
promotion.
