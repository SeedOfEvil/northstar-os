# Tests

Tests are organized by evidence level:

- `unit/` for libraries and Qt components;
- `integration/` for session and service behavior;
- `vm/` for clean install, boot, package, update, and rollback checks;
- `screenshots/` for approved visual regression evidence.

PR 2 adds `unit/test-m0-scripts.sh` for deterministic command stubs and `vm/m0-smoke.sh` for the native Wayland/Xwayland package/session preflight and launch checks. The M1 shell seed adds CMake/CTest Qt tests for `ShellState`, `ApplicationLauncher`, the standard `.desktop` application catalog, and query filtering; the native shell build is exercised through `make test` on FreeBSD. The M2 tests cover the supervisor, its staged Wayland session entry point, and the opt-in supervised nested-session wrapper. `integration/test-shell-session.sh` checks the live unprivileged Wayland shell and its scoped restart behavior. The M4 `vm/pkg-repository-smoke.sh` gate creates a disposable v2 `pkg` catalogue, exercises the documented external RSA signer, records a fingerprint-style trust file, and can run an isolated client `pkg update` without installing or upgrading anything. `unit/test-update-helper.sh` validates the bounded root-owned request protocol without invoking `bectl`, `pkg`, or sudo. `vm/update-broker-smoke.sh` verifies independent publication revalidation and root-owned request staging with fake tools.

Tests must state the required FreeBSD release, packages, privileges, and cleanup behavior.

## Image-test cadence

Image-related source changes do not trigger a complete QCOW2 rebuild and
manual Proxmox import for every PR. Routine PRs use unit/integration contracts,
temporary roots, fake tools, file-backed disk fixtures on disposable builders,
native FreeBSD checks, and focused DEV01 interaction. Their validation record
names the future image checkpoint and marks unperformed image behavior
`DEFERRED`.

Complete image assembly, transfer, fresh Proxmox import, provisioning, and the
full boot/install/update/rollback/noVNC suite run at named release-candidate
checkpoints. The next one is **M5 Installer Release Candidate**, followed by
**M6 Alpha RC** and **M6 Alpha release**. See
[`docs/MILESTONE_IMAGE_VALIDATION.md`](../docs/MILESTONE_IMAGE_VALIDATION.md).

Bootloader, GPT/ZFS layout, package-database placement, or early-service
changes receive the least expensive useful automated smoke first. An
unscheduled manual image cycle requires a recorded risk justification and
explicit operator approval.

PR73 adds `vm/signed-development-repository-smoke.sh`. It packages the real
Northstar install tree, publishes it through external disposable signers and a
resolved input lock, proves an isolated `pkg` client refreshes it, and requires
altered signed catalogues to be rejected without package mutation.
PR74 adds `vm/transactional-update-smoke.sh`, which uses isolated fake broker,
helper, and package-manager boundaries to prove create-before-mutation ordering,
post-update verification, explicit rollback, failure-triggered rollback, and
home-data preservation without touching the host package database or ZFS tree.
PR76 adds `unit/test-runtime-bundle.sh` for the offline package-closure capture
contract. The production capture reads the accepted local pkg database and
matches its exact dependency versions to a previously staged package cache.
Uncached artifacts are recreated with a fixed epoch only when the accepted
installed files are complete. Capture never downloads, installs, upgrades, or
modifies repository configuration.
`unit/test-nested-wayfire-package.sh` proves the accepted patched compositor
tree becomes an immutable, package-managed compatibility artifact instead of
an untracked user-home dependency.
`unit/test-installer-source-verify.sh` generates an external disposable RSA
key, verifies authentic fixed-layout source media, and rejects manifest-digest,
signature, payload, and path tampering. `unit/test-installer-engine.sh` then
proves source-before-target staging, durable sequenced state, interruption
detection, explicit recovery, archival abandonment, and the no-disk-mutation
boundary under a temporary root.
`unit/test-installer-executor.sh` proves the separate installer-media marker,
active transaction, source, target, archive, and typed-confirmation boundaries;
orders fake GPT, EFI, ZFS, extraction, installed-runtime, bootloader, and export
operations; archives success; and injects a post-dataset failure to verify pool
cleanup plus engine-visible interrupted state without exposing a host disk.
`unit/test-image-assembler.sh` exercises the complete non-root QCOW2 preflight
with fixture release sets and package artifacts, including tamper rejection.
The production path additionally requires root, FreeBSD, a protected
disposable-builder marker, and a newly allocated file-backed md device.
PR77 adds `unit/test-image-update-rollback-gate.sh` and the installed-image
`image/scripts/validate-image-update-rollback.sh` acceptance driver. The unit
test simulates both reboot boundaries. Production use is root-only, requires
the installed-image marker and a separate `/home` dataset, and must run only on
a disposable VM restored from the accepted QCOW2. It proves injected package
failure recovery, an actual N-1 to N transaction, explicit boot-environment
rollback, and preservation of a `/home` sentinel.
