# Northstar roadmap

Northstar advances through evidence-backed user-visible milestones. A merged
implementation is not a hardware-support claim: each lane closes only with the
automated, VM, image, or physical evidence named for it.

## Current milestone state

| Milestone | Status | Current truth |
| --- | --- | --- |
| M0: development base | Accepted | FreeBSD 15.1 amd64 bootstrap and the supplemental Proxmox development lane are established. |
| M1: shell seed | Partially accepted | The single-display shell is accepted; native multi-display placement remains open. |
| M2: desktop session | Accepted on Intel; fallback retained | SDDM, First Boot, supervised native Wayfire, lifecycle actions, and shell restart/resume are physically accepted on the Intel lane. Proxmox keeps its explicit X11/pixman fallback. |
| M3: core desktop | Accepted | Files, desktop, Dock, Settings, search, Quick Look, notifications, application bundles, and shared window chrome form the usable desktop baseline. |
| M4: packages and rollback | Foundation accepted | Signed repository provenance, guarded package mutation, ZFS boot environments, update failure recovery, and explicit rollback are implemented and tested. Production hosting and persistent signing custody remain release infrastructure. |
| M5: image and installer | Accepted | The r85 image passed assembly, UEFI boot, full-disk installation, First Boot, graphical login, signed update, failure recovery, rollback, and `/home` preservation. |
| M6: alpha hardware | In progress | The R90 Whiskey Lake/Intel UHD 620 installation and native DRM session are accepted. Intel Wi-Fi, modern Bluetooth pairing and OBEX transfer, audio, brightness, battery/power, suspend/resume, display modes, and pointer controls passed focused physical acceptance. AMD, native multi-display, and the consolidated Alpha regression remain open. |
| M7: daily-driver desktop | In progress | Files v3, Text Editor v2, Settings v2, persistent notifications, writable radios, software install/remove, Aurora Glass, and contextual application menus are merged. Broader daily-driver hardening continues without weakening M6 release gates. |

## Accepted physical Intel lane

The current physical reference is a FreeBSD 15.1 amd64 UEFI laptop with Intel
UHD 620 (`8086:3ea0`) and Intel Wireless-AC 9560. The accepted R90 installation
and subsequent merged physical slices:

- installed to NVMe/root-on-ZFS and completed First Boot;
- selected native Wayfire from real DRM card and render nodes;
- logged in through SDDM on the first attempt and remained stable after logout
  and reboot;
- associated through the graphical WPA2 wizard, obtained DHCP, restored Wi-Fi
  after reboot, and retained working desktop radio controls;
- paired a modern Android phone with Secure Simple Pairing, retained the bond,
  and transferred files in both directions through OBEX Push;
- exposed physically accepted audio, brightness, battery, power, suspend,
  resolution, mouse, and touchpad controls; and
- passed the Aurora Glass and contextual-menubar visual interaction gates at
  the laptop's 1920x1080 mode.

The detailed evidence remains under [`validation/`](validation/), especially
[`M6_INTEL_ALPHA_PHYSICAL_SESSION_2026-08-21.md`](validation/M6_INTEL_ALPHA_PHYSICAL_SESSION_2026-08-21.md),
[`M6_INTEL_WIFI_PHYSICAL_2026-08-22.md`](validation/M6_INTEL_WIFI_PHYSICAL_2026-08-22.md),
and [`M6_BLUETOOTH_FILE_TRANSFER.md`](validation/M6_BLUETOOTH_FILE_TRANSFER.md).
Completed physical tests are not repeated unless a later change invalidates
their exact boundary.

## Immediate work

1. **QML decomposition.** Refactor oversized surfaces, beginning with
   `FileBrowserWindow.qml`, without changing behavior or visual acceptance.
2. **Alpha closure.** Produce a consolidated Intel evidence bundle, add one
   AMD DRM/KMS lane, validate native multi-display behavior, and repeat only
   the release-spanning session, suspend, networking, update/rollback, and
   clean-install regressions whose boundaries changed after R90.
3. **Release operations.** Establish protected package/image publication,
   stable hosting, persistent signing-key custody, and an explicit Alpha
   promotion/rollback runbook.
4. **Daily-driver hardening.** Continue focused reliability and accessibility
   work in existing applications before adding another major application.

## Release boundary

Alpha support remains amd64, UEFI, root-on-ZFS, Wayland/Xwayland, and the
declared QEMU/Proxmox plus narrow physical Intel/AMD matrix. NVIDIA is
experimental or unsupported; ARM64 and Apple hardware are out of scope.

The Alpha milestone closes only when the Intel and AMD matrix records are
complete, the required consolidated evidence passes, and one deliberate
release candidate completes clean install, login, applications, networking,
suspend/resume, update, rollback, diagnostics, shell recovery, and shutdown.
The exact gates are defined in [`QUALITY_GATES.md`](QUALITY_GATES.md),
[`M6_ALPHA_READINESS.md`](M6_ALPHA_READINESS.md), and
[`M6_PHYSICAL_HARDWARE_OPERATOR_RUNBOOK.md`](M6_PHYSICAL_HARDWARE_OPERATOR_RUNBOOK.md).

The former long-form milestone chronology is preserved in
[`ROADMAP_HISTORY.md`](ROADMAP_HISTORY.md).
