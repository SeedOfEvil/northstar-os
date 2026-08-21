# M6 Intel Alpha physical session correction — 2026-08-21

Status: **IMPLEMENTED AND PACKAGED — new RC and focused physical acceptance pending**

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

Build exactly one new RC after the clean source commit is available to the
FreeBSD builder. Before physical reuse, require exact runtime-bundle and image
provenance validation, file-backed storage reset, VirtIO interruption/retry,
`qemu-img check`, snapshot-only UEFI boot, and a complete disposable Proxmox
install/First Boot/login check. Proxmox must report fallback mode.

The merge gate remains focused physical acceptance from the exact checksummed
USB: preferred-mode installer sizing, install and First Boot, selector state
`native`, native Wayfire login with `i915kms` and both DRM nodes present, shell
interaction, clean logout/reboot, and no inherited `output:X11-1` stanza. Wi-Fi
configuration resumes only after those display/session checks pass. Do not
merge this branch before that confirmation.
