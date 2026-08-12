# Image assembly and validation media

M0 uses stock FreeBSD installer media as an external input for the Proxmox
development VM. The selected media and its checksums are recorded in
[`image/manifests/freebsd-15.1-amd64-installer.lock`](manifests/freebsd-15.1-amd64-installer.lock).
The ISO is downloaded into the ignored `.artifacts/` directory and is never
committed to Git.

M5 begins with a non-privileged, non-mutating input gate. The strict lock in
[`image/manifests/northstar-15.1-amd64-qcow2.lock`](manifests/northstar-15.1-amd64-qcow2.lock)
pins the official FreeBSD base and kernel release sets and the manually
accepted Northstar package/repository provenance. Validate it with:

```sh
make image-input-test
```

After staging the three named artifacts beneath an ignored directory, prepare
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
QEMU/Proxmox, followed by raw and USB images. Northstar ISO assembly is later
M5 work.

The installed-image update and rollback gate is documented in
[`docs/M5_IMAGE_UPDATE_ROLLBACK.md`](../docs/M5_IMAGE_UPDATE_ROLLBACK.md). Its
deterministic test is `make image-update-rollback-gate-test`; production phases
are destructive and belong only on a disposable VM imported from the accepted
QCOW2.

See [`docs/M0_PROXMOX.md`](../docs/M0_PROXMOX.md) for installer upload, VM
configuration, local Git transfer, and native acceptance steps.

Generated images, installer media, release sets, package repositories, and
signing material are ignored by Git.
