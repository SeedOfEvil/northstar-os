# M6 Intel Alpha physical session correction — 2026-08-21

Status: **R90 PHYSICAL ACCEPTANCE COMPLETE — exact checksummed USB, installer/Setup, First Boot, first-attempt login, logout, reboot, and post-reboot native Intel session evidence pass; Proxmox fallback acceptance remains before merge**

## R90 integrated physical acceptance — 2026-08-22

The single replacement candidate was assembled from project commit
`304c8ae29c9d12b5330d1e793cdb9f856bca4b90`. Its 20 GiB raw installer image
was streamed directly to the Kingston USB and then read back in full. Both the
source stream and physical-device readback matched SHA-256
`08ddc4bc854afe7774795f964f413e4ff773ea0848bffa36af17f186a00945dc`.

On the physical Whiskey Lake / Intel UHD 620 laptop, the user confirmed that
the R90 installer Setup completed successfully, the branded First Boot wizard
completed successfully, and the newly provisioned desktop opened on the first
login attempt. This is the first integrated-image confirmation of the repaired
SDDM and hardware-aware session path; it replaces the earlier live-correction
result as the installer acceptance candidate.

The report closed the destructive installation, First Boot, and first-login
gates. The user subsequently confirmed that logout/login and a full reboot/login
also returned to a stable desktop on the first attempt.

Read-only SSH inspection after that reboot recorded:

- selector `mode=native` with `reason=drm-card-and-render-ready`;
- loaded `i915kms.ko` and `drm.ko`, with `/dev/dri/card0 -> ../drm/0` and
  `/dev/dri/renderD128 -> ../drm/128`;
- `sddm-0.21.0.36_6` and `setxkbmap-1.3.5` from the integrated runtime;
- administrator membership in `video`;
- no `[output:X11-1]` or `mode = 1280x800` inheritance in the administrator's
  Wayfire configuration;
- live `sddm-helper -> dbus-run-session -> northstar-session -> wayfire`
  lineage; and
- one Wayfire process with supervisor state `running`, shell present, and
  `restart_count=0`.

These observations complete the focused physical display/session merge gate.
Completed physical tests above must not be repeated.

## R89 physical acceptance findings

R89 booted from the checksummed USB, rendered the installer at the panel's
full size, completed the destructive installation, and completed the branded
First Boot wizard successfully. Those gates pass and must not be repeated for
this correction.

The first administrator still reached the small top-left desktop only by
choosing the Proxmox X11 fallback. The one-time First Boot entry remained in
SDDM after provisioning and returned immediately to the greeter when selected.
Live evidence showed that Intel DRM was healthy and both `/dev/dri/card0` and
`/dev/dri/renderD128` existed, but the selector recorded `mode=fallback`.

FreeBSD exposes those convenience paths as root-owned links into `/dev/drm`.
The selector followed the device type check with a blanket link rejection, so
it rejected the real FreeBSD DRM layout that the physical gate was meant to
recognize. The fallback then correctly loaded the nested `1280x800` template
inside SDDM's `1920x1080` Xorg display, leaving the untouched greeter visible
and frozen around the nested compositor.

The correction accepts only the constrained `../drm/<numeric>` devfs link
shape and still requires the resolved production node to be a character
device. First Boot provisioning now removes its generated runtime descriptor
and reruns the selector after protected pending state is sealed, so SDDM sees
the settled hardware session without retaining the one-time entry. Regression
fixtures cover the FreeBSD link layout and the post-provision refresh.

The first native login then exposed FreeBSD bug 296052 in the image's
`sddm-0.21.0.36_3`: authentication succeeded and the Wayland session started,
but `sddm-helper` closed PAM and exited almost immediately while Wayfire
continued as an orphan. Repeated login attempts therefore bounced to the
greeter or revealed an already-running orphaned desktop; that behavior was
diagnostic evidence, not acceptance.

FreeBSD completed the Wayland-session repair in the ports tree after the
quarterly package used for R89. The physical system was deliberately upgraded
to official `sddm-0.21.0.36_6` plus its `setxkbmap-1.3.5` dependency. Temporary
PAM and D-Bus-wrapper experiments were removed before the acceptance attempt.
The clean first login passed: the live lineage remained
`sddm-helper -> dbus-run-session -> northstar-session -> wayfire`, the selector
recorded `mode=native` with `reason=drm-card-and-render-ready`, and the session
reported `restart_count=0` with the shell running. The user confirmed that the
full-size desktop remained open on the first attempt.

The supervisor retains its guarded private runtime fallback for FreeBSD's
known SDDM runtime paths and a one-second output-settle delay after Wayland
socket readiness. The image assembler now rejects a runtime closure unless it
contains the accepted official SDDM and `setxkbmap` versions, preventing the
older quarterly package from silently returning in the replacement RC.

## R88 USB rejection

R88 passed raw-device checksum verification and booted its UEFI loader, kernel,
and live ZFS root on the physical laptop. Its SSH host key differed from the
installed system, independently confirming that firmware selected the USB even
though the resulting screen resembled a normal Northstar boot.

The media did not open the installer because the hardware-aware selector makes
SDDM read only boot-generated session directories, while the USB assembler
still placed `northstar-installer.desktop` in the former static X11 directory.
The selector now recognizes the protected installer-media marker and publishes
only the installer X11 session into SDDM's generated directory. Normal images
continue to publish either native Wayland or the Proxmox X11 fallback, plus
First Boot only while its pending marker exists. A focused regression fixture
covers the installer override on DRM-capable hardware.

R88 must not be used for installation. Its successfully checksummed USB write
and boot proof remain valid evidence for media transport and UEFI boot only;
they are not installer acceptance.

This record continues the Intel Alpha installer candidate after the first
successful installation on physical hardware. It does not repeat or replace
the completed destructive install, first-boot, or direct DRM observations.

## Completed physical evidence

The candidate installed and completed First Boot on a Whiskey Lake laptop
after firmware storage mode was changed from Intel RST/RAID to AHCI. The
installed Intel UHD 620 device is `8086:3ea0`.

With `drm-66-kmod`, the Kaby Lake Intel firmware package, and `i915kms` loaded,
the installed system exposed `/dev/dri/card0` and `/dev/dri/renderD128`, loaded
`i915/kbl_dmc_ver1_04.bin`, and retained the administrator's `video` group
membership. The Intel AC 9560 was also detected as `iwm0` after installing its
9000-series firmware package. These observations close the initial device and
driver proof for this laptop; they are not being rerun as part of the source
correction.

The desktop did not pass with DRM enabled because the image still selected the
Proxmox-oriented X11/pixman/nested-Wayfire session. That mismatch produced a
black screen with a cursor. Disabling `i915kms` restored the known-good scfb
fallback, confirming that the product session contract—not package presence or
the shell itself—was the immediate blocker. Wi-Fi configuration remains paused
until the native display path passes.

## Corrected image contract

The Intel Alpha image now:

- captures the proven DRM, Kaby Lake graphics firmware, AC 9560 firmware, and
  `xrandr` packages in the exact offline runtime bundle;
- enables `i915kms` and a boot-time Northstar session selector;
- publishes the native Wayland session only when both a DRM card node and a
  render node exist;
- publishes only the explicit X11/pixman fallback when either DRM node class is
  unavailable, preserving the Proxmox basic-VGA lane;
- keeps First Boot available while protected pending state exists;
- provisions physical users from an output-agnostic native Wayfire template,
  while fallback users retain `[output:X11-1] mode = 1280x800`; and
- runs `xrandr --auto` before the bare-X11 installer and First Boot applications
  read their screen dimensions, addressing the top-left conservative-mode
  rendering observed on the laptop.

The selector writes `/var/run/northstar/session-mode.conf`; one-time account
provisioning records the selected mode in its sealed completion marker and
copies only the corresponding Wayfire template into the new administrator's
home directory.

## Focused local evidence

The following checks pass from the Windows worktree through Git's POSIX shell:

- hardware selector transition matrix: no DRM, full card+render DRM, and
  incomplete DRM;
- native-versus-fallback First Boot provisioning;
- guarded installer executor including immutable session inputs;
- QCOW2 assembler offline/tamper-evident preflight contract; and
- POSIX syntax checks for every changed image/session script.

The broader QML surface script currently reports an unrelated pre-existing
`DesktopBackground.qml` contract failure on this source baseline. The new
Wayfire configuration assertions are covered by the focused selector test.

Native FreeBSD packaging at commit `61b9b00` completed all 423 Ninja steps and
passed the canonical 38/38 offscreen CTest suites. The reviewed package is
`northstar-0.2.7-amd64.pkg`, 22,037,016 bytes, SHA-256
`0208758979f1c7ea33b1711b249a82ffb31093538a1fc1612ff4a2bf289b6dc1`,
with ABI `FreeBSD:15:amd64` and origin `x11/northstar`. Its inventory includes
the native template and corrected First Boot/session files.

The corresponding 240-package offline bundle passed deterministic capture
with record digest
`2ac63720f3d1be3cfafb088b256ff4691b5e505935c403d4d701a59fa3df5fea`.

## Remaining promotion gates

R90 is the one deliberate replacement RC. Its exact runtime-bundle and image
provenance validation, file-backed storage reset, VirtIO interruption/retry,
`qemu-img check`, and snapshot-only UEFI boot gates passed before the physical
write. A complete disposable Proxmox install/First Boot/login check remains;
Proxmox must report fallback mode.

The exact checksummed USB has passed preferred-mode installer sizing,
installation, First Boot, native desktop entry on the first login, clean
logout/reboot, selector state `native`, `i915kms` with both DRM nodes present,
shell interaction, and the absence of an inherited `output:X11-1` stanza. The
physical display/session gate is complete, so Wi-Fi configuration may resume.
Do not merge until the remaining disposable Proxmox fallback acceptance is
also recorded.
