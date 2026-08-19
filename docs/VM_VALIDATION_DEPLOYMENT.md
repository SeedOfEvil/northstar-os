# Canonical VM validation deployment

This runbook is the project-owned contract for deploying a pull-request build
to `NSTAR-DEV01`. It prevents the canonical checkout, build trees, signed
repositories, installed shell, and rollback evidence from drifting into an
unreviewed collection of historical deployments.

This is the routine PR validation lane. It does not require a fresh QCOW2 or
Proxmox disk import for each PR. Full image cycles are batched under
[`docs/MILESTONE_IMAGE_VALIDATION.md`](MILESTONE_IMAGE_VALIDATION.md); image-
level claims from intermediate PRs remain deferred to their named checkpoint.

## Invariants

- `/home/northstar/src/northstar` is the only canonical checkout. It is clean,
  on the PR's `codex/` branch, and exactly matches the pushed commit.
- One build directory is canonical for the handoff:
  `/home/northstar/builds/pr<PR>-<short-commit>`.
- Signed repositories are immutable. A changed source commit or package always
  receives a new package version, signing identity, and monotonically
  increasing `development-channel-r<revision>` directory. Never overwrite an
  already published revision.
- The active deployment and its immediate predecessor are retained until
  interactive acceptance. Everything older must be moved intact beneath one
  dated quarantine root; nothing is permanently deleted during acceptance.
- Signing keys remain root-owned beneath `/var/db/northstar/signing/r<revision>`
  and never enter the checkout, build, publication, PR runner, or user config.
- The installed system package may be either the recorded baseline or candidate
  while update/rollback testing is active. The development session always runs
  the binary installed from the canonical build under `~/.local`.
- ZFS snapshots and the named boot environment remain present until update,
  rollback, reboot, and home-preservation acceptance are complete.
- Local and remote branches must match before a VM handoff. A PR is
  squash-merged only after the focused noVNC checklist passes.
- The root-owned manifest is rewritten at every handoff. It is a record of the
  current deployment, never a snapshot carried forward. A manifest and an
  active `pkg` configuration that have drifted together will agree with each
  other and hide the drift from the auditor, so verify the repository the
  configuration names actually exists.

## Root-owned deployment manifest

Every handoff installs `/usr/local/etc/northstar/validation-deployment.conf` as
root, mode `0644`, using schema 2. Values are data, not shell code. Required
keys are:

```ini
schema_version=2
canonical_checkout=/home/northstar/src/northstar
canonical_build=/home/northstar/builds/pr74-eef25c9
source_branch=codex/m4-transactional-update-rollback
source_revision=<full-lowercase-commit>
development_prefix=/home/northstar/.local
repository_revision=76
repository_path=/home/northstar/validation/development-channel-r76
previous_repository_path=/home/northstar/validation/development-channel-r75
package_file=/home/northstar/validation/development-channel-r76/northstar-0.1.2-amd64.pkg
package_sha256=<sha256>
catalogue_sha256=<sha256>
metadata_sha256=<sha256>
signature_fingerprint=<sha256>
active_repository_config=/usr/local/etc/pkg/repos/northstar-development.conf
quarantine_root=/home/northstar/quarantine/pre-pr74-acceptance
```

The manifest may also record package baseline/candidate versions, login-session
descriptors, snapshots, boot environments, and preparation time. Those fields
are evidence; the required schema-2 fields above are enforced by the auditor.

## Before every handoff

Run these checks before touching the VM. Each one exists because skipping it
has cost a rebuild, a false evidence claim, or a broken session.

| Check | Command | Why |
| --- | --- | --- |
| Local `main` matches origin | `git rev-parse main origin/main` | A handoff from a stale `main` produces a branch nobody can merge cleanly. |
| The pushed commit is recorded | `git rev-parse HEAD` | Every later artifact is named after it. Never build from "the branch"; build from the commit. |
| VM is idle | `pgrep -f "pkg\|ninja\|cmake --build"` | Step 4 and a running package transaction must never overlap. |
| Canonical checkout is clean | `git -C /home/northstar/src/northstar status --short` | A dirty tree is quarantined, never reset destructively. |
| Disk headroom | `df -h /home` | A build tree is ~340 MB; a failed build mid-way is harder to diagnose than a refused one. |
| Retained build trees | `ls /home/northstar/builds` | Only the active deployment and its immediate predecessor are kept. |
| The running session | `pgrep -lf northstar-shell` | Note the PID now so a later restart can be proven to have happened. |

Two things about the VM's Git configuration are easy to trip over:

- The canonical checkout's remote refspec fetches **only** `refs/heads/main`.
  `git fetch origin <branch>` therefore creates no tracking ref, and
  `git checkout -B <branch> origin/<branch>` fails. Fetch the commit and create
  the branch at it explicitly.
- The audit requires the checkout to be **on the PR's `codex/` branch**, not
  detached at its commit. `git branch --show-current` is empty for a detached
  HEAD and the audit reads it as a mismatch.

## Deployment sequence

Each step lists what it must leave updated. A step that changes state without
updating its record is how a deployment drifts.

1. Synchronize local `main`, create one `codex/` branch, commit, push, and open
   or update the matching draft PR. Record the full pushed commit.
2. Inventory the VM. Stop if a package process, update transaction, or shell
   restart is active. Preserve the previous root-owned manifest.
3. If the canonical checkout is dirty or stale, move it intact to the declared
   quarantine root and clone the pushed branch fresh. Do not reset or clean a
   dirty tree destructively.
4. Configure a new commit-named build directory. Build, run the full CTest and
   QML gates, and package from that exact clean source.

   Configure with the project's canonical flags, because that is what the
   Makefile uses and therefore what the project ships:

   ```sh
   cmake -S . -B /home/northstar/builds/pr<PR>-<short> -G Ninja      -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
   ```

   Gates that are not part of `ctest` have to be named explicitly, and one of
   them takes the build directory as an argument:

   ```sh
   sh tests/unit/test-qml-surfaces.sh
   sh tests/unit/test-session-entrypoint.sh /home/northstar/builds/pr<PR>-<short>
   ```

5. Increment the package version and repository revision. Generate a new
   root-owned signing identity and publish to a new output directory. Verify it
   with an isolated `pkg` database/cache/trust store before activation.
6. Back up the active repository policy, metadata, signature, fingerprint, and
   `pkg` configuration under an `r<previous>-backup` name. Activate the new
   repository only after the isolated client accepts it.
7. Install the development build to `~/.local` **directly from the tested
   tree**, never through `make install-user`:

   ```sh
   cmake --install /home/northstar/builds/pr<PR>-<short> --prefix "$HOME/.local"
   ```

   Restart only the supervised shell when a session is active; otherwise leave
   SDDM at the greeter so the next login starts the new binary. A `SIGTERM` to
   the shell is treated as a crash by the supervisor and respawns it, which
   restarts the shell without ending the session. An exit status of 0 ends the
   session instead.

   Then confirm the installed binary is the one just built. Do **not** compare
   hashes: CMake rewrites RPATH on install, so the installed file never matches
   the build tree byte for byte. Compare symbols instead, and remember that Qt
   stores `QStringLiteral` data as UTF-16, so plain `strings` will not find a
   literal that is present:

   ```sh
   nm -C ~/.local/bin/northstar-shell | grep <NewSymbol>
   strings -e l ~/.local/bin/northstar-shell | grep <new-literal>
   ```

8. Atomically install the schema-2 deployment manifest and run:

   ```sh
   make validation-deployment-audit
   ```

   **Write the manifest every handoff, without exception.** It is not a
   snapshot that ages gracefully. Between 2026-08-10 and 2026-08-18 it went
   unwritten across six slices, and the cost was not merely an inaccurate
   record: because it still named repository `r78` and the active `pkg`
   configuration also still named `r78`, the auditor's "active pkg repository
   points at the canonical publication" check **passed by comparing two copies
   of the same wrong value**. The `r78` directory had been removed, so the
   development repository was non-functional the whole time and the audit
   reported it as correct. A stale manifest does not merely fail to describe
   the deployment; it can conceal a broken one.

   Where each value comes from:

   | Key | Source |
   | --- | --- |
   | `source_branch`, `source_revision` | The checkout, which must be on the branch, not detached. `codex/*` during a handoff, or `main` between them — see below |
   | `canonical_build` | The commit-named tree from step 4 |
   | `repository_*`, `package_file` | The active signed publication |
   | `package_sha256`, `catalogue_sha256`, `metadata_sha256` | Computed with `sha256 -q` from the published files |
   | `signature_fingerprint` | The publication record |
   | `quarantine_root` | The dated quarantine for this cycle |
   | `lane` | `ui` or `package`, see below. Absent means `package` |

   **The two resting states.** A deployment is either mid-handoff, on the
   `codex/*` branch under validation, or between handoffs, on `main` at the
   revision that was just merged. Both are legitimate and the audit accepts
   both. `main` carries one extra requirement the in-flight state does not:
   the revision must be reachable from `origin/main`, so resting on `main`
   cannot be used to describe work that was never merged. After a
   squash-merge, update `source_branch` to `main` and `source_revision` to the
   new head; the branch itself no longer exists to name.

   The manifest names the commit under validation, so the later commit that
   records the audit result in the validation document is necessarily not the
   one the manifest names. That is expected and is not drift: the evidence
   cannot be written before the run it reports. Do not rewrite the manifest to
   chase it, and do not re-run the audit after committing the evidence and
   report the mismatch as a failure.

   Compute the digests from the files and confirm they match the publication
   record. Agreement between the two is what proves the repository intact.
   Never copy a digest forward from a previous manifest, and never supply a
   value that cannot be derived from something on disk. A manifest that is
   truthful and fails the audit is worth more than one that passes because it
   was made to.

   The audit ties `source_revision` to the publication's source revision, which
   a handoff that publishes no package cannot satisfy. Declare the lane rather
   than waiving the check; see the next section.

## Declaring the lane

The auditor was written for the package lane, where a handoff publishes a
signed repository revision at step 5 and the checkout, the build, and the
publication all describe one commit. It is also run from the UI lane, where
they legitimately do not: an interface pull request installs under `~/.local`
and publishes no package, so the deployed source is ahead of the last signed
publication. `docs/M4_SIGNED_DEVELOPMENT_CHANNEL.md` is explicit that no
persistent signing key lives in this repository, so a UI handoff *cannot* close
that gap, by design rather than by omission.

**Declare the lane in the manifest.** The expectation is enforced by the
auditor, not by a note in a document:

```ini
lane=ui       # an interface handoff that publishes no package
lane=package  # a handoff that publishes a signed revision at step 5
```

- `lane=package`, which is also the default when the key is absent, requires
  the publication to have been built from the deployed source revision. Omitting
  the key can therefore only make an audit stricter, never weaker.
- `lane=ui` reports that one divergence as
  `NOTE: deployed source is ahead of the signed publication`, and nothing else
  is relaxed. A missing shell, a stale repository, a dirty checkout, or a
  detached HEAD still fail exactly as they do in the package lane.

A UI handoff with a correctly written manifest therefore **passes**. If it does
not, something is genuinely wrong, and the fix is never to edit the manifest so
that it names the publication's older revision: that would make the manifest
describe a deployment which is not installed.

This mechanism exists because the previous arrangement had no way to express
it. `docs/QUALITY_GATES.md` required a passing audit unconditionally while one
check was unsatisfiable in the UI lane, so the only available move was to waive
it in prose, which happened in six consecutive validation documents on the
assumption it was repairable debt. A rule that can only be honoured by writing
an exception is a rule that will keep producing exceptions.

**Warnings in both lanes.** Retained predecessor build trees produce
`historical build remains outside retention boundaries` warnings. The auditor
expects exactly one canonical build, while the retention rule keeps the active
deployment and its immediate predecessor, so one such warning is normal. More
than one means the retention rule has not been applied.

Whatever the lane, state the result honestly in the validation document and
never claim `make validation-deployment-audit` passes when it does not.

9. Hand off the exact noVNC checklist and record manual results in the PR's
   validation document. Never delete quarantine, the previous signed revision,
   snapshots, or the rollback boot environment before acceptance.

## Changes that produce no new binary

A pull request touching only documentation, shell tooling, or shell tests still
runs the shell gates and `git diff --check`, but does not need a new
commit-named build tree, a fresh install, or an interactive checklist: there is
no new binary to accept. Confirm it rather than assert it:

```sh
git diff --name-only origin/main...HEAD -- src apps packaging
```

An empty result means no compiled artifact changed. The manifest for such a
handoff keeps the previous `canonical_build`, because that tree really is the
build the installed prefix came from; note in the validation document that the
build is carried forward and why.

## Proving a gate works

A guard that has never been observed to fail is not evidence. Before recording
any new gate, break the thing it pins, confirm the gate fails, restore it, and
confirm it passes. Record both results.

This is not a formality. Two gates added during M7 were tested this way and the
results differed:

- The QML surface contracts were verified by removing each pinned line. Both
  failed as intended and passed again when restored. They are real.
- A self-test gate that counted QML warnings and failed on any turned out to be
  incapable of catching its own removal. Deleting the teardown it protected
  produced **7 counted warnings and an exit status of 0**, because the warnings
  are emitted while `main()`'s locals are destroyed, after the self-test has
  already returned its status. Untested, it would have been recorded as
  protection that did not exist.

The lesson generalises: a gate can only observe what happens before it runs.
For anything emitted during teardown, or after `main()` returns, prefer a
structural fix -- a scope guard, an ownership change -- over a check, and state
in the code what the check can and cannot see.

## Known traps

Every entry here has actually happened, and each one cost time or produced a
false claim.

### Build and install

- **`make install-user` reconfigures the tree it installs from.** It depends on
  `build`, which depends on `configure`, which re-runs `cmake` with the
  Makefile's own `CMAKE_BUILD_TYPE ?= Debug` and `-DBUILD_TESTING=ON`. A tree
  built with any other flags is silently reconfigured and fully rebuilt
  underneath whatever evidence was just collected from it. Use `cmake --install`
  from the tested tree instead.
- **`BUILD_DIR ?= build` points inside the canonical checkout.** `install-user`
  and `shell-smoke` both default to it and will create a ~270 MB tree in the
  checkout. Pass `BUILD_DIR` explicitly to any Make target.
- **`tests/unit/test-session-entrypoint.sh` takes the build directory as `$1`.**
  Invoked bare it looks for `build/` in the checkout and fails, or worse, tempts
  someone into creating one there.
- **Large shell heredocs can exceed the process argument limit.** Write large
  source files with a file-writing tool rather than a heredoc, and do not insert
  code containing `\n` escapes through another language's string literals.
  Several such substitutions have silently produced literal newlines, and one
  `perl -0p` with `\Q...\E` applied nothing at all while reporting success.
  Always confirm a substitution changed the file before trusting it.

### Verifying a deployment

- **Installed and build-tree binaries never hash-match**, because CMake rewrites
  RPATH on install. Compare symbols with `nm -C` instead.
- **Plain `strings` will not find Qt string literals.** `QStringLiteral` data is
  UTF-16; use `strings -e l`. A bare `strings` returning nothing is not evidence
  of absence.
- **Passing every automated gate proves nothing about deployment.** PR #99
  installed cleanly, passed every gate, and did nothing at all: the command was
  not on the compositor's `PATH`, the client could not derive the socket name,
  and deploying the configuration had replaced `~/.xinitrc`. All three were
  found only by inspecting the running session. Verify the feature against the
  live session, in the environment it will really run in.
- **Verify against the binary the checklist is meant to test.** A checklist
  handed over before the new build was installed validates the old one. When
  that happens, reinstall and re-run it rather than counting the result.

### The session and its entry points

- **SDDM resolves `northstar-session-x11` by name.**
  `/usr/local/share/xsessions/northstar-proxmox.desktop` uses
  `Exec=northstar-session-x11` with no path, resolved through a `PATH` that does
  not include `~/.local/bin`. **Never delete the `/usr/local` copy** -- login
  would break. Refresh it from current source instead, which also removes the
  shadowing hazard by making both copies identical.
- **Refreshing a script can change what it resolves.** The current
  `northstar-session-x11` prefers
  `/usr/local/libexec/northstar-wayfire-nested/bin/wayfire` over the
  home-directory compositor. Check whether such a path exists before accepting
  the refresh, or the next login may quietly start a different compositor.
- **`tools/install-nested-wayfire-session.sh --force` also replaces
  `~/.xinitrc`** with the unsupervised variant. It writes its own backup; use it.
- **The compositor runs key-binding commands without `WAYLAND_DISPLAY`**, so a
  client cannot derive a per-display socket name and has to discover it.
- **A `SIGTERM` to the shell restarts it; a clean exit ends the session.** The
  supervisor treats a non-zero status as a crash and respawns, up to
  `NORTHSTAR_SESSION_MAX_SHELL_RESTARTS`. Compare PIDs before and after to prove
  the restart happened, and confirm the compositor stayed up so it was
  shell-only.

### Tests

- **A controller with default-path persistence will read and write the real
  desktop's state when a test constructs it bare.** Every test must pass a
  `QTemporaryDir` path. This was caught in review while adding notification
  persistence; the existing tests would otherwise have begun mutating the
  account's own history.
- **Single-threaded socket tests deadlock on blocking `waitFor*` calls**,
  because the server never gets a chance to accept. Pump the event loop instead.
- **Leave no stray processes.** Check after any aborted run, and kill only what
  that run started.
- **A test must not read the machine it runs on.** Any lookup that falls back to
  `$HOME`, `PATH`, or a system prefix will make a suite pass or fail according
  to what happens to be installed. Expressing absent by unsetting an override
  is not enough when the override has a filesystem fallback: give the override
  authority to name nothing, and pin it in every case. This has now happened
  twice — first with a controller whose default path would have written the
  real desktop's history, then with a privileged helper looked up in
  `~/.local/bin`. In the second instance the suite passed only because it ran
  before the helper was installed, so run a suite again after installing
  anything it can see.
- **Timeouts are failure deadlines, not stopwatches.** `QTRY_*` returns as soon
  as its condition holds, so a generous value costs a passing run nothing and
  only sets how long a genuine hang takes to report. A budget close to the real
  duration will fail on a loaded machine. Name the constant so the next reader
  knows which it is.

## Acceptance and retirement

After the user confirms the focused checklist, rerun the strict audit and
capture `git`, package, repository, boot-environment, and snapshot identities.
Then squash-merge the PR, delete its local and remote branches, prune stale
refs, and verify local `main == origin/main`.

Retirement is a separate reviewed operation. Keep the newly accepted
deployment and one previous signed repository. Older checkout/build/repository
trees may be moved to quarantine first. Permanently remove only explicitly
listed quarantine paths after confirming their commits and artifacts are
reachable from merged Git history or retained release evidence. Destroy ZFS
snapshots or boot environments only after rollback and home-preservation gates
are closed. The auditor intentionally has no cleanup mode.

## Recovery

If activation or validation fails, stop mutation, preserve logs and the failed
tree, restore the previous repository configuration/fingerprint/metadata
backup, and select the previous development or package-validation session.
For a failed protected update, use the transaction runner's recorded boot
environment; do not improvise package changes outside the broker. Re-run the
auditor after recovery and do not promote the PR while any invariant fails.
