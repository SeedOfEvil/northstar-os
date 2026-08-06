# Third-party licence inventory

Northstar does not vendor third-party source code in this initial repository. It consumes upstream FreeBSD components and packages, and it will preserve their notices in source, package, image, and binary distribution contexts as required by each licence.

This inventory is a starting point, not a substitute for reviewing the exact package versions used in a release.

| Component or source | Planned use | Licence and notice requirement |
| --- | --- | --- |
| FreeBSD base system | Kernel, base utilities, ZFS, networking, drivers | BSD-family licences; retain upstream notices |
| FreeBSD Ports and packages | Qt, Wayland, D-Bus, seatd, Xwayland, Wayfire, utilities | Varies by port; use the port's licence metadata |
| Qt 6 | Shell toolkit, QML, project applications | Follow the selected Qt package's licence and distribution terms |
| Wayland protocols | Display protocol interfaces | Retain upstream licence notices |
| Wayfire | Initial Wayland compositor | Follow the upstream project licence and package metadata |
| Xwayland | X11 application compatibility | Follow the upstream project licence and package metadata |
| QEMU or Proxmox tooling | VM and release validation | Follow the tool's own licence terms; not shipped by Northstar |

Before a release, tooling must generate an exact inventory from the resolved package set. A release is blocked when a package's licence is unknown, incompatible with the distribution plan, or missing required notices.

Northstar original code and documentation use the BSD-2-Clause licence in [`LICENSE`](LICENSE).
