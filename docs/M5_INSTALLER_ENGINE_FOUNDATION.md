# M5 installer engine foundation

This slice establishes the protected boundary between Northstar's installer UI
and future disk mutation. It stages a reviewed transaction but cannot install
Northstar yet.

## Request contract

The version-1 request includes exactly:

- operation `install`;
- whole-disk target name, media size, sector size, and description SHA-256;
- layout `gpt-uefi-zfs` and a bounded `nstar_` pool name;
- installer source-manifest SHA-256;
- `confirmation=erase-target`; and
- `plan_status=verified`.

Unknown, duplicate, missing, malformed, partition, undersized, changed, or
active targets are rejected. The protected path authenticates through the
fixed PolicyKit executable and stages one root-owned transaction. Existing
transaction state blocks replacement.

The original staged state said `execution=disabled`. PR83 changes that field to
`execution=guarded-executor-only`; the engine itself still has no calls to
`gpart`, `newfs`, `zpool create`, `dd`, extraction, or package installation.
Execution is a separate PolicyKit boundary documented in
[`M5_INSTALLER_EXECUTION_FOUNDATION.md`](M5_INSTALLER_EXECUTION_FOUNDATION.md).

## Routine validation

```sh
make installer-engine-test
make installer-disk-test
make qml-surface-test
make build
ctest --test-dir build --output-on-failure
```

All staging tests use a temporary root and fake FreeBSD metadata tools. Never
stage a real transaction on `NSTAR-DEV01`; its only disk is the running system
disk and must remain ineligible.

## Deferred image acceptance

Image checkpoint: M5 Installer Release Candidate

Image status: DEFERRED

Routine evidence: request and permission contracts, fake-target identity drift
and active-use rejection, root-state transaction staging, unresolved-state
guard, native FreeBSD tests, and read-only DEV01 system-disk exclusion.

Follow-on evidence: trusted installer-source verification, root-owned progress
journaling, and explicit interruption recovery are implemented in
[`M5_INSTALLER_SOURCE_AND_JOURNAL.md`](M5_INSTALLER_SOURCE_AND_JOURNAL.md).

Follow-on evidence: the fixed executor, final source/target/archive checks,
ordered destructive phases, completion archive, and interrupted cleanup state
are implemented in
[`M5_INSTALLER_EXECUTION_FOUNDATION.md`](M5_INSTALLER_EXECUTION_FOUNDATION.md).

Deferred evidence: actual execution on a disposable target, destructive retry,
installed boot, and preservation of every non-target disk.
