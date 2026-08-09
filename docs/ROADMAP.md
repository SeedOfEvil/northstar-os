# Northstar roadmap

The project advances through user-visible milestones with explicit pass gates. A milestone is not complete because a component starts once on one machine; it is complete when its clean, repeatable acceptance evidence exists.

| Milestone | Name | Status | Primary outcome |
| --- | --- | --- | --- |
| M0 | Reproducible development desktop | Supplemental VM lane validated; direct DRM/KMS gate pending | A clean FreeBSD 15.1 amd64 VM becomes the approved development environment |
| M1 | Shell seed | Single-display product slice validated; multi-display acceptance pending | Top bar and dock render correctly on every connected display |
| M2 | Desktop session | Development session lane validated; production display-manager and service integration pending | Session, services, supervision, notifications, and lifecycle work coherently |
| M3 | Core desktop | Most user-facing slices implemented; acceptance closure and remaining polish pending | Filer, settings, search, overview, associations, and project apps form a usable desktop |
| M4 | Packages, updates, rollback | Package policy, fingerprint stores, provenance preview, publication signature verification, native `pkg` publication smoke, and bounded helper contract merged; protected release publication and update/rollback remain | Signed packages and ZFS boot-environment rollback work end to end |
| M5 | Reproducible image and installer | Not started | Pinned inputs produce a bootable UEFI root-on-ZFS image |
| M6 | Alpha hardware release | Not started | The supported VM and narrow Intel/AMD hardware matrix meets the alpha definition |

## Current baseline and clear path forward

The current `main` line contains the merged Northstar shell, session, Files,
application-bundle, notification, keyboard, read-only Software Center, and
M4 package-trust slices. The NSTAR-DEV01 FreeBSD 15.1 development VM is the
active validation environment. Its nested X11/pixman lane is working and the
latest recorded native suite passed 13/13 Qt tests plus the script/session
checks.

The VM is intentionally not being treated as final graphics evidence. Its
Proxmox basic-VGA/scfb path does not provide a guest DRM render device, so
direct DRM/KMS, multi-display, and native display-manager claims remain open
until the appropriate hardware or graphics path is available. This keeps the
product work moving while preserving honest release gates.

The next work is ordered as follows:

1. **Close the current desktop acceptance lane.** Re-run the M3 acceptance
   matrix on a clean install: Files operations and Trash recovery, Open With
   and reversible associations, home search, mounted-volume boundaries,
   Files-to-Apps launching, keyboard mappings, settings persistence, and the
   Software Center search/refresh flow. Record the exact VM commit and manual
   results.
2. **Close the remaining M1/M2 development evidence.** Capture a multi-display
   Layer Shell run when hardware permits, and separately validate the branded
   display-manager session, controlled lifecycle actions, and the production
   privilege boundary. The current Proxmox fallback remains supplemental.
3. **Close M4 publication evidence.** The policy, fingerprint-store,
   publication-provenance, catalogue-integrity, read-only signature
   verification, and native `pkg` publication/client smoke foundations are
   merged. The remaining trust evidence is an actual signed development/stable
   `pkg` repository publication built from pinned Ports/Poudriere inputs with
   protected key custody and recorded provenance.
4. **Build safe update and rollback.** The bounded root-owned helper request
   contract is now defined and tested. Next establish its privileged broker,
   preflight a named `bectl` boot environment before upgrades, validate N-1 to
   N upgrades and rollback, and prove that home data survives both paths. No
   package mutation is exposed before these gates pass.
5. **Produce the reproducible image and installer.** Only after M4 has current
   evidence, pin the image inputs, build QCOW2 first, validate UEFI GPT/root-on-
   ZFS installation and first boot, then add raw/USB/ISO outputs.
6. **Run the alpha hardware matrix.** Validate the supported VM plus the
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

Pass only when login starts exactly one session, shell crashes are detected and restarted, logout terminates only the user session, privileged lifecycle actions are controlled, launches record PID and identity, and logs contain no secrets.

## M3: Core desktop experience

Deliver the filer, settings, search, application overview, keyboard mapping, drag-and-drop launching, desktop icons or volumes, trash integration, the first `.app` implementation, and a project-owned Qt global menu.

The first filer slice is documented in [`docs/M3_FILES.md`](M3_FILES.md). It
adds a home-folder-scoped Files window with folder navigation, default file
opening, path-boundary protection, search, explicit Open With selection, and
top-menu/dock entry points. The first project-defined `.app` slice now
discovers a validated development bundle, surfaces its owned icon, passes its
owned executable directly to the launcher, supports dragging regular Files
entries onto Apps tiles for explicit file launching, provides a Locations bar
for mounted non-pseudo volumes with read-only navigation, and persists the
Appearance preference across shell restarts. The first project-owned menu
accelerators and application-level keyboard mappings now share a tested
command catalog. Project `.app` bundles now require and expose a tested
source/package/revision provenance record. Cryptographic signing, package
repository verification, and compositor-wide/global third-party menu
integration remain follow-on slices. File opening now supports reversible,
user-scoped extension defaults with an explicit Open With escape hatch.

Third-party global menus and full macOS compatibility remain out of scope.

## M4: Packages, updates, and rollback

The first Software Center foundation is documented in [`docs/M4_SOFTWARE.md`](M4_SOFTWARE.md). It reads and searches the installed FreeBSD package inventory without mutating the host. The merged M4 package-trust work validates a FreeBSD `pkg`-aligned UCL policy, fingerprint stores, a provenance-aware publication manifest, catalogue integrity, a read-only RSA publication-signature envelope, the native `pkg repo`/client publication smoke contract, and a bounded root-owned update-helper request protocol; it also exposes a read-only update-authorization preflight. It does not close M4: an actual signed development/stable repository built from protected Poudriere inputs, a privileged broker, boot-environment creation before upgrades, rollback documentation, and N-1 compatibility tests are still required.

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
