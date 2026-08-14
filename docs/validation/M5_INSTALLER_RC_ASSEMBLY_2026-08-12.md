# M5 installer RC assembly - 2026-08-12

Draft PR 87 assembled the first integrated M5 installer release candidate on a
disposable FreeBSD 15.1 amd64 builder from the immutable r80 inputs recorded in
[`M5_INSTALLER_RC_PREPARATION_2026-08-12.md`](M5_INSTALLER_RC_PREPARATION_2026-08-12.md).
The accepted project commit is `0d38a445f7efd9a2cd5e8c9dd9a8c870d3c76f53`.

## Builder boundary

- builder: disposable FreeBSD 15.1 amd64 VM, four vCPUs, 8 GiB RAM;
- image and media builder markers: present, root-owned, mode `0600`;
- external signing key: root-owned, mode `0400`, outside the checkout;
- private signing key copied into project or public artifacts: `no`;
- persistent DEV01 disk used or attached: `no`; and
- `northstar` builder sudo access after assembly: preserved.

## Corrected real-builder defect

The first integrated attempt safely stopped after QCOW2 and signed-source
assembly because FreeBSD `gpart add` prints `<provider> added`, while the media
assembler compared the complete output with the expected provider name. The
assembler now parses and validates the returned provider before comparing it.
The failed file-backed staging image was detached and removed; no physical or
VM disk was selected or changed. Unit contracts passed before the clean retry.

## Accepted artifacts

- QCOW2: `northstar-15.1-amd64.qcow2`
  - apparent size: 3.2 GiB
  - allocated size on ZFS: 2.6 GiB
  - SHA-256: `16aa8a16494ced38ef482610a7fdb96eaafc2a6de4c8870ac243b6e35f5adf12`
- raw installer: `northstar-15.1-amd64-installer-usb.img`
  - virtual size: 20 GiB
  - allocated sparse size on ZFS: 3.5 GiB
  - SHA-256: `11df9722d4f635b975226ed261f1deb17cb683fbfae9dd38ce937ed4c02afcf4`
- compressed raw-installer transfer copy:
  `northstar-15.1-amd64-installer-usb.img.zst`
  - compressed size: 2.8 GiB
  - expanded size verified by `zstd -t`: 21,474,836,480 bytes
  - SHA-256: `b5579445d523cf104ce4e0ea3b4d122476e917025585323ddac83f1942dc7874`
- rootfs payload SHA-256: `50998781bc565b12b85a3e18c8c92ed89d5a2e52afdb5db8fa1e4ff22baab64c`
- release-candidate record SHA-256: `f74de82a59c764c12acedcb2860dc81464a1a58e071218e05c83c3f96b67e163`
- image provenance SHA-256: `3b0598e6e5d6fa6dee7a9f1f3e8c5956979b74fade91c03bdaf3aa9f29f37ec3`
- media provenance SHA-256: `f174cfaa41cd0c3388485aed65fc79e9b1fb55de13dd1764097199f48b0f0c29`
- signed source manifest SHA-256: `1679afeccd3d243c89fca683e799c9c8962347658596f138bbe4faf21b091399`
- source signature SHA-256: `b2b90c71e1d582245e1f5363ebca3cb961744cc6b6cb802ac0d3b29b3fc57513`
- source public-key fingerprint: `894937b31ba0cd7c3dbff9df97c774dceb2c3c27b6607841a8444cb023e242a4`
- runtime manifest SHA-256: `2908951674adad21002431487f905d74eec1112e9386b7f6e6657e09cf31b093`

`qemu-img check` reported no errors. The raw image contains the expected EFI,
root-on-ZFS, and separate read-only installer-source partitions.

## Snapshot-only boot smoke

The accepted QCOW2 booted with UEFI firmware and a virtio disk under QEMU. It
reached all required serial milestones in 219.9 seconds:

- ZFS root mount from `nstar_0d38a445f7ef/ROOT/default`;
- `FreeBSD/amd64 (northstar-image)`; and
- the multi-user `login:` prompt.

The smoke used QEMU snapshot mode, recorded serial-log SHA-256
`47d036f12437c4f123cab44b42a5300b55de4c76a854485f5c3945d363c0284d`,
and left the source QCOW2 hash unchanged.

## Remaining promotion gate

PR 87 remains draft. Import the raw installer into a disposable Proxmox VM,
attach a separate empty target disk, and complete the noVNC install,
failure/retry, first-boot, desktop, signed update, rollback, and `/home`
preservation checklist before promotion or merge.
