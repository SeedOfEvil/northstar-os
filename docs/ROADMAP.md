# Northstar roadmap

The project advances through user-visible milestones with explicit pass gates. A milestone is not complete because a component starts once on one machine; it is complete when its clean, repeatable acceptance evidence exists.

| Milestone | Name | Status | Primary outcome |
| --- | --- | --- | --- |
| M0 | Reproducible development desktop | Not started | A clean FreeBSD 15.1 amd64 VM becomes the approved development environment |
| M1 | Shell seed | Not started | Top bar and dock render on every connected display |
| M2 | Desktop session | Not started | Session, services, supervision, notifications, and lifecycle work coherently |
| M3 | Core desktop | Not started | Filer, settings, search, overview, associations, and project apps form a usable desktop |
| M4 | Packages, updates, rollback | Not started | Signed packages and ZFS boot-environment rollback work end to end |
| M5 | Reproducible image and installer | Not started | Pinned inputs produce a bootable UEFI root-on-ZFS image |
| M6 | Alpha hardware release | Not started | The supported VM and narrow Intel/AMD hardware matrix meets the alpha definition |

## M0: Reproducible development desktop

Deliver `check-host.sh`, an idempotent `bootstrap-dev.sh`, a pinned package manifest, Wayland/seatd/Wayfire/Xwayland, the Qt 6 development environment, a terminal and test applications, and a manual recovery path.

Pass only when the host is exactly FreeBSD 15.1 amd64, bootstrap succeeds from a clean VM and is harmless on a second run, Wayfire starts unprivileged, a native Wayland Qt application and an X11 application through Xwayland launch, installed versions are captured, and no project file is copied into `/usr/bin` or `/usr/lib`.

## M1: Shell seed

Deliver one `northstar-shell` process with a top bar, dock, clock, active-window title, static system menu, pinned application list, launcher buttons, light/dark design tokens, and one surface per connected display. Use Qt 6, QML, and Layer Shell behind a platform interface.

Pass only when surfaces reserve or overlay space correctly, appear on every display, restart without terminating applications, launch terminal and Firefox, run unprivileged, and pass native Qt unit tests.

## M2: Desktop session

Deliver `northstar-session`, environment setup, D-Bus startup, logout/reboot/shutdown, launcher, notifications, settings, file associations, removable-volume events, crash restart policy, and diagnostic logging.

Pass only when login starts exactly one session, shell crashes are detected and restarted, logout terminates only the user session, privileged lifecycle actions are controlled, launches record PID and identity, and logs contain no secrets.

## M3: Core desktop experience

Deliver the filer, settings, search, application overview, keyboard mapping, drag-and-drop launching, desktop icons or volumes, trash integration, the first `.app` implementation, and a project-owned Qt global menu.

Third-party global menus and full macOS compatibility remain out of scope.

## M4: Packages, updates, and rollback

Deliver Ports overlays, Poudriere configuration, signed development/stable repositories, an upgrade command, boot-environment creation before upgrades, rollback documentation, and compatibility tests.

Pass only when project components install through `pkg`, repository metadata is signed, N-1 to N upgrades work, rollback restores the prior shell and package set, and user documents survive.

## M5: Reproducible image and installer

Produce a ZFS QCOW2 image first, then raw and USB images, and only then an ISO. Use official release sets, pinned package repositories, project configuration, and established FreeBSD release tooling.

Pass only when clean builders produce recorded inputs and checksums, QEMU/Proxmox boots the image, the installer creates UEFI GPT/root-on-ZFS, first boot reaches graphical login and the shell, and update/rollback work after installation.

## M6: Alpha hardware release

Support x86-64 UEFI systems with SATA, NVMe, and virtio disks; QEMU/Proxmox; one tested Intel graphics lane; one tested AMD graphics lane; wired networking; and specifically tested Intel Wi-Fi adapters. NVIDIA is unsupported or experimental, and ARM64/Apple hardware is out of scope.

Alpha requires install, boot, login, Qt/Xwayland/browser applications, file management, settings, update, rollback, diagnostics, shell crash recovery, and clean shutdown.

## First eight implementation pull requests

1. Charter and architecture (this repository foundation).
2. FreeBSD host validation and bootstrap.
3. CI foundation, including native FreeBSD checks.
4. Qt shell skeleton and tests.
5. Layer-shell top bar.
6. Dock and `.desktop` application launch.
7. Session supervisor.
8. First FreeBSD port for `northstar-shell`.

The ISO is deliberately absent from the first eight pull requests.
