# M5 installer RC r85 acceptance - 2026-08-15

## Verdict

Release candidate r85 is the first Northstar installer media to pass the
authoritative build, snapshot-only UEFI boot smoke, destructive disposable-disk
installation, independent destination boot, First Boot provisioning, and
graphical desktop login gates. It supersedes rejected revisions r82-r84.

The installed-image update, injected-failure recovery, explicit rollback, and
`/home` preservation confirmation remains the final M5 closure item.

## Immutable source and repository

- RC project revision: `d561e06519cd78aef9e2918fadd22fc3fe0ee4d1`
- Northstar package source revision: `7b176962b5635b576f145bfb2d40791a5ae6e156`
- Package: `northstar-0.2.5-amd64.pkg`
- Package SHA-256: `e5ff9873d0fdae9285b29b0a5c6e21286fbdf1ace05ca14a118c07dccb4fdd15`
- Repository revision: `85`
- Catalogue SHA-256: `f1b7b390dbcb9e7337bf53d0770076615de30653d94f51a0c5977b0715eada6a`
- Metadata SHA-256: `c28ce3933dfca33c07ec382e3bba55644aacec594dd6021b50bae09e380717ac`
- Signature fingerprint: `64def739dd467ced96594d4c52d53f3c39381d8b233fde82537ddc12fbff6c42`

The authentic repository exposed Northstar `0.2.5`, altered catalogue data was
rejected, publication contained no private key material, and the installed
executor digest matched the locked package.

## Assembly and boot smoke

The authoritative FreeBSD builder consumed 236 pinned packages and passed the
file-backed and real VirtIO ZFS/GPT reset regressions before assembly. The
resulting QCOW2 passed `qemu-img check` and reached a ZFS multi-user boot in a
snapshot-only UEFI smoke test.

- Release candidate configuration SHA-256: `60187374454c318cc6f2d77dacfb49f8334ec1c2a9e8d5a9b09592e4248e3c95`
- QCOW2 SHA-256: `8c44f5ab1e6a7f9961afa6ea7b2c4e389aa82b3a7cc15f7e6d349d24e6ed6ca9`
- Raw installer-media SHA-256: `ce7ccb3008995d4aafb6c99f6e51db48a38308fc050fe71b6d976cd53f2e667e`
- Compressed installer-media SHA-256: `d7dea1201ae42072d6adcaff70e2826181d1b56f67416e8f3a552c68a8ed3987`
- Boot-smoke serial log SHA-256: `695bb31f704720fc900c1eaf0ffa4319d3c059dd655fe6274fcd5382171a9847`

The accepted local export is retained under
`.artifacts/accepted/m5-installer-rc/r85` and was independently rehashed after
transfer.

## Disposable Proxmox installation

Validation used VM 104 (`NSTAR-TEST01`) with UEFI/q35, Secure Boot disabled,
standard VGA and tablet input, a 20 GiB installer disk, and a separate 50 GiB
VirtIO-SCSI destination. The installer:

1. distinguished media from destination;
2. accepted the exact erasure confirmation;
3. replaced the destination GPT and stale ZFS metadata without `Device busy`;
4. extracted and verified the signed payload;
5. installed EFI plus ZFS root and reported `Northstar is ready`; and
6. booted the destination after the installer disk was detached.

The First Boot wizard created administrator `seedofevil`, applied the selected
regional settings, sealed the setup identity, and returned to the branded
greeter. The image-managed `Northstar (Image Proxmox X11 fallback)` session
then reached the interactive Northstar desktop successfully.

## Session-entry follow-up

The installed image also exposed the package's source-development fallback.
Selecting that entry returned to the greeter because it resolved Wayfire below
the new user's empty `~/.local` tree. This was not an installer or desktop
runtime failure: the image-managed entry used the packaged system runtime and
passed immediately.

The release-image source now prevents recurrence by removing the development
descriptor during assembly, retaining only the image-owned wrapper, preferring
the packaged Wayfire runtime in the generic launcher, and deleting the sealed
First Boot descriptor after successful provisioning. Regression contracts
cover all three boundaries.
