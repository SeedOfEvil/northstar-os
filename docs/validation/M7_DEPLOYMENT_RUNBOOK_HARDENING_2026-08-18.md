# Deployment runbook hardening — 2026-08-18

PR #102, verified on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2) at commit `fa24ae4`.

## Documentation-only

`git diff --name-only origin/main...HEAD -- src apps tests packaging` is empty:
no buildable source changed. Per the rule this pull request itself adds, there
is no new commit-named build tree, no reinstall, and no interactive checklist,
because there is no new binary to accept.

## Why this change exists

The root-owned deployment manifest went unwritten from 2026-08-10 to
2026-08-18 and was recorded as an honest exception in six consecutive
validation documents, each assuming it was repairable debt.

It was not repairable from those pull requests. The auditor cross-checks the
deployed source revision against the signed publication's source revision, and
a user-interface pull request installed under `~/.local` publishes no package,
so the two legitimately differ. `docs/M4_SIGNED_DEVELOPMENT_CHANNEL.md` is
explicit that no persistent signing key lives in this repository: signing
material belongs to a protected publication environment outside pull-request
execution. A routine UI handoff therefore cannot close that check.

Meanwhile `docs/QUALITY_GATES.md` stated a passing audit as an unconditional
requirement for handoff and merge. With one check unsatisfiable in the UI lane
and no stated exception, waiving it in prose was the only move left. The gate
was not being ignored; it was impossible as written.

## What is now recorded

- **Per-lane audit expectations.** Zero failures in the package lane; exactly
  one known failure in the UI lane, named, with the reason it cannot be closed
  there. Everything else blocks in both lanes.
- **The manifest is rewritten every handoff in both lanes**, with a table of
  where each value comes from and an instruction to compute digests from the
  published files rather than copy them forward.
- **A pre-flight checklist** covering branch synchronisation, VM idleness,
  checkout cleanliness, disk headroom, retained builds, and recording the
  running PID so a later restart can be proven.
- **What each deployment step must leave updated**, with the canonical
  configure flags, the gates that are not part of `ctest`, and installation via
  `cmake --install` from the tested tree.
- **How to prove a gate works**, and the record of one added this cycle that
  could not catch its own removal.
- **A catalogue of traps**, each of which actually happened.

## The fault a stale manifest was hiding

`/usr/local/etc/pkg/repos/northstar-development.conf` points `pkg` at
`file:///home/northstar/validation/development-channel-r78`, which no longer
exists. The Northstar development repository has been non-functional on DEV01
since that directory was removed.

The auditor's "active pkg repository points at the canonical publication" check
passed throughout, **because the manifest named r78 as well**. It compared two
copies of the same wrong value. This is the strongest argument in the runbook
for maintaining the manifest per handoff: a stale manifest does not merely fail
to describe the deployment, it can conceal a broken one behind a passing check.

The repair is staged on DEV01 and is a trust decision for a privileged
operator, because the system trust store still holds r78's `ba1a1b56…` while
r86 is signed with `2edc7ddb…`. It is deliberately not applied here.

## Evidence

- `sh tests/unit/test-qml-surfaces.sh` — passed.
- `sh tests/unit/test-session-entrypoint.sh /home/northstar/builds/pr101-ce30d9c`
  — passed.
- `git diff --check` — exit 0.
- Checkout clean on `codex/m7-deployment-runbook-hardening` at `fa24ae4`.

## Not claimed

`make validation-deployment-audit` is not claimed to pass. Against the manifest
installed during PR #101 it reports two failures: the expected UI-lane
`publication source revision` mismatch, and the `pkg` configuration fault
described above, which remains open pending the staged trust decision. Once
that is applied, the expected steady state for this lane is one failure.

Physical Intel/AMD DRM/KMS acceptance, native compositor quality, and
multi-display evidence remain deferred.
