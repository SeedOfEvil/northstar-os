# ADR 0012: Installer engine preflight and transaction staging

Status: Accepted

## Context

The installer UI can safely select and confirm a destination, but UI state is
unprivileged, caller-controlled, and potentially stale. It must never become a
direct instruction to partition a disk. Northstar also needs explicit state so
an interrupted or abandoned installation cannot be hidden by a later request.

## Decision

Introduce a fixed PolicyKit executable, `northstar-installer-engine`, with a
strict versioned request. The request binds a whole-disk device name, media and
sector sizes, description digest, GPT/UEFI/ZFS layout, Northstar pool name,
source-manifest digest, verified-plan state, and explicit erase confirmation.

The protected `--stage` path requires root and a caller-owned mode-0600 request.
It independently queries FreeBSD GEOM and rechecks size, sector size, and the
description digest. It rejects partitions, missing or changed disks, targets
used by mounts, ZFS, or swap, and targets below 16 GiB. It then publishes one
root-owned mode-0600 transaction request under `/var/db/northstar/installer`.
An unresolved transaction cannot be overwritten.

This foundation deliberately implements no partitioning, filesystem creation,
pool creation, extraction, or bootloader installation. Staged state records
`execution=disabled`. A later execution helper must consume this state,
revalidate the target again immediately before mutation, verify the source
manifest against trusted installer media, and maintain a recoverable journal.

## Consequences

- The graphical installer cannot widen the privileged command surface or pass
  shell commands.
- Device drift and newly active disks are rejected after UI confirmation.
- Historical or interrupted transaction state is visible and blocks a new run
  until an explicit recovery flow resolves it.
- Target-description hashing improves accidental-device drift detection but is
  not a unique hardware identity; the execution slice must use stronger GEOM
  identifiers when available and preserve all conservative checks.
- DEV01 can test every contract with fake tools and cannot be used for actual
  disk installation evidence.

## Alternatives considered

Running the QML installer as root was rejected as an unnecessarily broad
privilege boundary. Trusting the UI's eligible flag was rejected because it can
be stale. Passing a device name directly to `gpart` was rejected because it has
no immutable plan or independent preflight. Automatically replacing an old
transaction was rejected because it recreates the historical-state ambiguity
already eliminated from the validation VM workflow.

## Validation

Tests cover bounded fields, whole-disk enforcement, identity drift, active-disk
rejection, root-state staging, unresolved-state refusal, and a static guarantee
that this helper contains no disk mutation command. Native FreeBSD validation
uses only fake targets plus a read-only check that DEV01's system disk remains
ineligible. Real execution is deferred to a disposable installer VM and the M5
Installer Release Candidate.
