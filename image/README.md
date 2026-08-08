# Image assembly and validation media

M0 uses stock FreeBSD installer media as an external input for the Proxmox
development VM. The selected media and its checksums are recorded in
[`image/manifests/freebsd-15.1-amd64-installer.lock`](manifests/freebsd-15.1-amd64-installer.lock).
The ISO is downloaded into the ignored `.artifacts/` directory and is never
committed to Git.

The Northstar image work remains deferred until package and session behavior
is stable. The first Northstar artifact target is a reproducible ZFS QCOW2
image for QEMU/Proxmox, followed by raw and USB images. Northstar ISO assembly
is later M5 work.

See [`docs/M0_PROXMOX.md`](../docs/M0_PROXMOX.md) for installer upload, VM
configuration, local Git transfer, and native acceptance steps.

Generated images, installer media, release sets, package repositories, and
signing material are ignored by Git.
