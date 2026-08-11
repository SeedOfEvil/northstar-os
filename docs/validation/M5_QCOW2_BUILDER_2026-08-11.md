# M5 QCOW2 builder validation - 2026-08-11

## Scope

PR76 was exercised on the explicitly disposable `NSTAR-CLONE` FreeBSD builder
at `192.168.1.204`. The accepted `NSTAR-DEV01` validation VM was not modified.
The build was bound to project commit
`c46425a6e705b06c0e6b55260f498bfbe8f83fef`.

## Assembly evidence

- Input preparation and non-mutating preflight passed for 235 immutable runtime
  packages and a 16 GiB target.
- Offline package installation required two bounded passes. The first
  registered the dependency closure; the second converged. Exact package count
  and name/version verification passed before image export.
- The assembler emitted `northstar-15.1-amd64.qcow2`, immutable provenance,
  partition/ZFS layout, runtime records, and recorded `qemu-img` evidence.
- Artifact size: 3,249,405,952 bytes; virtual size: 16 GiB.
- Artifact SHA-256:
  `f560026bb98d5912ecd8d15144180493f6e12f8e382ce20f061fb05dcbf0336a`.
- `qemu-img check` reported no errors. No image md device or temporary image
  zpool remained attached after assembly.

## Boot evidence

A QEMU 11.0.2 TCG guest used EDK2 x64 firmware, virtio block/network devices,
and `snapshot=on`. The serial log proved:

- EDK2 loaded `EFI/BOOT/BOOTX64.EFI` from the GPT EFI partition;
- FreeBSD 15.1 detected a 1280x800 EFI framebuffer and the 16 GiB virtio disk;
- ZFS mounted `nstar_c46425a6e705/ROOT/default`;
- `vtnet0` reached link-up; and
- `FreeBSD/amd64 (northstar-image)` reached its `login:` prompt.

The isolated guest also returned `SSH-2.0-OpenSSH_10.0 FreeBSD-20250801` through
a temporary localhost-only diagnostic forward. All QEMU processes were stopped
after capture, and the source QCOW2 was not changed.

## Remaining focused acceptance

The Proxmox noVNC screen observed during this work was the existing clone
host's healthy Northstar greeter, not the nested QCOW2. Nested TCG framebuffer
captures were black and are not counted as graphical evidence. Before PR76 is
promoted, import a copy of the QCOW2 into a separate Proxmox VM and validate the
branded greeter, Northstar session, 1280x800 interaction, clean shutdown, and
image-local update/rollback. This keeps serial boot evidence distinct from the
graphical product gate.

The first VM 104 import reached the branded greeter and exposed a development
image defect: the requested SDDM autologin account remained locked by the base
image policy. PR76 now unlocks that account only when
`--development-autologin` is explicit and records the passwordless-local policy
in image provenance. A rebuilt artifact must repeat boot and graphical
validation before promotion.

The corrected account then authenticated automatically, exposing a second
integration defect in the accepted 0.1.4 package: its X11 wrapper searched only
`~/.local/bin` even though the package installed the supervisor and shell in
`/usr/local/bin`. The image now adds a separate image-managed X11 session entry
with explicit compositor, supervisor, and shell paths; package-owned files are
left unchanged. The replacement artifact must demonstrate that this session
remains alive and paints the Northstar desktop.
