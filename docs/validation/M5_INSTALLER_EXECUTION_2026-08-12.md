# M5 guarded installer execution validation - 2026-08-12

## Scope

PR83 adds the fixed privileged executor for an authenticated, staged installer
transaction. Production execution remains disabled unless purpose-built
installer media supplies the exact root-owned authorization marker and a
distinct read-only signed source mount.

Image checkpoint: M5 Installer Release Candidate

Image status: DEFERRED

## Immutable source evidence

- Branch: `codex/m5-installer-execution-foundation`
- Validated code commit: `e8f5071`
- Archive: `northstar-pr83-e8f5071.tar.gz`
- Archive SHA-256:
  `b7349555dfa2678c074186635997364033602b3e39274dc79eb0a50f3414ac5a`
- Native host: `NSTAR-DEV01`, FreeBSD `15.1-RELEASE-p2`, unprivileged
  user `northstar`
- Checkout: `/tmp/northstar-pr83-e8f5071`, separate from the canonical
  development checkout

The Windows and FreeBSD archive digests matched before extraction. The
canonical VM checkout, installed desktop, installer authorization state, and
all real disks were unchanged.

## Results

- POSIX syntax checks passed for every changed shell program and focused test.
- The signed-source, transaction-engine, guarded-executor, read-only disk
  discovery, and QML surface contract suites passed natively on FreeBSD.
- The fake-tool execution harness proved that wrong confirmation, source
  failure, target drift, and journal tampering cause no mutation.
- The harness proved the required GPT, EFI, ZFS, extraction, boot setup, and
  export order, installed-runtime checks, completion archival, idempotent
  finalization, and cleanup after an injected post-ZFS failure.
- The clean native build completed all 326 Ninja steps.
- CTest passed 26 of 26 targets with zero failures.
- The full `make test` contract suite passed from the immutable checkout.
- A separate staged install contained executable source verifier, transaction
  engine, and guarded executor helpers. All three reported their bounded
  capabilities.
- The staged prefix did not contain
  `/etc/northstar/installer-execution.conf`, proving that a routine install
  does not enable destructive execution.
- ShellCheck was unavailable on the VM; `sh -n`, native focused execution,
  the complete shell contract suite, clean build, and CTest supplied the
  routine shell evidence.

## Safety statement

No `gpart`, `newfs_msdos`, `zpool`, `zfs`, mount, archive extraction, or other
disk mutation was run against a real or file-backed FreeBSD device for this
routine PR. The production executor was exercised only through the isolated
fake-tool harness, and DEV01 did not receive the installer-media marker.

## Deferred image acceptance

The M5 Installer Release Candidate checkpoint must provision the protected
marker and public key in generated installer media, mount the release source
read-only, and run actual file-backed `md` GPT/EFI/ZFS installation on a
disposable privileged builder. It must then install to representative
virtio/SATA/NVMe targets, reboot the installed system, prove interruption
cleanup and explicit retry, export diagnostics, preserve every non-target
disk, and complete the noVNC acceptance checklist.
