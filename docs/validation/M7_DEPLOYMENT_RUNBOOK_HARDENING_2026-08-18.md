# Deployment audit lanes and runbook hardening — 2026-08-18

PR #102, verified on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2) at commit `daf9221`.

## No new binary

`git diff --name-only origin/main...HEAD -- src apps packaging` is empty. The
change touches documentation, the auditor, and the auditor's unit test, none of
which are compiled. There is therefore no new commit-named build tree, no
reinstall, and no interactive checklist, because there is no new binary to
accept. The manifest carries the previous `canonical_build`
(`pr101-ce30d9c`), which really is the build the installed prefix came from.

## The problem

The root-owned deployment manifest went unwritten from 2026-08-10 to
2026-08-18 and was recorded as an honest exception in six consecutive
validation documents, each assuming it was repairable debt.

It was not repairable from those pull requests. The auditor cross-checks the
deployed source revision against the signed publication's source revision. An
interface pull request installs under `~/.local` and publishes no package, so
the two legitimately differ, and `docs/M4_SIGNED_DEVELOPMENT_CHANNEL.md` is
explicit that no persistent signing key lives in this repository: signing
material belongs to a protected publication environment outside pull-request
execution.

Meanwhile `docs/QUALITY_GATES.md` required a passing audit unconditionally for
handoff and merge. One check was unsatisfiable in the UI lane and no exception
was expressible, so waiving it in prose was the only way to satisfy the gate.
**A rule that can only be honoured by writing an exception will keep producing
exceptions**, which is exactly what six documents recorded.

## The fix: the expectation is enforced, not written down

The manifest now declares a lane, and the auditor acts on it.

- `lane=package`, and any manifest with no `lane` key, requires the publication
  to have been built from the deployed source revision. Omitting the key can
  therefore only make an audit stricter, never weaker.
- `lane=ui` reports that single divergence as
  `NOTE: deployed source is ahead of the signed publication`, which is neither
  a failure nor a warning, and relaxes nothing else.

Because the lane is declared, a correctly written UI manifest passes, so
`docs/QUALITY_GATES.md` returns to requiring **zero failures in both lanes**,
with any failure blocking handoff and merge. There is no longer a case where
the only way to satisfy the gate is to write an exception.

Verified against this handoff's own manifest:

```
NOTE: deployed source is ahead of the signed publication, expected in the ui lane
PASS: canonical ui lane deployment is coherent (1 warning(s))
```

The single warning is the retained predecessor build tree, which the retention
rule expects.

## A check that read the manifest instead of the disk

`/usr/local/etc/pkg/repos/northstar-development.conf` pointed `pkg` at
`file:///home/northstar/validation/development-channel-r78`, a directory that
had been removed, so the Northstar development repository was non-functional.

The auditor's "active pkg repository points at the canonical publication" check
reported **PASS** throughout, because it only compared the configuration
against the manifest and the manifest had gone stale in the same direction.
Two copies of the same wrong value agreed with each other. The wider audit did
fail on the missing r78 artifacts, but the one check whose job was to describe
the package manager's configuration said it was correct.

The auditor now also reads the filesystem: every `file://` path named by the
active configuration must exist. This cannot be defeated by a manifest that
drifted alongside it.

The repository configuration and the trust store were repaired during this
cycle. `pkg` now names r86, and the trusted fingerprint is r86's
`2edc7ddb…` rather than r78's `ba1a1b56…`.

## Also recorded in the runbook

- A pre-flight checklist, and what each deployment step must leave updated.
- Where every manifest value comes from, with digests computed from the
  published files rather than copied forward.
- How to prove a gate works, after one added this cycle turned out to be
  incapable of catching its own removal.
- A catalogue of traps, each of which actually happened: the install target
  that reconfigures the tree it installs from, the build directory defaulting
  inside the checkout, hash comparison defeated by RPATH rewriting, Qt literals
  invisible to plain `strings`, the session entry point that must be refreshed
  rather than deleted because the display manager resolves it by name, and
  tests that write the real desktop's state when constructed with default
  paths.

## Evidence

Run natively on DEV01.

- `sh tests/unit/test-validation-deployment-audit.sh` — passed, both cases. It
  covers each lane, the stricter default when `lane` is absent, that the UI
  lane relaxes exactly one check and still fails on a missing shell, an
  unsupported lane value, and the stale-together pkg configuration.
- The new test was confirmed to **fail against the previous auditor**
  (`FAIL: audit did not report the removed repository directory`) and to pass
  against this one, so it is not decorative.
- `sh tests/unit/test-qml-surfaces.sh` — passed.
- `sh tests/unit/test-session-entrypoint.sh /home/northstar/builds/pr101-ce30d9c`
  — passed.
- Both shell files parse under `sh -n`.
- `git diff --check` — exit 0.

## Not claimed

The audit result above is from a dry run against the prepared manifest with
`--allow-unprivileged-manifest`. Installing the manifest as root is a
privileged step performed by the operator; the audit is re-run against the
installed copy afterwards.

Physical Intel/AMD DRM/KMS acceptance, native compositor quality, and
multi-display evidence remain deferred.
