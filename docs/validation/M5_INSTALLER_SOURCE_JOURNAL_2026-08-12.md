# M5 installer source and journal validation - 2026-08-12

## Scope

PR82 adds authenticated installer-source verification and recoverable,
root-owned transaction journaling. The slice is deliberately non-destructive;
installer execution remains disabled.

Image checkpoint: M5 Installer Release Candidate

Image status: DEFERRED

## Immutable source evidence

- Branch: `codex/m5-installer-source-journal`
- Validated code commit: `60ad1c0`
- Archive: `northstar-pr82-60ad1c0.tar.gz`
- Archive SHA-256:
  `9556cd4f6f363e644aefaba68099ddded83eaf70e29bf769493f9f415b2c33d3`
- Native host: `NSTAR-DEV01`, FreeBSD `15.1-RELEASE-p2`, unprivileged
  user `northstar`
- Checkout: `/tmp/northstar-pr82-60ad1c0`, separate from the canonical
  development checkout

The Windows and FreeBSD archive digests matched before extraction. The
canonical VM checkout and live desktop deployment were not changed.

## Results

- POSIX syntax checks passed for the verifier, engine, and focused tests.
- The temporary-key source contract accepted authentic media and rejected a
  mismatched reviewed digest, modified payload, altered detached signature,
  and signed path traversal.
- The engine contract rejected source failure before active state, revalidated
  the target, staged a mode-bounded transaction and four-event journal,
  detected request and journal tampering, rejected duplicate staging, detected
  interrupted publication, recovered it explicitly, and archived explicit
  abandonment.
- Read-only installer disk discovery kept active and undersized targets
  ineligible.
- All QML surface contracts passed.
- The clean native build completed all 326 Ninja steps.
- CTest passed 26 of 26 targets with zero failures.
- A separate staged install contained executable
  `northstar-installer-source-verify` and `northstar-installer-engine` helpers;
  both reported the expected no-mutation capabilities.
- ShellCheck was unavailable on the VM; `sh -n`, native focused execution, the
  complete shell contract suite, clean build, and CTest provided the routine
  shell evidence.

## Deferred evidence

No QCOW2 rebuild or noVNC interaction was required because this routine slice
does not change the desktop or image boot path. Public-key provisioning,
read-only release-media mounting, destructive GPT/UEFI/ZFS execution,
destructive-phase interruption recovery, installed-system boot, and non-target
disk preservation remain deferred to the integrated M5 Installer Release
Candidate checkpoint.
