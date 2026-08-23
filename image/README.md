# Image assembly and validation media

M0 uses stock FreeBSD installer media as an external input for the Proxmox
development VM. The selected media and its checksums are recorded in
[`image/manifests/freebsd-15.1-amd64-installer.lock`](manifests/freebsd-15.1-amd64-installer.lock).
The ISO is downloaded into the ignored `.artifacts/` directory and is never
committed to Git.

M5 begins with a non-privileged, non-mutating input gate. The strict lock in
[`image/manifests/northstar-15.1-amd64-qcow2.lock`](manifests/northstar-15.1-amd64-qcow2.lock)
pins the official FreeBSD base and kernel release sets, the physically accepted
FreeBSD 15.1 RFCOMM listener correction, and the manually accepted Northstar
package/repository provenance. The RFCOMM artifact is bound to its exact
FreeBSD source revision, patched digest, size, and stock-module rollback
digest. Validate the complete lock with:

```sh
make image-input-test
```

After staging the four named artifacts beneath an ignored directory, prepare
an immutable resolved-input record with:

```sh
make prepare-image-inputs \
  IMAGE_ARTIFACTS=.artifacts/m5-inputs \
  IMAGE_INPUT_OUTPUT=.artifacts/m5-resolved-inputs
```

This stage verifies names, sizes, SHA-256 digests, source revision, repository
revision, catalogue/metadata provenance, and a clean project checkout whose
actual Git `HEAD` equals the recorded project commit. It
does not download, extract, mount, partition, invoke ZFS, or create a QCOW2
file. Root-owned disk assembly begins only after this gate is accepted.

The first Northstar artifact target remains a reproducible ZFS QCOW2 image for
QEMU/Proxmox. A future milestone build also emits its matching
`northstar-rootfs-v1-<commit>.txz` before any live-media privilege is added.
`prepare-installer-source.sh` binds that payload and runtime manifest to an
external signing key, while `assemble-installer-usb.sh` converts the accepted
QCOW2 into a raw USB image and adds a read-only signed-source partition plus
installer-only session state. The private key is never copied into either
output. Northstar ISO assembly remains later M5 work.

Full artifacts are assembled and manually imported at named milestone
checkpoints, not for every image-related PR. Intermediate first-boot,
installer, recovery, diagnostics, and media changes use routine DEV01 and
non-destructive/file-backed tests and record image acceptance as deferred. The
next integrated artifact checkpoint is **M5 Installer Release Candidate**.
See
[`docs/MILESTONE_IMAGE_VALIDATION.md`](../docs/MILESTONE_IMAGE_VALIDATION.md).

The installed-image update and rollback gate is documented in
[`docs/M5_IMAGE_UPDATE_ROLLBACK.md`](../docs/M5_IMAGE_UPDATE_ROLLBACK.md). Its
deterministic test is `make image-update-rollback-gate-test`; production phases
are destructive and belong only on a disposable VM imported from the accepted
QCOW2.

See [`docs/M0_PROXMOX.md`](../docs/M0_PROXMOX.md) for installer upload, VM
configuration, local Git transfer, and native acceptance steps.

Generated images, installer media, release sets, package repositories, and
signing material are ignored by Git.

The raw USB assembler never accepts a destination device. It creates a new
file only; writing that artifact to removable hardware remains an explicit
operator action outside the builder. See
[`docs/M5_INSTALLER_MEDIA.md`](../docs/M5_INSTALLER_MEDIA.md).

The milestone-only `assemble-installer-rc.sh` composes the production QCOW2,
signed source, and raw-media stages and emits one top-level provenance record.
Its disposable-builder and Proxmox acceptance workflow is documented in
[`docs/M5_INSTALLER_RELEASE_CANDIDATE.md`](../docs/M5_INSTALLER_RELEASE_CANDIDATE.md).
