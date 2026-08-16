# Tests

Tests are organized by evidence level:

- `unit/` for libraries and Qt components;
- `integration/` for session and service behavior;
- `vm/` for clean install, boot, package, update, and rollback checks;
- `screenshots/` for approved visual regression evidence.

PR 2 adds `unit/test-m0-scripts.sh` for deterministic command stubs and `vm/m0-smoke.sh` for the native Wayland/Xwayland package/session preflight and launch checks. The M1 shell seed adds CMake/CTest Qt tests for `ShellState`, `ApplicationLauncher`, the standard `.desktop` application catalog, and query filtering; the native shell build is exercised through `make test` on FreeBSD. The M2 tests cover the supervisor, its staged Wayland session entry point, and the opt-in supervised nested-session wrapper. `integration/test-shell-session.sh` checks the live unprivileged Wayland shell and its scoped restart behavior. The M4 `vm/pkg-repository-smoke.sh` gate creates a disposable v2 `pkg` catalogue, exercises the documented external RSA signer, records a fingerprint-style trust file, and can run an isolated client `pkg update` without installing or upgrading anything. `unit/test-update-helper.sh` validates the bounded root-owned request protocol without invoking `bectl`, `pkg`, or sudo. `vm/update-broker-smoke.sh` verifies independent publication revalidation and root-owned request staging with fake tools.

Tests must state the required FreeBSD release, packages, privileges, and cleanup behavior.

`unit/test-alpha-readiness.sh` uses deterministic fixtures to classify the
supplemental VM, ready Intel, ready-capability AMD, unsupported DRM, wrong-base,
and incomplete lanes. It also checks the strict `--require-ready` result,
mode-0600 output, and the bounded privacy contract. Native DEV01 collection is
read-only and records current VM capabilities without turning scfb/pixman into
DRM/KMS evidence.

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
cleanup plus engine-visible interrupted state without exposing a host disk. It
also verifies bounded privacy-preserving diagnostics, exact interrupted-journal
ordering, target revalidation, typed retry confirmation, archival preservation,
and a no-mutation clean-retry transition. The Qt recovery-controller test
rejects unexpected diagnostic fields and writes only the sanitized report.
`unit/test-installer-recovery.sh` verifies that the separate PolicyKit-facing
wrapper accepts only the two fixed recovery operations and cannot dispatch
unsafe transaction or device arguments.
`unit/test-image-assembler.sh` exercises the complete non-root QCOW2 preflight
with fixture release sets and package artifacts, including tamper rejection.
The production path additionally requires root, FreeBSD, a protected
disposable-builder marker, and a newly allocated file-backed md device.
`unit/test-installer-media.sh` builds a rootfs fixture with an embedded runtime
manifest, signs its strict source manifest using an external disposable key,
verifies the complete image/source/provenance binding, and rejects a project-
internal key, mismatched manifest, unknown provenance, development-autologin
image, and altered QCOW2. Media assembly preflight is output-free and
disk-device-free. Production raw assembly and boot/install behavior remain
deferred to the M5 Installer Release Candidate.
`unit/test-installer-rc.sh` proves the milestone orchestrator calls QCOW2,
signed-source, and raw-media stages in that exact order, emits a commit-bound
top-level provenance record, creates no preflight output, rejects a private key
inside the project, requires both disposable-builder boundaries, and exposes
no host-disk destination.
PR77 adds `unit/test-image-update-rollback-gate.sh` and the installed-image
`image/scripts/validate-image-update-rollback.sh` acceptance driver. The unit
gate now binds the accepted image and signed candidate identities in schema-2
state. PR89 adds `unit/test-installed-image-update-staging.sh` for the
non-mutating repository and trust-staging boundary before VM execution. The
test simulates both reboot boundaries. Production use is root-only, requires
the installed-image marker and a separate `/home` dataset, and must run only on
a disposable VM restored from the accepted QCOW2. It proves injected package
failure recovery, an actual N-1 to N transaction, explicit boot-environment
rollback, and preservation of a `/home` sentinel.
