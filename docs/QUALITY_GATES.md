# Quality gates

Quality gates are evidence requirements, not aspirations. The author records the exact command and environment; reviewers check that the evidence matches the change.

The current evidence baseline is the NSTAR-DEV01 FreeBSD 15.1 amd64 VM. Its
nested X11/pixman path is approved for development and interactive product
validation, but it does not close the direct DRM/KMS graphics gate. A gate is
closed only when the required evidence exists on the declared environment;
the fact that a surface started once is not sufficient.

Full disk-image assembly and manual Proxmox import are not per-PR requirements.
Routine PRs use the development lane and may defer image-level claims to a
named release-candidate checkpoint. The authoritative batching, exception,
and evidence policy is
[`docs/MILESTONE_IMAGE_VALIDATION.md`](MILESTONE_IMAGE_VALIDATION.md).

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

`make qml-surface-test` is a fast supplemental check for the product-critical
Desktop, Dock, Files, Settings, Software Center, and system-menu wiring. It
does not replace interactive noVNC acceptance or graphics-path evidence.

Run the relevant manual acceptance flow in the VM after restarting the
installed shell. Record the FreeBSD release, VM identity, source commit,
commands, test count, and any deliberately deferred gate. Documentation-only
changes still require `git diff --check` and link/heading inspection.

For image and installer work, this routine PR step means DEV01, native
FreeBSD, and non-destructive file-backed/model validation. It does not mean
building, transferring, importing, or reprovisioning a QCOW2. Record the named
future checkpoint and the exact deferred image behaviors in the PR. The
accepted M5 Installer Release Candidate and its installed-image update/rollback
evidence are recorded under `docs/validation/`. The next image checkpoint is
the **M6 Alpha Hardware Release**; routine PRs continue using the development
lane unless the milestone policy requires earlier image evidence.

## Consolidated sprint acceptance checklist

The current desktop integration is a broad M3 acceptance candidate, so the
checks are deliberately split into repeatable headless evidence and a short
manual noVNC gate. The headless lane is already green on the isolated
NSTAR-DEV01 validation checkout: 17/17 Qt tests passed, the QML surface
contract passed, compiled QML startup produced no reference or syntax errors,
and the Welcome/Text Editor self-tests plus session supervisor/entry-point
checks passed. The offscreen Layer Shell notices are expected because that
lane has no real Wayland compositor; they are not direct DRM/KMS evidence.

### Automated gate

Run the following after the final source is installed in the VM. If a shell or
QML file changed, rebuild the target and install it again; do not rely on an
earlier installed binary or build-tree smoke.

```sh
env QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
make qml-surface-test
make shell-smoke
make shell-restart-smoke
sh tests/unit/test-session-script.sh
sh tests/unit/test-session-entrypoint.sh build
env QT_QPA_PLATFORM=offscreen "$HOME/.local/share/northstar/apps/NorthstarWelcome.app/Contents/Executable/northstar-welcome-gui" --self-test
env QT_QPA_PLATFORM=offscreen "$HOME/.local/share/northstar/apps/NorthstarTextEditor.app/Contents/Executable/northstar-text-editor" --self-test
```

The exact target names may be inspected with `make help` on the declared VM;
the evidence record must include the commands actually run and their exit
status. A plain GUI test over SSH is not valid if Qt selected XCB without
`DISPLAY`; use the offscreen environment for headless checks and noVNC for
interactive checks.

### Extended-sprint checkpoint gate

When a long-running goal is used, the broad checklist is executed as cohesive
checkpoints rather than as one unbounded change set. The detailed sequence is
in [`docs/OVERNIGHT_SPRINT.md`](OVERNIGHT_SPRINT.md). At each checkpoint the
agent must have a focused diff, a targeted test result, a VM install result,
and a written pass/open/fail observation before starting the next dependent
slice. A micro-step may be grouped with its neighboring steps in one branch
and PR; only cohesive user-visible slices receive cloud PRs.

The checkpoint states have strict meanings:

- `PASS`: the stated behavior was observed on the declared environment and
  the supporting command completed successfully.
- `OPEN`: the behavior still needs an interactive or hardware gate; no claim
  is made from an unrelated headless or SSH result.
- `FAIL`: the behavior regressed or the command failed; promotion is blocked
  for that slice until it is repaired or explicitly parked with a follow-up.
- `N/A`: the check is outside the current environment, with the reason and
  future gate recorded.

Before moving between slices, record the source commit, VM checkout, install
prefix, exact command, result, and next action. Before cloud promotion, run
the full automated gate again, review the diff for generated or secret files,
and confirm that every required manual item is `PASS` or explicitly deferred
by a documented environment limitation. The complete unattended-sprint
sequence and fast-failure policy are maintained in
[`docs/OVERNIGHT_SPRINT.md`](OVERNIGHT_SPRINT.md).

### Manual noVNC gate

Record each item as pass, fail, or not-applicable with a short observation:

- The installed shell starts from the supported console/session path, displays
  the Northstar logo/background, and does not auto-launch Terminal.
- Menu/logo button, top bar, Dock, Files, Apps, Settings, Welcome, Software
  Center, and Text Editor open and close without a dead click target.
- Desktop-folder creation and external changes appear in the desktop projection;
  folder icons open Files and file icons use the association chooser.
- Desktop icon selection, double-click, rename, Delete, Properties, drag,
  snap-to-grid, restart persistence, and Appearance reset behave correctly.
- Files creates folders and text files, renames them, searches them, switches
  list/grid views, sorts in both directions, navigates Home/Desktop/Documents,
  and rejects paths outside the allowed home boundary.
- Delete sends a disposable item to Trash and Restore returns it to its
  original path with contents intact. There must not be two competing labels
  for the same destructive action in one surface.
- Open With filters by MIME/extension, allows Northstar Text Editor to edit and
  save a text file, preserves the user-scoped default, and permits changing or
  forgetting that default. Firefox must not open every file automatically.
- Application discovery refreshes, known `.desktop`/`.app` entries launch, and
  Software Center search/details/unsupported mutation messaging are usable.
- Dock shortcuts align at the VM resolution, running indicators are accurate,
  focus/minimize/restore/close work, and the desktop uses the available area.
- Lunar top-bar navigation, routed global search, left-side system menu,
  centered launcher, quick settings, notifications, and icon-first dock fit at
  1280x800 without clipping or overlap. Files, Settings, Software Center,
  Welcome, and Text Editor use the shared visual hierarchy in both themes.
- Settings preference changes persist, diagnostics show session state, and
  light/dark appearance plus keyboard mappings do not break the shell.
- Logout, graceful restart, and shutdown follow confirmation and ownership
  rules; after reboot the shell starts once and the core file path still works.

Interactive noVNC success closes the current M3 manual gate only. It does not
close direct DRM/KMS, native multi-display, display-manager, or Intel/AMD
hardware gates, which require the declared graphics hardware later.

### Evidence record and promotion gate

The validation note should contain the VM identity and FreeBSD release, source
commit, VM checkout path, install prefix, exact commands, automated counts,
manual results, screenshots or sanitized logs for failures, and the explicit
graphics limitation. Only after that note is complete may the author mark the
integration PR ready, squash-merge it, synchronize `main`, and remove merged
feature refs. If any manual item fails, keep the PR draft, make a focused fix,
repeat the relevant test and full automated gate, and update the note before
promotion.

The latest sprint evidence is recorded in
[`docs/validation/M4_SPRINT_2026-08-09.md`](validation/M4_SPRINT_2026-08-09.md).
It records PRs #56-#59 as automated-green integration slices while keeping
the noVNC/manual session observations explicitly open.

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

An intermediate image/installer PR also states `Image checkpoint`, `Image
status`, `Routine evidence`, and `Deferred evidence` using the template in
[`docs/MILESTONE_IMAGE_VALIDATION.md`](MILESTONE_IMAGE_VALIDATION.md). A
documented `DEFERRED` image gate is compatible with merge; it is not image
acceptance.

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
| M2 session acceptance | Exactly one supervised session, bounded shell recovery with visible terminal-failure diagnostics, controlled logout/restart/shutdown, display-manager entry-point evidence, and sanitized logs |
| M3 desktop acceptance | Files mutation and Trash recovery, live Desktop Icons create/delete/rename/open/restore, Desktop-folder icon projection and position persistence, full-output wallpaper behind the floating Dock, persistent grouped Dock pins, responsive Dock alignment and focus/minimize/restore behavior, bounded draggable shell panels, associations/Open With, asynchronous categorized unified search, sorting and view modes, mounted-volume boundaries, overview, live desktop/bundle application discovery, keyboard mappings, Files-to-Apps launch, app-bundle provenance, and settings persistence |
| M3 unified search acceptance | At 1280x800, click and Ctrl+K open a focused overlay; action/application/file categories render without clipping; typing remains responsive during bounded Home search; Up/Down, Enter, Escape, and mouse activation work; every activation is revalidated and no command or web execution is exposed |
| M3 Quick Look acceptance | At 1280x800, Space and mouse actions preview selected Files/Desktop items; bounded UTF-8 text, scaled raster images, folders, metadata, unavailable, and error states render unclipped; the panel moves/resizes; Home and explicit mounted-volume boundaries are revalidated; associations and contents remain unchanged |
| M3 Quick Settings acceptance | At 1280x800, every tile and slider reflects confirmed FreeBSD capability state; unsupported controls are disabled and route to Settings; DND persists across shell restart and suppresses unread badges while retaining history; mixer mutation is enabled only with a readable device and reports success only after confirmation |
| M3 shared chrome acceptance | At 1280x800, Files, Settings, Software Center, Welcome, and Text Editor share Lunar framing; each moves and resizes reliably; minimize, maximize/restore, and close work; content is unclipped; standalone apps honor the selected theme after relaunch |
| M4 package trust | Pinned Ports/Poudriere inputs, an actual signed repository publication, package provenance, read-only/update-preview behavior, and no unauthorized mutation; policy, fingerprint-store, catalogue-integrity, publication-signature verification, and native `pkg` publication/client smoke foundations are implemented |
| M4 signed development channel | A native Northstar `.pkg` is published from resolved FreeBSD, Ports, dependency, and project inputs; signing keys remain external; the output records package versions, source-lock/manifest/catalogue digests, and fingerprint provenance; an isolated client accepts the authentic repository and rejects altered signed catalogues; Software Center exposes verified provenance without enabling mutation |
| M4 update/rollback | Read-only authorization preflight, independent broker verification, bounded root-owned update-helper request contract, N-1 to N upgrade, pre-upgrade `bectl` environment, rollback to the prior environment, package/shell recovery, and home-data preservation |
| M4 transactional runner | Fixed PolicyKit executable; broker-derived package targets only; boot environment created before `pkg`; target versions verified; failures schedule rollback; explicit rollback uses root-owned state; reboot requirement is visible |
| VM deployment hygiene | Root-owned schema-2 deployment manifest; clean canonical checkout at the pushed commit; one canonical build; immutable current and previous signed revisions; older state quarantined; strict read-only deployment audit passes before and after handoff |
| M5 image | Reproducible clean builder, checksums, UEFI GPT/root-on-ZFS installation, first boot, and update/rollback from the produced image |
| M5 image inputs | Exact FreeBSD release-set names/sizes/hashes, accepted Northstar package/repository provenance, explicit project commit, immutable resolved-input output, and rejection of unresolved or tampered inputs without root or disk mutation |
| M5 QCOW2 boot smoke | `qemu-img check`, UEFI firmware, virtio disk, snapshot-only source protection, ZFS root-mount evidence, Northstar host identity, bounded multi-user login detection, serial-log digest, and no host port forwarding; graphical login remains a separate Proxmox/noVNC gate |
| M5 QCOW2 Proxmox acceptance | The exact checksummed QCOW2 reaches the branded desktop at 1280x800; compositor and shell remain supervised; keyboard, pointer, clicking, ordinary text entry, terminal and Files launch, and clean shutdown work through noVNC |
| M5 installed-image update/rollback | On the protected accepted r85 installation: non-mutating candidate staging; schema-2 state bound to the exact image commit, repository revision, candidate source, catalogue digest, and signing fingerprint; signed N-1 to N package update; actual ZFS boot-environment creation and activation; injected package-failure recovery after reboot; explicit rollback after reboot; unchanged `/home` sentinel; and retained command/package/repository evidence; never run the destructive gate on the persistent development VM |
| M5 first-boot setup | Branded bounded wizard; complete installed FreeBSD timezone inventory with safe system default; dedicated non-wheel setup identity; root-owned pending/completion states; fixed active-console PolicyKit action; caller/request/field revalidation including independent zoneinfo containment and existence; password only over stdin and cleared after delivery; exactly one first administrator with package-owned session configuration and a mode-0440 account-specific sudo policy; setup identity, autologin, and one-time session entry sealed on success; exactly one image-managed Proxmox fallback remains and resolves packaged runtime paths; retry removes both partial account and authorization; integrated SDDM/login evidence repeated on the immutable M5 Installer Release Candidate |
| M5 installer target selection | Read-only unprivileged FreeBSD discovery; bounded versioned records; active mounted/ZFS/swap disks and undersized targets visibly ineligible; malformed or contradictory records rejected; exact device-name and permanent-erasure acknowledgement required; review plan only; no storage mutation until an independent privileged revalidation slice |
| M5 installer engine preflight | Fixed PolicyKit executable; caller-owned mode-0600 bounded request; whole-disk GEOM identity independently revalidated; changed, partition, undersized, mounted, ZFS, and swap targets rejected; one root-owned mode-0600 transaction staged without overwrite; execution restricted to the separate guarded executor and no disk mutation commands present in the engine |
| M5 installer source and journal | Fixed root-controlled source/key paths; bounded signed manifest; detached RSA/SHA-256, reviewed-manifest, payload-size, and payload-digest verification; private key external; source passes before target staging; root-owned sequenced journal; staged/interrupted status; explicit authenticated recovery and archival abandonment; execution restricted to the guarded executor |
| M5 guarded installer execution | Separate fixed PolicyKit executor; root-owned installer-media marker bound to the source manifest; exact active transaction and typed whole-disk confirmation; production overrides disabled; source, target, and archive revalidated immediately before mutation; active GEOM label aliases resolved to their backing target providers; ZFS label clearing verified when present and idempotently skipped when already absent; ordered GPT/EFI/ZFS/rootfs/bootloader journal; installed runtime verification; completion archive; cleanup and explicit interrupted state after injected failure; file-backed and real disposable VirtIO retry gates required before another M5 Installer Release Candidate |
| M5 installer recovery and diagnostics | Separate fixed non-mutating PolicyKit recovery path; strict interrupted-journal phase validation; bounded allowlisted diagnostic record with private data excluded; user-owned atomic export; no in-place destructive resume; marker, manifest, active transaction, target identity, quiescence, and exact-device confirmation before failed-attempt archival; retry begins from a new reviewed transaction; actual interrupted-disk/reboot evidence deferred to the M5 Installer Release Candidate |
| M5 boot-environment recovery | Bounded read-only `bectl list -H` inventory; strict UI record parsing; only an existing `northstar-before-...` target may be selected; exact-name confirmation and administrator authentication; activation verified by rereading next-boot state; no create/delete/rename/mount/package/reboot capability; sanitized user-owned diagnostics; actual activation/reboot evidence deferred to the M5 Installer Release Candidate |
| M5 installer media | Production QCOW2 emits its matching runtime-bound rootfs payload before live-media state; external private key and detached RSA/SHA-256 source signature; exact image/source/runtime/commit provenance binding; read-only labeled source partition; inherited installed-system EFI automount removed before live-media boot; dedicated passwordless local-active media identity limited to fixed PolicyKit actions and absent from installed targets; installed payload excludes media privilege; raw assembler accepts no host disk and requires a marked disposable FreeBSD builder; boot/install evidence deferred to the M5 Installer Release Candidate |
| M5 Installer RC orchestration | One clean exact checkout and immutable input set; ordered QCOW2, signed-source, and raw-media stages; distinct protected builder markers; external root-owned signing key; top-level commit/digest/size provenance; no host-disk destination; failed staging removed or quarantined; manual installation evidence required before promotion |
| M6 alpha | VM plus declared Intel/AMD hardware matrix, graphics/login, applications, diagnostics, crash recovery, update, rollback, and clean shutdown |
| M6 readiness inventory | Schema-1 privacy-bounded capability record; exact FreeBSD 15.1 amd64 UEFI/ZFS base; virtual, Intel DRM, AMD DRM, unsupported DRM, and incomplete lanes classified deterministically; VM output remains supplemental; no identifiers or raw hardware listings |
| M6 matrix runner | Exact expected lane; readiness claim match; installed shell/session/desktop-entry/browser/terminal/diagnostics/display-manager preflight; strict 14-field operator observations; no unknown or duplicate records; physical pass rejects fail/pending/deferred states; VM can never become a physical pass; mode-0600 privacy-bounded output; no lifecycle or package mutation authority |
| M6 platform evidence | Passive normalized network/audio/input/ACPI inventory; no traffic, playback, input injection, suspend, or mutation; strict six-field operator record; VM always supplemental; physical pass requires complete capabilities and observations; matrix physical pass requires the validated platform record; mode-0600 privacy-bounded output |

Hold a change when it depends on an unpinned dependency, requires broad root access, changes the FreeBSD base without a compelling reason, couples the shell to undocumented Wayfire internals, adds Apple-owned assets, or breaks the clean-build/clean-install lane.

## Milestone gates

| Gate | Required evidence |
| --- | --- |
| M0 host | Exact FreeBSD 15.1 amd64, clean VM bootstrap, idempotent second run, package versions, Wayland and Xwayland smoke tests |
| M1 shell | Multi-monitor Layer Shell behavior, unprivileged launch, app launch, shell-only restart, Qt unit tests |
| M2 session | Exactly one session, controlled lifecycle, service restart, launch identity, bounded notification feedback, sanitized logs |
| M3 desktop | File operations, live branded Desktop Icons, responsive Dock and window controls, settings persistence, user-scoped file associations, home search, sorting and view modes, mounted-volume locations, overview, live desktop/bundle application discovery, Files-to-Apps drag-and-drop, project-owned menu, validated app-bundle provenance |
| M4 update | Signed repository, N-1 to N upgrade, pre-upgrade `bectl` environment, rollback, home-data preservation |
| M5 image | Clean builder, pinned inputs, checksums, UEFI GPT/root-on-ZFS install, first boot, update/rollback |
| M6 alpha | Supported VM and physical hardware matrix, diagnostics, crash recovery, shutdown, application coverage |

The readiness inventory is a pre-gate, not release acceptance. A `ready`
physical record allows a machine to enter focused matrix validation; it does
not prove graphical login, compositor stability, applications, suspend/resume,
update/rollback, or shutdown. A `supplemental` VM record can support routine
development evidence but cannot satisfy either physical graphics lane.

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

Every persistent Proxmox handoff must also follow
[`docs/VM_VALIDATION_DEPLOYMENT.md`](VM_VALIDATION_DEPLOYMENT.md) and pass:

```sh
make validation-deployment-audit
```

The auditor is deliberately read-only. A failed audit blocks handoff or merge;
it never authorizes deletion of historical checkouts, builds, repositories,
signing identities, snapshots, or boot environments.

## Release gate

No alpha image is published until install, login, Qt/Wayland launch, Xwayland launch, browser launch, file management, settings, update, rollback, diagnostics, shell crash recovery, and clean shutdown have current evidence on the declared hardware matrix.

The ordered roadmap and the distinction between the supplemental VM lane and
the native release gates are maintained in [`docs/ROADMAP.md`](ROADMAP.md).
