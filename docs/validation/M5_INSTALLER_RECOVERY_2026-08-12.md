# M5 installer recovery and diagnostics validation - 2026-08-12

## Scope

PR84 adds visible interrupted-install status, bounded diagnostic export, and a
clean-retry transition that archives the failed attempt. It never resumes a
partially completed destructive phase.

Image checkpoint: M5 Installer Release Candidate

Image status: DEFERRED

## Immutable source evidence

- Branch: `codex/m5-installer-recovery-diagnostics`
- Validated implementation commit: `8a39ebe`
- Archive: `northstar-pr84-8a39ebe.tar.gz`
- Archive SHA-256:
  `a55478c24a6114d8ecb9478f0a1417ef183518ab57aee6edf14d5123897d5ff1`
- Native host: `NSTAR-DEV01`, FreeBSD `15.1-RELEASE-p2`, unprivileged
  user `northstar`
- Checkout: `/tmp/northstar-pr84-8a39ebe`, separate from the canonical
  development checkout

The Windows and FreeBSD archive digests matched before extraction. The
canonical VM checkout, installed desktop, root-owned installer state, execution
marker, and all real disks were unchanged.

## Results

- POSIX syntax checks passed for the engine, executor, recovery wrapper, and
  focused shell tests.
- The native engine, executor, recovery-wrapper, and QML surface contracts
  passed.
- The interrupted-execution harness injected failure after dataset creation,
  verified pool export and halted extraction, and retained the exact last safe
  phase.
- Diagnostics validated the complete ordered interruption journal and exposed
  only the 18-field allowlist with `PRIVATE_DATA=excluded`; raw state files,
  logs, home paths, and credentials were absent.
- Wrong typed confirmation, changed GEOM identity, and journal tampering were
  rejected before retry preparation and made no disk-tool call.
- Successful retry preparation appended `retry-prepared`, preserved the failed
  transaction in the archive, removed only the active pointer, reported
  `DISK_MUTATION=none`, and returned the engine to idle.
- The fixed PolicyKit-facing wrapper rejected unsafe operations, transaction
  identifiers, device values, and option shapes before dispatch.
- The clean archive compiled the 69-step focused installer/recovery target set,
  then completed all 263 remaining project build steps.
- Focused installer CTest passed 3 of 3 targets. The complete repository gate
  passed 27 of 27 CTest targets with zero failures.
- The Qt controller accepted idle and interrupted records, required exact
  confirmation, atomically exported sanitized diagnostics, rejected an unknown
  private field, and reached `retry-ready` only after a valid helper result.
- A separate staged install contained executable engine, executor, and recovery
  helpers plus the dedicated recovery PolicyKit path. Recovery capabilities
  reported `disk_mutation=none`.
- The staged prefix did not contain
  `/etc/northstar/installer-execution.conf`, so routine installation still does
  not authorize disk execution.
- ShellCheck was unavailable on DEV01; `sh -n`, native focused execution, the
  full shell contract suite, native compilation, and CTest supplied the routine
  shell evidence.

## Safety statement

No real or file-backed FreeBSD disk was partitioned, formatted, mounted,
pooled, extracted to, or otherwise modified. All destructive command names
were isolated fake tools under an unprivileged temporary root. PR84 did not
install or enable privileged project files on DEV01.

## Deferred image and interaction evidence

No routine QCOW2 rebuild or noVNC handoff was performed. The integrated M5
Installer Release Candidate must create a real interrupted target on a
disposable VM, restart or reboot installer media, render the recovery screen at
1280x800, authenticate the dedicated PolicyKit prompt, export and inspect the
diagnostic file, prepare a clean retry, complete a new installation, boot its
installed system, and prove every non-target disk remains unchanged.
