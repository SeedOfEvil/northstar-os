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

That corrected entry authenticated and invoked the packaged runtime, but the
nested compositor then reported `Authorization required, but no authorization
protocol specified`. SDDM 0.21 had created its protected `/tmp/xauth_*` cookie,
while the nested X11 client had no usable authorization path. The image-owned
launcher now accepts an inherited usable authority or resolves exactly one
regular, mode-0600 SDDM authority file owned by the session user and containing
a cookie for the active display. It records only the selected path for
diagnostics and never copies or logs the cookie itself.

The replacement candidate at project commit
`4893a77e47a18a4b126a3638c2192dc51dcdf317` then passed the bounded
snapshot-only UEFI/ZFS boot smoke. A separate snapshot-backed VNC boot selected
`/tmp/xauth_LhQLEk`, reached `state=running` on `wayland-1`, and kept the
supervisor, compatibility compositor, and Northstar shell alive with
`restart_count=0`. A 1280x800 framebuffer capture showed the painted Northstar
desktop, top bar, wallpaper, empty-desktop state, and dock. The expected
pixman-lane warnings about unavailable GLES2 effects and software Xwayland
glamor remain supplemental-lane limitations rather than session failures.

The first Proxmox import of that candidate painted and remained alive but had
no usable keyboard or pointer input. Comparison with the accepted M0 evidence
found that the image runtime roots contained the `libinput` library but omitted
the separate `xf86-input-libinput` Xorg driver that had made the QEMU USB
Tablet interactive on NSTAR-DEV01. The driver is now an explicit immutable
runtime root; the replacement image must repeat the visual gate and prove both
pointer and keyboard interaction before PR76 promotion.

The corrected candidate at project commit
`41d1b8ba8f8b0be75b71f1ff8b0eee2c7b795598` captured 236 exact runtime
packages and assembled successfully after obsolete builder outputs were
removed. In a snapshot boot, `xf86-input-libinput-1.5.0` was installed and
Xorg loaded `libinput_drv.so` for the system keyboard multiplexer, AT keyboard,
system mouse, and IntelliMouse device. The session reached `state=running` on
`wayland-1` with zero restarts. An injected `Ctrl+Alt+T` traveled through the
emulated keyboard, Xorg, nested compositor, and Northstar shortcut handler and
launched QTerminal, providing an end-to-end keyboard-input check. Final
Proxmox/noVNC acceptance still requires the operator to confirm pointer motion,
clicking, and ordinary text entry on the imported artifact.

Manual Proxmox acceptance completed on VM 104 at `192.168.1.156`. The exact
export with SHA-256
`86ecf0a28993686ed59a425cc4dd0ef2d1e12e97e1e779d23883fff43f14c6ee`
autologged into the branded desktop. The operator confirmed pointer movement,
clicking, ordinary keyboard input, terminal and Files interaction, and clean
shutdown through noVNC. This passes PR76's focused graphical QCOW2 gate. It
does not by itself close the remaining installer, production first-boot,
image-local update/rollback, direct DRM/KMS, or alpha hardware gates.

After acceptance, the operator archive retained the QCOW2, checksum, image and
boot-smoke provenance, exact FreeBSD release sets, Northstar and compatibility
packages, resolved-input records, and the complete 236-package offline runtime
bundle. Every retained package and release artifact was rehashed against its
manifest after transfer. The disposable builder is therefore no longer the
sole copy of the inputs required to reproduce this accepted development image.
