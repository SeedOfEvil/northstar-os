# M5 guarded installer execution foundation

PR83 introduces the fixed privileged consumer for PR82's authenticated,
root-owned transaction. It implements the production command sequence but does
not authorize execution on the persistent development VM and does not close the
installer image gate.

## Authorization boundary

The GUI remains unprivileged. The dedicated PolicyKit action launches only
`/usr/local/libexec/northstar-installer-executor` after administrator
authentication. Its caller can provide only a transaction identifier and an
exact typed whole-disk confirmation.

The executor additionally requires the root-owned marker:

```text
/etc/northstar/installer-execution.conf
```

Its six exact fields declare schema 1, `northstar-installer-media` purpose,
`installer-media` boot environment, explicit execution authorization,
`confirmed-whole-disk` scope, and the exact signed source-manifest digest. The
normal image, installed system, and DEV01 do not receive this marker. The fixed
source path must also be a distinct read-only mount; a root-owned directory on
the writable system filesystem is insufficient.

## Source and payload contract

Source-manifest schema 2 adds `payload_kind=northstar-rootfs-v1`. The verifier
returns and the transaction stores the signed runtime-manifest SHA-256. Before
execution, the archive is listed and bounded; absolute and parent-traversal
paths are rejected. After extraction, the executor verifies the installed
runtime manifest plus the ZFS loader setting, labeled EFI mount, ZFS service,
UEFI loader, Northstar session, shell, and desktop entry.

## Execution and journal phases

The fixed order is:

1. authenticate and bind the installer-media marker;
2. reverify source, target, and payload archive;
3. record `mutation-started`;
4. create GPT plus labeled EFI/ZFS partitions;
5. format EFI and create the ZFS pool/datasets;
6. extract and verify the signed root filesystem;
7. install the UEFI fallback loader and ZFS cache;
8. unmount/export, verify completion, and archive the transaction.

Every phase is appended atomically. Cleanup unmounts EFI and exports the pool.
A post-mutation failure is retained as interrupted with its last phase and
`cleanup-and-restart-required`. PR83 offers no unsafe generic resume. A narrow
`--finalize` operation can complete only the archive publication of a
transaction already recorded as completed and performs no disk mutation.

## Routine validation

```sh
make installer-source-test
make installer-engine-test
make installer-executor-test
make installer-disk-test
make qml-surface-test
make build
ctest --test-dir build --output-on-failure
```

The executor test uses fake GEOM, GPT, filesystem, ZFS, archive, mount, and
state tools under an unprivileged temporary root. It proves exact ordering and
failure cleanup without exposing any host disk. Native execution of those
contracts and a staged-install packaging check run from an immutable DEV01
archive.

The accepted immutable validation evidence is recorded in
[`validation/M5_INSTALLER_EXECUTION_2026-08-12.md`](validation/M5_INSTALLER_EXECUTION_2026-08-12.md).

## Deferred image acceptance

Image checkpoint: M5 Installer Release Candidate

Image status: DEFERRED

Routine evidence: fixed PolicyKit path; disabled production overrides;
installer-media marker; exact active-state, source, target, archive, and typed
confirmation checks; ordered fake-tool execution; installed payload checks;
completion archival; post-ZFS injected failure cleanup; engine-visible
interrupted state; native FreeBSD clean build and tests.

Deferred evidence: protected marker and public-key provisioning in generated
installer media; actual file-backed md/GPT/EFI/ZFS execution on a disposable
privileged builder; physical virtio/SATA/NVMe target installation; reboot after
installation; retry from an interrupted destructive transaction; diagnostic
export; preservation of every non-target disk; and complete noVNC acceptance.
