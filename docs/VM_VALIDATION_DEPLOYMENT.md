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

## Deployment sequence

1. Synchronize local `main`, create one `codex/` branch, commit, push, and open
   or update the matching draft PR. Record the full pushed commit.
2. Inventory the VM. Stop if a package process, update transaction, or shell
   restart is active. Preserve the previous root-owned manifest.
3. If the canonical checkout is dirty or stale, move it intact to the declared
   quarantine root and clone the pushed branch fresh. Do not reset or clean a
   dirty tree destructively.
4. Configure a new commit-named build directory. Build, run the full CTest and
   QML gates, and package from that exact clean source.
5. Increment the package version and repository revision. Generate a new
   root-owned signing identity and publish to a new output directory. Verify it
   with an isolated `pkg` database/cache/trust store before activation.
6. Back up the active repository policy, metadata, signature, fingerprint, and
   `pkg` configuration under an `r<previous>-backup` name. Activate the new
   repository only after the isolated client accepts it.
7. Install the development build to `~/.local`. Restart only the supervised
   shell when a session is active; otherwise leave SDDM at the greeter so the
   next login starts the new binary.
8. Atomically install the schema-2 deployment manifest and run:

   ```sh
   make validation-deployment-audit
   ```

9. Hand off the exact noVNC checklist and record manual results in the PR's
   validation document. Never delete quarantine, the previous signed revision,
   snapshots, or the rollback boot environment before acceptance.

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
