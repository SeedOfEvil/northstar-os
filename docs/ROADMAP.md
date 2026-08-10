# Northstar roadmap

The project advances through user-visible milestones with explicit pass gates. A milestone is not complete because a component starts once on one machine; it is complete when its clean, repeatable acceptance evidence exists.

| Milestone | Name | Status | Primary outcome |
| --- | --- | --- | --- |
| M0 | Reproducible development desktop | Supplemental VM lane validated; direct DRM/KMS gate pending | A clean FreeBSD 15.1 amd64 VM becomes the approved development environment |
| M1 | Shell seed | Single-display product slice validated; multi-display acceptance pending | Top bar and dock render correctly on every connected display |
| M2 | Desktop session | Development session lane validated; production display-manager and service integration pending | Session, services, supervision, notifications, and lifecycle work coherently |
| M3 | Core desktop | Core workflows, Lunar redesign, shared chrome, and Dock v2 accepted; unified search is in progress | Filer, branded desktop surface, responsive dock, settings, search, overview, associations, and project apps form a usable desktop |
| M4 | Packages, updates, rollback | Package policy, fingerprint stores, provenance preview, publication signature verification, native `pkg` publication smoke, bounded helper contract, and broker staging merged; protected release publication and update/rollback remain | Signed packages and ZFS boot-environment rollback work end to end |
| M5 | Reproducible image and installer | Not started | Pinned inputs produce a bootable UEFI root-on-ZFS image |
| M6 | Alpha hardware release | Not started | The supported VM and narrow Intel/AMD hardware matrix meets the alpha definition |

## Current baseline and clear path forward

The current `main` line contains the merged Northstar shell, session, Files,
application-bundle, notification, keyboard, read-only Software Center, text
editor, desktop-icon, search, association, and M4 package-trust foundations.
The core M3 desktop workflows and initial Lunar redesign have passed iterative
noVNC validation in the NSTAR-DEV01 FreeBSD 15.1 development VM. The wallpaper,
dock transparency, shell-panel movement, and persistent desktop icon placement
slice is accepted. The reusable
`Northstar.Ui 1.0` module and gives Files, Settings, Software Center, Welcome,
and Text Editor one shared frame, title-bar, control, move, resize, palette, and
icon contract. That shared-chrome slice passed the 1280x800 noVNC acceptance
gate on NSTAR-DEV01. Its design and acceptance contract are in
[`docs/M3_LUNAR.md`](M3_LUNAR.md), with evidence in
[`docs/validation/M3_SHARED_UI_2026-08-09.md`](validation/M3_SHARED_UI_2026-08-09.md).
Dock v2 now provides persistent ordered pins, Pin/Unpin recovery from the
application overview, drag-to-reorder, canonical application grouping, and
bounded upward menus. PR #69 passed the complete 19-test FreeBSD gate and
iterative 1280x800 noVNC acceptance before its squash merge.

The VM is intentionally not being treated as final graphics evidence. Its
Proxmox basic-VGA/scfb path does not provide a guest DRM render device, so
direct DRM/KMS, multi-display, and native display-manager claims remain open
until the appropriate hardware or graphics path is available. This keeps the
product work moving while preserving honest release gates.

Since the earlier baseline, the sprint integration head has promoted PRs #56
through #59: the shared Software Center application catalog, read-only update
plan review, Files view-mode persistence, and an identity-checked supervised
shell restart action. The current automated evidence and the remaining manual
noVNC items are recorded in
[`docs/validation/M4_SPRINT_2026-08-09.md`](validation/M4_SPRINT_2026-08-09.md).

The next work is ordered as follows:

1. **Build keyboard-first unified search.** Aggregate bounded system actions,
   validated apps, and asynchronous home-scoped files with cancellation.
2. **Add Quick Look.** Preview bounded text, raster images, folders, and
   metadata from Files and Desktop without changing associations or content.
3. **Back Quick Settings with capabilities.** Report confirmed FreeBSD state,
   persist shell-local DND, and disable unsupported controls honestly.
4. **Close the remaining M3 hardware-sensitive evidence.** Repeat placement,
   animation, multi-display, and direct compositor checks on the future
   Intel/AMD DRM lane; keep the scfb/pixman observations supplemental.
5. **Close the remaining M1/M2 development evidence.** Capture a multi-display
   Layer Shell run when hardware permits, and separately validate the branded
   display-manager session, controlled lifecycle actions, and the production
   privilege boundary. The current Proxmox fallback remains supplemental.
6. **Close M4 publication evidence.** The policy, fingerprint-store,
   publication-provenance, catalogue-integrity, read-only signature
   verification, and native `pkg` publication/client smoke foundations are
   merged. The remaining trust evidence is an actual signed development/stable
   `pkg` repository publication built from pinned Ports/Poudriere inputs with
   protected key custody and recorded provenance.
7. **Build safe update and rollback.** The bounded root-owned helper request
   contract and independent verified-plan broker are defined and tested. Next
   establish the reviewed privileged deployment, preflight a named `bectl` boot
   environment before upgrades, validate N-1 to N upgrades and rollback, and
   prove that home data survives both paths. No package mutation is exposed
   before these gates pass.
9. **Produce the reproducible image and installer.** Only after M4 has current
   evidence, pin the image inputs, build QCOW2 first, validate UEFI GPT/root-on-
   ZFS installation and first boot, then add raw/USB/ISO outputs.
10. **Run the alpha hardware matrix.** Validate the supported VM plus the
   declared Intel and AMD graphics lanes, networking, applications,
   diagnostics, crash recovery, updates, rollback, and clean shutdown.

Each step becomes a small, reviewable branch and PR. A milestone moves to
complete only when its quality-gate evidence is current, reproducible, and
documented; interactive success on one session is useful evidence but is not
itself a release claim. The branch, validation, squash-merge, and cleanup
workflow is defined in [`docs/QUALITY_GATES.md`](QUALITY_GATES.md).

## M0: Reproducible development desktop

The PR 2 tooling now includes `check-host.sh`, an idempotent `bootstrap-dev.sh`, a package manifest, sanitized diagnostics, Wayland/seatd/Wayfire/Xwayland package setup, the Qt 6 development environment, QTerminal, Firefox, `xterm`, and a manual recovery path. The basic-VGA Proxmox lane also has `make nested-wayfire-session`, which builds a user-local Wayfire v0.10.1 X11/pixman compatibility binary and installs its session files without replacing the stock package. The host, bootstrap, nested client, and visible console evidence are recorded in [`docs/validation/M0_NSTAR-DEV01_2026-08-06.md`](validation/M0_NSTAR-DEV01_2026-08-06.md). See [`docs/M0_BOOTSTRAP.md`](M0_BOOTSTRAP.md) and [`docs/M0_PROXMOX.md`](M0_PROXMOX.md).

Pass only when the host is exactly FreeBSD 15.1 amd64, bootstrap succeeds from a clean VM and is harmless on a second run, Wayfire starts unprivileged through a supported graphics path, a native Wayland Qt application and an X11 application through Xwayland launch, installed versions are captured, and no project file is copied into `/usr/bin` or `/usr/lib`. The nested lane is supplemental evidence and does not close the direct DRM/KMS gate.

## M1: Shell seed

Deliver one `northstar-shell` process with a top bar, dock, clock, active-window title, application catalog/menu, searchable application overview, pinned application list, launcher buttons, light/dark design tokens, and one surface per connected display. Use Qt 6, QML, and Layer Shell behind a platform interface.

The first implementation slice is documented in [`docs/M1_SHELL.md`](M1_SHELL.md). It provides a native Qt/QML process, a C++ LayerShellQt adapter, and one reserved panel surface per display. By decision on 2026-08-06, multi-display testing is deferred for the current development lane; it remains an M1 release gate.

Pass only when surfaces reserve or overlay space correctly, appear on every display, restart without terminating applications, launch terminal and Firefox, run unprivileged, and pass native Qt unit tests.

## M2: Desktop session

Deliver `northstar-session`, environment setup, D-Bus startup, logout/reboot/shutdown, launcher, notifications, settings, file associations, removable-volume events, crash restart policy, and diagnostic logging.

The first scoped foundation is documented in [`docs/M2_SESSION.md`](M2_SESSION.md). It prepares the user environment, starts D-Bus and the configured compositor, discovers the compositor's actual Wayland socket, supervises the shell, restarts shell crashes within a bounded limit, stops only the child processes it owns, installs a standard Wayland session descriptor, and provides an opt-in supervised nested `startx` wrapper that is now live-validated. The first branded SDDM greeter, official Northstar logo, and explicit Proxmox X11 fallback session are now prepared for non-destructive preview. Native display-manager login and persistent service integration remain future work.

The follow-on session slices add a user-private status contract, a confirmed
unprivileged end-session request, tracked application launches, controlled
restart/shutdown actions, and an opt-in console-login autostart path for the
current development VM. The Session settings page shows supervisor state,
Wayland display, owned process IDs, and restart count; the launcher records
desktop identity and PID and gives the shell success/failure feedback without
widening supervisor process ownership. The system menu now exposes confirmed
logout, restart, and shutdown actions, with an explicit unmanaged-shell
fallback for logout. The in-shell Notification Center retains bounded launch
events, shows an unread badge, and supports mark-read, dismiss, and clear
actions without claiming a desktop-wide notification protocol.

The supervisor now preserves a terminal `failed` state and its failure event
when startup or the shell restart limit fails, and duplicate-session attempts
leave the active session's status/control contract untouched. This makes the
recovery state visible in Settings diagnostics instead of appearing as a clean
stop; the warning directs the user to restart from the console or login session
after saving work.

Pass only when login starts exactly one session, shell crashes are detected and restarted, logout terminates only the user session, privileged lifecycle actions are controlled, launches record PID and identity, and logs contain no secrets.

## M3: Core desktop experience

Deliver the filer, settings, search, application overview, keyboard mapping, drag-and-drop launching, desktop icons or volumes, trash integration, the first `.app` implementation, and a project-owned Qt global menu.

The first filer slice is documented in [`docs/M3_FILES.md`](M3_FILES.md). It
adds a home-folder-scoped Files window with folder navigation, default file
opening, path-boundary protection, search, explicit Open With selection, and
top-menu/dock entry points. The first project-defined `.app` slice now
discovers a validated development bundle, surfaces its owned icon, passes its
owned executable directly to the launcher, and watches project bundles for
catalog changes. The XDG `.desktop` catalog also refreshes live and exposes
source and launchability metadata without evaluating Exec through a shell.
The slice supports dragging regular Files
entries onto Apps tiles for explicit file launching, provides a Locations bar
for mounted non-pseudo volumes with read-only navigation, and persists the
Appearance preference across shell restarts. The first project-owned menu
accelerators and application-level keyboard mappings now share a tested
command catalog. Project `.app` bundles now require and expose a tested
source/package/revision provenance record. Cryptographic signing, package
repository verification, and compositor-wide/global third-party menu
integration remain follow-on slices. File opening now supports reversible,
user-scoped extension defaults with an explicit Open With escape hatch.

The Desktop Icons v1 surface is documented in [`docs/M3_DESKTOP.md`](M3_DESKTOP.md).
It adds a live, home-bound icon grid over the branded wallpaper with safe
selection, opening, rename, Trash, and Properties actions. Verified execution
of application-like `.desktop` and `.app` entries remains part of the
application-discovery follow-on slice.
The Desktop surface now projects entries from the home `Desktop` folder into
the primary desktop as directly activated folder/file icons. Folder activation
opens the corresponding Files location; file activation enters the same
association chooser used by Files. It remains home-scoped and does not claim
to replace a full desktop-file or icon-position service. The live icon grid
also exposes safe selection, opening, rename, Trash, and Properties actions;
verified execution of application-like `.desktop` and `.app` entries remains
part of the application-discovery follow-on slice.

The first editable first-party app, `NorthstarTextEditor.app`, now accepts a
file argument from the Open With flow and provides bounded UTF-8 editing with
atomic user-owned saves. This closes the first practical file-association loop
without claiming broad macOS application compatibility. The Desktop surface
uses the same association chooser as Files. User-scoped icon positions can be
dragged and are persisted with bounded coordinates; release positions snap to
the nearest free cell, while reset falls back to the default column layout
through Settings > Appearance. The projection also watches the home directory
so a Desktop folder created after shell startup appears without a manual
refresh. This remains home-scoped and primary-display-only; it does not claim
to replace a full desktop-file or multi-display icon-position service.

Third-party global menus and full macOS compatibility remain out of scope.

The Dock v1 surface is documented in [`docs/M3_DOCK.md`](M3_DOCK.md). It
replaces the fixed-width dock panel with a responsive full-width layout and
adds active/running indicators plus focus/minimize/restore behavior backed by
the existing Wayfire window controller.

### Current product-slice execution sequence

The next Northstar work is intentionally split into small slices so every
overnight increment has a visible user-facing result and a reversible merge
boundary:

1. **Desktop surface:** live Desktop-folder icons, safe double-click/open
   behavior, rename/delete/properties actions, and an empty/error state.
2. **Dock surface:** full-width responsive layout, pinned launchers, running
   application state, focus/minimize toggling, and an overflow-safe app strip.
3. **Files polish:** Finder-style tiles/list views, metadata sorting, ascending
   and descending order, directory-first ties, search, Open With, associations,
   recoverable Trash, and mounted-volume read-only boundaries.
4. **Application discovery:** parse user/system `.desktop` entries and the
   Northstar `.app` contract, validate executable/icon provenance, and expose a
   safe catalog without executing untrusted metadata.
5. **Launch and associations:** connect file types to discovered applications,
   preserve user-scoped defaults, support Open With and Forget Default, and
   pass only validated paths as launch arguments.
6. **Welcome experience:** make the Welcome app explain the desktop, expose
   first-run actions, show validation/error feedback, and remain useful when
   optional applications are absent.
7. **Software Center:** turn the current read-only inventory into a clearly
   staged catalog with package details, install/remove intent, pending-action
   preview, authorization boundaries, and explicit unsupported-state messages.
8. **Northstar visual system:** use the approved logo/icon family consistently
   across the menu, dock, Files, Apps, Welcome, Software Center, dialogs, and
   empty states, with light/dark contrast checks.
9. **Session lifecycle:** finish startup/login entry points, controlled logout,
   restart and shutdown, shell-crash recovery, duplicate-session prevention,
   and operator-facing diagnostics.
10. **Manual acceptance:** reinstall the combined build into a separate VM
    checkout, validate the noVNC desktop interactively, reboot gracefully,
    repeat the core workflows, and record any native DRM/KMS limitation.
11. **Release hygiene:** update the roadmap and quality gates with evidence,
    commit each slice locally, push its `codex/` branch, open a draft PR,
    promote it after validation, squash-merge in dependency order, delete
    merged branches, prune refs, and verify clean main.

Every slice must have a focused implementation, native/unit coverage where
feasible, a reproducible VM build/test record, a manual gate when UI behavior
is involved, and a documented limitation when the current nested graphics
path cannot prove direct DRM/KMS behavior.

### Expanded seven-hour execution matrix

This is the working order for a long Northstar sprint. The timeboxes are
guidance, not permission to mark an interactive gate complete without seeing
the result. A failed manual step becomes the next focused fix, followed by the
targeted test and the full regression gate before continuing.

| Timebox | Step | Expected output | State |
| --- | --- | --- | --- |
| 00:00-00:15 | Branch and PR audit | Confirm clean `codex/` branch, `origin/main` base, open integration PR, and no unrelated worktree changes | Complete |
| 00:15-00:35 | VM source sync and install | Copy the exact branch archive to the separate VM checkout, build, and install into `$HOME/.local` | Complete |
| 00:35-00:55 | Automated regression gate | Run 17/17 Qt tests, QML surface contracts, session entry-point/supervisor checks, and app self-tests | Complete |
| 00:55-01:10 | Compiled startup smoke | Start the built shell with the offscreen QPA and confirm no QML reference, binding, or syntax errors; record expected Layer Shell notices | Complete |
| 01:10-01:35 | noVNC launch and shell restart | Start the installed session from the supported console path, confirm the branded desktop appears, restart only the shell, and verify the desktop returns | Open: manual |
| 01:35-01:55 | Desktop-folder projection | Create `Desktop` if absent, create a folder and text file, refresh/watch the desktop, open both icons, and verify the Files location | Open: manual |
| 01:55-02:20 | Desktop icon operations | Double-click, select, rename, delete, Properties, and empty/error states; confirm invalid paths stay inside the home boundary | Open: manual |
| 02:20-02:40 | Icon layout behavior | Drag icons, verify bounds and nearest-free-cell snapping, restart, confirm positions persist, then test Settings > Appearance reset | Open: manual |
| 02:40-03:05 | Files mutations | Create a folder and text file, rename each, navigate Home/Desktop/Documents, sort ascending/descending, switch grid/list views, and search | Open: manual |
| 03:05-03:25 | Trash recovery | Delete a disposable file/folder with the single `Delete` action, inspect Trash, restore it, and verify the original path and contents | Open: manual |
| 03:25-03:50 | Associations and editor | Use Open With, confirm typed MIME/extension filtering, open in Northstar Text Editor, edit/save, reopen, and verify Firefox is not forced for every file | Open: manual |
| 03:50-04:10 | Apps, Welcome, and Software Center | Refresh application discovery, launch a known `.desktop`/`.app`, exercise Welcome actions, search Software Center, and confirm unsupported mutations explain themselves | Open: manual |
| 04:10-04:35 | Dock and window controls | Confirm full-width alignment, Files/Apps shortcuts, running indicators, focus, minimize/restore, close, and no automatic Terminal launch | Open: manual |
| 04:35-04:55 | Settings and visual system | Check logo/background, menu icon, light/dark appearance, settings navigation, diagnostics, keyboard mappings, and responsive layout at the VM size | Open: manual |
| 04:55-05:15 | Session lifecycle | Test confirmed logout, then a graceful restart and shutdown path; verify the supervisor owns only its session children | Open: manual |
| 05:15-05:30 | Recovery diagnostics | Trigger or inspect a safe shell-failure/restart state, confirm restart count and terminal failure are visible, and ensure duplicate sessions are rejected | Open: manual or log |
| 05:30-05:50 | Clean reboot repeat | Reboot the VM gracefully, log in again, repeat the minimum desktop/file/app path, and confirm no stale installed binary or auto-start regression | Open: manual |
| 05:50-06:10 | Evidence pack | Capture commit, VM checkout, install prefix, commands, screenshots/log excerpts, pass/fail notes, and deferred DRM/KMS limitation | Pending |
| 06:10-06:30 | Review and hardening | Convert any failure into a focused fix, rerun affected tests plus the full gate, update roadmap/quality documentation, and inspect `git diff --check` | Pending |
| 06:30-06:45 | Cloud promotion | Mark the integration PR ready only when manual gates pass, squash-merge in dependency order, and verify the resulting `main` commit | Pending |
| 06:45-07:00 | Branch cleanup and handoff | Delete merged local/remote feature refs after verification, prune stale refs, create the next `codex/` branch, and leave exact next tests | Pending |

The minimum automated rerun after any shell/QML change is `env
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`,
`make qml-surface-test`, and the compiled startup smoke. The minimum manual
rerun after any Files, Dock, association, or session change is the affected
workflow plus the clean reboot repeat. Direct DRM/KMS, native multi-display,
and physical Intel/AMD acceptance remain separate hardware gates even when
this entire VM matrix passes.

For an unattended or extended sprint, use the more granular
[`docs/OVERNIGHT_SPRINT.md`](OVERNIGHT_SPRINT.md). It expands the matrix into
97 micro-steps across eight branch/PR slices, defines a checkpoint after each
slice, and specifies what remains open when noVNC or native graphics evidence
is unavailable. The micro-steps are deliberately grouped into cohesive PRs;
they are not an instruction to create dozens of trivial branches.

## M4: Packages, updates, and rollback

The first Software Center foundation is documented in [`docs/M4_SOFTWARE.md`](M4_SOFTWARE.md). It reads and searches the installed FreeBSD package inventory without mutating the host. The merged M4 package-trust work validates a FreeBSD `pkg`-aligned UCL policy, fingerprint stores, a provenance-aware publication manifest, catalogue integrity, a read-only RSA publication-signature envelope, the native `pkg repo`/client publication smoke contract, a bounded root-owned update-helper request protocol, and independent broker-side revalidation; it also exposes a read-only update-authorization preflight. It does not close M4: an actual signed development/stable repository built from protected Poudriere inputs, reviewed privileged deployment, boot-environment creation before upgrades, rollback documentation, and N-1 compatibility tests are still required.

Pass only when project components install through `pkg`, repository metadata is signed, N-1 to N upgrades work, rollback restores the prior shell and package set, and user documents survive.

## M5: Reproducible image and installer

Produce a ZFS QCOW2 image first, then raw and USB images, and only then an ISO. Use official release sets, pinned package repositories, project configuration, and established FreeBSD release tooling.

Pass only when clean builders produce recorded inputs and checksums, QEMU/Proxmox boots the image, the installer creates UEFI GPT/root-on-ZFS, first boot reaches graphical login and the shell, and update/rollback work after installation.

## M6: Alpha hardware release

Support x86-64 UEFI systems with SATA, NVMe, and virtio disks; QEMU/Proxmox; one tested Intel graphics lane; one tested AMD graphics lane; wired networking; and specifically tested Intel Wi-Fi adapters. NVIDIA is unsupported or experimental, and ARM64/Apple hardware is out of scope.

Alpha requires install, boot, login, Qt/Xwayland/browser applications, file management, settings, update, rollback, diagnostics, shell crash recovery, and clean shutdown.

## Historical first eight implementation pull requests

1. Charter and architecture (this repository foundation).
2. FreeBSD host validation and bootstrap.
3. CI foundation, including native FreeBSD checks.
4. Qt shell skeleton and tests.
5. Layer-shell top bar.
6. Dock and `.desktop` application catalog/launch.
7. Session supervisor.
8. First FreeBSD port for `northstar-shell`.

The Northstar-produced ISO is deliberately absent from the first eight pull
requests. M0 may use a stock FreeBSD installer ISO as an external, ignored
validation input; that media is not a Northstar release artifact.

The current work continues with milestone-scoped slices rather than trying to
land an ISO prematurely. The immediate product target is the M4 package-trust
and update-safety sequence described above.
