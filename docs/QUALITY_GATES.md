# Quality gates

Quality gates are evidence requirements, not aspirations. The author records the exact command and environment; reviewers check that the evidence matches the change.

The current evidence baseline is the NSTAR-DEV01 FreeBSD 15.1 amd64 VM. Its
nested X11/pixman path is approved for development and interactive product
validation, but it does not close the direct DRM/KMS graphics gate. A gate is
closed only when the required evidence exists on the declared environment;
the fact that a surface started once is not sufficient.

## Pull request gate

Every pull request must satisfy:

- scope matches the issue and milestone;
- documentation and ADRs reflect architectural changes;
- new behavior has tests at the appropriate layer;
- native FreeBSD checks are included when the change touches FreeBSD behavior;
- no unrelated formatting churn is included;
- all build inputs remain pinned;
- new external licences and notices are recorded;
- no secrets, signing keys, images, package repositories, or downloaded source archives are committed;
- rollback or revert behavior is stated.

## Branch, validation, and PR workflow

Every product slice follows the same local-to-cloud path. `main` is kept
clean, and feature work is never developed directly on it.

### 1. Start from synchronized main

```sh
git fetch origin main
git switch main
git merge --ff-only origin/main
git switch -c codex/<milestone>-<slice>
```

Use a focused branch name such as `codex/m4-package-trust`. If a slice is
stacked on another PR, record the dependency in the PR body and rebase the
feature branch onto the latest `origin/main` after the parent is merged. A
force push is permitted only for that feature branch and only with
`--force-with-lease`; never rewrite `main`.

### 2. Validate locally and on the FreeBSD VM

Keep the diff scoped to the slice, add or update tests and documentation, and
run the narrowest checks that prove the behavior. For Northstar shell/session
changes, the normal FreeBSD validation sequence is:

```sh
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
make shell-smoke
make shell-restart-smoke
```

Run the relevant manual acceptance flow in the VM after restarting the
installed shell. Record the FreeBSD release, VM identity, source commit,
commands, test count, and any deliberately deferred gate. Documentation-only
changes still require `git diff --check` and link/heading inspection.

### 3. Publish the cloud branch and draft PR

After validation, commit only the intended files with a terse message and
push the matching branch:

```sh
git status --short --branch
git diff --check
git add <intended-files>
git commit -m "<focused change>"
git push -u origin codex/<milestone>-<slice>
```

Open a draft PR from that cloud branch to `main`. The PR must state:

- what changed and why;
- user-visible impact and architectural boundaries;
- exact local and FreeBSD VM validation;
- known deferred gates or limitations;
- rollback/revert behavior and any stacked-PR dependency.

### 4. Promote, squash-merge, and synchronize

Once the VM/manual checks and review are complete, mark the draft PR ready.
Merge it into `main` with **squash** so the default branch has one coherent
commit per product slice. Then synchronize the local checkout:

```sh
git fetch origin main
git switch main
git merge --ff-only origin/main
git status --short --branch
```

After confirming the PR is merged and `main` contains the intended result,
delete the redundant local feature ref. Because squash merging creates a new
commit, `git branch -d` may refuse an otherwise fully integrated branch; use
`git branch -D codex/<milestone>-<slice>` only after verifying the merged PR
and preserving the cloud PR record. Remote feature-branch deletion is a
separate cleanup action and is not automatic.

The final handoff must identify the merged PR, squash commit, local branch
state, validation result, and any gate that remains open.

## Roadmap transition gates

| Transition | Required evidence before marking complete |
| --- | --- |
| M1 shell acceptance | Single-display smoke and restart evidence, plus multi-display Layer Shell evidence on the declared graphics path |
| M2 session acceptance | Exactly one supervised session, bounded shell recovery, controlled logout/restart/shutdown, display-manager entry-point evidence, and sanitized logs |
| M3 desktop acceptance | Files mutation and Trash recovery, live Desktop Icons create/delete/rename/open/restore, responsive Dock alignment and focus/minimize/restore behavior, associations/Open With, search, sorting and view modes, mounted-volume boundaries, overview, keyboard mappings, Files-to-Apps launch, app-bundle provenance, and settings persistence |
| M4 package trust | Pinned Ports/Poudriere inputs, an actual signed repository publication, package provenance, read-only/update-preview behavior, and no unauthorized mutation; policy, fingerprint-store, catalogue-integrity, publication-signature verification, and native `pkg` publication/client smoke foundations are implemented |
| M4 update/rollback | Read-only authorization preflight, independent broker verification, bounded root-owned update-helper request contract, N-1 to N upgrade, pre-upgrade `bectl` environment, rollback to the prior environment, package/shell recovery, and home-data preservation |
| M5 image | Reproducible clean builder, checksums, UEFI GPT/root-on-ZFS installation, first boot, and update/rollback from the produced image |
| M6 alpha | VM plus declared Intel/AMD hardware matrix, graphics/login, applications, diagnostics, crash recovery, update, rollback, and clean shutdown |

Hold a change when it depends on an unpinned dependency, requires broad root access, changes the FreeBSD base without a compelling reason, couples the shell to undocumented Wayfire internals, adds Apple-owned assets, or breaks the clean-build/clean-install lane.

## Milestone gates

| Gate | Required evidence |
| --- | --- |
| M0 host | Exact FreeBSD 15.1 amd64, clean VM bootstrap, idempotent second run, package versions, Wayland and Xwayland smoke tests |
| M1 shell | Multi-monitor Layer Shell behavior, unprivileged launch, app launch, shell-only restart, Qt unit tests |
| M2 session | Exactly one session, controlled lifecycle, service restart, launch identity, bounded notification feedback, sanitized logs |
| M3 desktop | File operations, live branded Desktop Icons, responsive Dock and window controls, settings persistence, user-scoped file associations, home search, sorting and view modes, mounted-volume locations, overview, Files-to-Apps drag-and-drop, project-owned menu, validated app-bundle provenance |
| M4 update | Signed repository, N-1 to N upgrade, pre-upgrade `bectl` environment, rollback, home-data preservation |
| M5 image | Clean builder, pinned inputs, checksums, UEFI GPT/root-on-ZFS install, first boot, update/rollback |
| M6 alpha | Supported VM and physical hardware matrix, diagnostics, crash recovery, shutdown, application coverage |

## Reproducibility gate

Release artifacts are blocked unless:

- `packaging/manifests/upstream.lock` has no unresolved values;
- each external source has a version or commit and checksum;
- package versions are captured from the selected repository;
- the project commit and build toolchain are recorded;
- artifact SHA-256 is generated after assembly;
- a clean builder can reproduce equivalent package contents.

## Security gate

Reviewers must verify privilege boundaries, authorization, secret handling, log redaction, package/repository signing, and CI runner isolation. Public pull requests must not execute on a persistent privileged release builder. Release builds run only from protected branches or manually approved workflows in disposable environments.

The native publication smoke command is:

```sh
make pkg-repository-smoke
sudo -n make pkg-repository-smoke NORTHSTAR_PKG_CLIENT=1
```

The first invocation proves disposable FreeBSD v2 catalogue creation and the
external RSA signing contract as the development user. The explicit root
invocation runs only an isolated client `pkg update -f` against a temporary
root-owned `file://` repository and temporary package DB/cache paths. Both
paths clean up their package files, repository metadata, keys, and trust store;
neither path performs package mutation or installs repository configuration.
This gate does not substitute for protected Poudriere publication, key
custody, or the M4 update/rollback gate.

## Release gate

No alpha image is published until install, login, Qt/Wayland launch, Xwayland launch, browser launch, file management, settings, update, rollback, diagnostics, shell crash recovery, and clean shutdown have current evidence on the declared hardware matrix.

The ordered roadmap and the distinction between the supplemental VM lane and
the native release gates are maintained in [`docs/ROADMAP.md`](ROADMAP.md).
