# M5 installer recovery and diagnostics

PR84 gives interrupted guarded installations a visible, non-destructive
recovery workflow. It does not close the integrated installer image gate and
does not resume a partially completed installation.

## User workflow

From the installer, choose **Recovery** and authenticate to inspect protected
state. An interrupted execution shows the transaction, exact target, last safe
phase, and whether disk mutation began. **Export Diagnostics** writes an atomic
sanitized report to `Documents/Northstar Installer Diagnostics`. The report
contains bounded provenance and phase fields only.

To retry, type the exact target device and choose **Prepare Clean Retry**. The
privileged helper revalidates the installer-media marker, source binding,
transaction, complete phase order, target GEOM identity, and target quiescence.
It archives the failed attempt and releases the installer engine without
issuing disk commands. Return to destinations, rediscover the disk, and create
a brand-new reviewed plan.

## State model

```text
executing
  -> interrupted / cleanup-and-restart-required
  -> diagnostics exported (state unchanged)
  -> retry-prepared journal event + archived failed attempt
  -> idle engine
  -> new destination review and new transaction
```

There is no transition from `interrupted` back into an execution phase. A
completed transaction awaiting only final archive publication continues to use
the executor's disk-free idempotent `--finalize` path.

## Privacy and privilege boundary

The GUI remains unprivileged. Status uses the protected engine; diagnostics and
protected status, diagnostics, and retry preparation use the fixed
`northstar-installer-recovery` PolicyKit path.
The wrapper accepts no source path, output path, pool name, command, or generic
operation. The executor returns a strict report rather than raw files. The Qt
controller rejects duplicate, unknown, contradictory, malformed, or oversized
records before writing anything.

The diagnostic report excludes credentials, keys, usernames, home paths,
environment, raw request and journal content, logs, payload contents, and mount
paths. The destination file is user-owned and atomically replaced.

## Routine validation

```sh
make installer-engine-test
make installer-executor-test
make installer-recovery-test
make qml-surface-test
make build
ctest --test-dir build --output-on-failure
```

Routine tests use temporary state and fake disk tools. They must prove that
wrong confirmation, target drift, journal tampering, malformed diagnostics,
and unsafe wrapper arguments make no disk-tool call.

The accepted immutable routine evidence is recorded in
[`validation/M5_INSTALLER_RECOVERY_2026-08-12.md`](validation/M5_INSTALLER_RECOVERY_2026-08-12.md).

## Deferred image acceptance

Image checkpoint: M5 Installer Release Candidate

Image status: DEFERRED

The release-candidate image must inject an actual failure after disk mutation
on a disposable target, reboot or restart the installer environment, display
the interrupted state, export and retain diagnostics, prepare a clean retry,
repeat installation from a new transaction, boot the installed system, and
prove every non-target disk remains unchanged.
