# Shell surface teardown and deployment manifest repair — 2026-08-18

PR #101, validated on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2, Proxmox/noVNC),
built from commit `ce30d9c` in `/home/northstar/builds/pr101-ce30d9c` with the
project's canonical `-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.

## What changed

`main()` collected the QML surfaces and their contexts and then discarded both
with `Q_UNUSED`. The surfaces outlived every controller declared above them, so
when those controllers were destroyed on return their still-live bindings
re-evaluated against them. The offscreen self-test emitted seven QML
`TypeError`s at shutdown. The same thing happened on normal exit, where nobody
saw it.

Teardown is now a scope guard declared after the controllers. It is destroyed
before them and runs on every path out of `main()`, including the two early
failure returns that previously leaked the surfaces they had already created.

## A gate that did not work, and why it is not the fix

The first attempt counted QML warnings during the self-test and failed on any.
Deleting the teardown call to check that guard proved it could not catch its
own removal:

- QML warnings reported: **7**
- Self-test exit code: **0**

The warnings are emitted while `main()`'s locals are destroyed, which is after
the self-test has already returned its status, so the count was checked before
the errors existed. The collector saw all seven and the process still passed.

That is why the teardown is structural rather than gated. The warning check is
kept, and the code now states its real scope: it sees binding errors from
startup, from the self-test actions, and from the explicit early teardown, and
nothing emitted after `main()` returns. A binding error is invisible in a
running session but means the bound surface silently rendered nothing, so it is
worth failing a gate over where a gate can actually see it.

## Automated evidence

- Clean build, all 394 targets, 0 errors.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr101-ce30d9c --output-on-failure`
  — **34/34 suites passed, 0 failed**.
- The offscreen self-test emits **0** QML errors, down from 7. The remaining
  output is LayerShellQt reporting that an offscreen window is not a Wayland
  window, and the offscreen plugin reporting that it does not support
  `raise()`. Both are expected under `QT_QPA_PLATFORM=offscreen` and are not
  QML errors.
- The regression was reproduced deliberately, as recorded above, rather than
  assumed.

## Deployment manifest

The root-owned manifest at `/usr/local/etc/northstar/validation-deployment.conf`
had described PR74 and repository r78 since 2026-08-10, and was recorded as an
honest exception in six consecutive validation documents.

`docs/VM_VALIDATION_DEPLOYMENT.md` specifies that the manifest is rewritten at
every handoff to describe that handoff. It was not a snapshot that had gone
stale; it had simply stopped being maintained once DEV01 began serving M7 UI
work. It cannot be repaired between pull requests, because the audit requires
the canonical checkout to sit on the pull request's `codex/` branch, which is
only true during a handoff. This handoff is the first opportunity to write it
correctly rather than waive it.

Every value in the new manifest is real. The catalogue and metadata digests
were computed from the published files and **match the r86 publication record
exactly**, which independently verifies that repository's integrity. No hash,
build path, or provenance value was invented; where a true value was not
recoverable it was not guessed.

The audit result improves from **7 failures** to **2**:

- `publication source revision does not match the manifest` — true and
  structural. The deployed source is `ce30d9c`; the last signed package was
  built from `448b297`. The audit ties those together because the runbook
  expects every changed source commit to receive a new signed repository
  revision. M7 UI pull requests install under `~/.local` and publish no
  package, so the two legitimately diverge. Closing this requires publishing a
  signed revision, which is a deliberate key-handling operation and is not
  done here.
- `active pkg repository does not point at .../development-channel-r86` — see
  below.

`448b297` cannot be recovered on DEV01: it is an orphaned commit from a
squash-merged branch and has been pruned there. That deployment can never be
reconstituted, which is why the manifest describes the current one instead.

## A broken deployment the stale manifest was hiding

`/usr/local/etc/pkg/repos/northstar-development.conf` points `pkg` at
`file:///home/northstar/validation/development-channel-r78`, a directory that
no longer exists. The Northstar development repository is therefore
non-functional on DEV01.

The audit passed that check for six slices **because the manifest was equally
stale** — it was comparing two copies of the same wrong value. A stale manifest
did not merely fail to describe the deployment; it actively masked a real
fault. That is the strongest available argument for maintaining it per handoff.

Repair needs two root-owned changes, neither of which touches the immutable
r86 tree:

1. Repoint the repository URL at `development-channel-r86`.
2. Install r86's trusted fingerprint. The system trust store still holds r78's
   `ba1a1b56…`, while r86 is signed with `2edc7ddb…`, so signature
   verification would fail even after the URL is corrected.

Both are staged on DEV01 for a privileged operator to install. They are trust
decisions and were deliberately not applied automatically.

## Stale system session entry points

`/usr/local/bin/northstar-session`, `northstar-session-x11`, and
`northstar-power` dated 2026-08-10 were refreshed from the current source and
are now `root:wheel`, mode `0755`. The Aug 10 originals are preserved under
`/home/northstar/quarantine/20260818-stale-usr-local-session/`.

Deleting the stale `northstar-session-x11`, which earlier documents suggested,
would have broken login: `/usr/local/share/xsessions/northstar-proxmox.desktop`
runs `Exec=northstar-session-x11` **by name**, resolved through SDDM's `PATH`,
which does not include `~/.local/bin`. Refreshing it removes the shadowing
hazard because both copies are now identical.

One behavioural difference was checked before accepting the refresh: the
current script prefers `/usr/local/libexec/northstar-wayfire-nested/bin/wayfire`
over the home-directory compositor. That path is absent, so resolution falls
through to the compositor already running, and next login is unchanged.

## Interactive acceptance

Pending.

## Not claimed

`make validation-deployment-audit` is **not** claimed to pass. It now reports 2
failures instead of 7, and both are true statements about DEV01 rather than
stale pointers.

Physical Intel/AMD DRM/KMS acceptance, native compositor quality, and
multi-display evidence remain deferred. The Proxmox scfb/pixman VM remains
supplemental product evidence only.
