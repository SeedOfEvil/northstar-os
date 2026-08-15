# Milestone image validation workflow

Northstar does not rebuild, transfer, import, and manually validate a complete
disk image for every pull request. Full-image validation is a milestone release
gate, not a routine feature-PR gate.

This policy keeps development moving while preserving honest image and release
claims. Passing source-level or DEV01 validation never means an unbuilt image
has passed; it means the full-image gate is deliberately deferred to the next
named checkpoint.

## Validation lanes

### Routine development lane

`NSTAR-DEV01` is the normal lane for feature and integration PRs. Depending on
the change, evidence includes:

- focused unit, integration, QML, package-contract, and native FreeBSD tests;
- immutable or commit-named builds from a clean checkout;
- installation under the development prefix and a controlled shell/session
  restart;
- interactive noVNC validation of the affected product behavior;
- file-backed, temporary-root, fake-tool, or model-driven installer and disk
  tests that cannot mutate the host; and
- explicit documentation of the image-level behavior deferred to the next
  checkpoint.

Routine PRs do not require a new QCOW2, transfer to Proxmox, disk swap, fresh
SSH provisioning, or repetition of the complete boot/update/rollback suite.
They may be squash-merged after their applicable routine gates pass.

### Milestone image lane

A full image checkpoint uses a clean, privileged, disposable builder and a
fresh disposable Proxmox import. It validates the integrated result rather
than one small implementation detail. The operator performs the manual noVNC
gate only after artifact integrity, boot smoke, and automated image checks pass.

The accepted **M5 Installer Release Candidate** batched and passed:

- production first-boot and first-administrator setup;
- installer UI, target-disk selection, installation, and destructive-action
  confirmation;
- interrupted-install recovery, retry, diagnostics, and cleanup;
- boot-environment and recovery tooling;
- raw/USB and installer-media generation; and
- branded boot, installed login, desktop, applications, update, rollback, and
  clean shutdown.

Its installation, First Boot, graphical login, signed update, injected-failure
recovery, explicit rollback, and `/home` preservation evidence is retained
under `docs/validation/`. The next full checkpoints are **M6 Alpha RC** and
**M6 Alpha release**. They add the declared VM and Intel/AMD hardware matrix,
native graphics, networking, audio, recovery, diagnostics, and application
coverage.

## Deferring image acceptance on intermediate PRs

An intermediate PR that contributes to a future image checkpoint must state:

```text
Image checkpoint: M6 Alpha RC
Image status: DEFERRED
Routine evidence: <exact tests and DEV01 observations>
Deferred evidence: <boot/install/first-boot behavior to verify in the RC>
```

`DEFERRED` is not a failure and does not block merging when the PR's applicable
routine gates pass. It also cannot be rewritten as `PASS` until the named
checkpoint is built and exercised. The checkpoint validation record must map
every deferred claim back to the merged PR or source commit that introduced it.

## Exceptional early image checks

An unscheduled full manual image cycle is exceptional. It is considered only
when a change affects EFI/bootloader structure, GPT or ZFS root layout, package
database placement, early-boot service ordering, or another boundary where
continuing development without boot evidence would create a material risk of
losing the integration baseline.

Before asking for a manual Proxmox import, use the least expensive useful
evidence first: static contracts, fixture images, file-backed md devices on a
disposable builder, `qemu-img check`, and bounded snapshot-only QEMU boot smoke.
If those checks are insufficient, the PR must record why an early manual image
cycle is necessary and receive explicit operator approval. Convenience or the
existence of an image-related code change is not sufficient justification.

## Artifact and VM retention

Milestone image artifacts are immutable and identified by source commit,
provenance, size, and SHA-256. Retain the accepted artifact and its exact input
set according to the image runbook. Disposable builder and acceptance VMs do
not become development machines and may be retired after evidence is secured.
`NSTAR-DEV01` remains the persistent source-level and interactive development
lane.
