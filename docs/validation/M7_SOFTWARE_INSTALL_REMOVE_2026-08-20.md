# Software installation and removal — 2026-08-20

PR #115 was accepted on NSTAR-DEV01 at source revision
`5a37ae004b3b994f3226b0e54e4c43547d00d87d` on branch
`codex/software-install-remove`. The exact clean validation build is
`/home/northstar/builds/pr115-5a37ae0`, installed into
`/home/northstar/.local`.

## What this changes

The Software Center can prepare an exact preview for installing or removing a
permitted FreeBSD package, obtain administrator authorization, and execute
that same transaction through a narrowly scoped root helper. The privileged
side independently validates the root-owned source policy, refreshes the
catalogue, reproduces the preview, creates a ZFS boot environment, performs
the mutation, and verifies the result. `/home` is not part of the boot
environment rollback.

Installed dependencies are visible, but they are not direct removal targets.
Pinned packages and packages outside the configured source policy are also
not permitted mutation targets.

## Trust boundary deployed on NSTAR-DEV01

| Artifact | Mode | SHA-256 |
| --- | ---: | --- |
| `/usr/local/libexec/northstar-package-transaction` | `0755` | `ced767836b64837671b0eef4eb3ab48047a184f6782f9623c0816a6dfccbb4b9` |
| `/usr/local/share/polkit-1/actions/org.northstar.package.policy` | `0644` | `609e975d0aa4a0b4828ce3fa7076159dc079dc10bc61f56c1b131afc40c2a80a` |
| `/usr/local/etc/northstar/third-party-package-source.conf` | `0644` | `a9c846c4de4fc2cd55f1ecd3fb763a5aa0d7713f95fcceef9c099d386d04fb64` |

All three are owned by `root:wheel`. The UI passes an opaque, short-lived plan
identifier rather than an arbitrary package command.

## Defects found during acceptance

The walkthrough found and fixed four integration faults before acceptance:

1. FreeBSD `pkg install -n` emitted a valid preview but returned nonzero. Both
   preview paths now use the explicit non-interactive dry-run form.
2. UI and helper catalogue ordering differed for mixed-case package names.
   Plan identity now uses one byte-stable canonical ordering.
3. The privileged preview contained implicit repository-update chatter, so its
   digest could not match the user's preview. Both paths now disable implicit
   auto-update, while the helper still performs its explicit refresh first.
4. A successful transaction initially surfaced package progress before its
   completion markers. The dialog now leads with an explicit success status
   and preserves the complete boot-environment identifier.

The final direct install preview digest matched on both sides:
`2748e72eef9b030c3624de046d937782a31dad448087790da86f5bff244fe325`.
The direct removal previews were byte-identical.

## Automated evidence

- The clean final build completed all 423 build steps.
- `ctest --test-dir /home/northstar/builds/pr115-5a37ae0
  --output-on-failure` passed **38/38 suites, 0 failed** in 21.29 seconds.
- QML surface, session supervisor, and session-entrypoint unit gates passed.
- The root-isolated `package-mutation-smoke` target passed install and removal,
  opaque-plan and ordering checks, verification, rejection, rollback, and
  `/home` preservation cases.
- The installed shell's offscreen QML self-test passed.
- The live Wayland session integration smoke passed, including a shell restart
  with the compositor left running. The session recorded restart count `1/3`.
  There were zero qterminal clients at the time, so this run does not claim a
  nonzero terminal client was preserved.

## Interactive acceptance

Accepted by Hector through NSTAR-DEV01's 1280x800 noVNC session on 2026-08-20:

- The `cowsay 3.04_3` installation preview identified source
  `FreeBSD-ports`, origin `games/cowsay`, one package, and a 29 KiB download.
  It showed the ZFS boot-environment warning and required administrator
  authentication.
- Closing the review made no package change.
- The authenticated install completed. `pkg` then reported
  `cowsay|3.04_3|0|FreeBSD-ports|games/cowsay`.
- The install recorded boot environment
  `northstar-before-package-1787273631-1fb0e5ae`.
- `Imath`, installed as a dependency, showed that direct removal was not
  permitted and its **Review Removal** action remained disabled.
- The authenticated `cowsay` removal completed and `cowsay` was absent
  afterward.
- The removal recorded boot environment
  `northstar-before-package-1787273774-78dfefe0`.
- `bectl list` showed both boot environments after the two real transactions.

The final transaction state was `completed` for removal. The real package used
for acceptance was left uninstalled.

## Deployment audit

After acceptance, `/usr/local/etc/northstar/validation-deployment.conf` was
updated once to name the exact tested branch, revision, and build. The signed
publication fields remained unchanged at development channel revision 86.

`tools/audit-validation-deployment.sh` passed the checkout revision, branch,
cleanliness, build, installed development shell, signed publication, catalogue,
metadata, package artifact, active repository, and quarantine-root checks. It
reported the expected UI-lane note that deployed source is ahead of the signed
publication and 14 retention warnings for historical builds; it reported no
failures and concluded that the canonical UI-lane deployment is coherent.

## Scope

This is supplemental VM/noVNC validation. It does not claim physical-machine
DRM/KMS, GPU, radio hardware, or full image-rebuild coverage. The radio
checklist remains a separate hardware-dependent gate.

Status: **accepted**.
