# M5 installer RC r84 rejection and VirtIO regression - 2026-08-14

## Verdict

Release candidate r84 is rejected. Its package, repository, assembly,
checksums, QCOW2 structure, and snapshot-only UEFI boot smoke passed, but the
disposable Proxmox installation failed before GPT replacement. No r84 artifact
may be promoted as an installable Northstar release candidate.

## Confirmed failure chain

The first protected transaction stopped at `mutation-started` with
`gpart: Device busy`. Console diagnostics established all of the following:

- the running installer pool was on `/dev/da1p2`;
- the selected destination was `/dev/da0`;
- `/dev/da0` was not imported as ZFS, swap, or directly mounted;
- `geom label status` mapped `msdosfs/NSTAR_EFI` to `da0p1`; and
- `/boot/efi` was mounted from `/dev/msdosfs/NSTAR_EFI`.

The installer media had inherited the installed image's `/boot/efi` fstab
entry. Because installer and destination shared the `NSTAR_EFI` FAT label, the
alias resolved to the destination. Direct-device usage matching missed that
alias and `gpart destroy -F da0` correctly refused the mounted target.

After `/boot/efi` was manually unmounted, a clean retry stopped because the
previous interrupted attempt had already cleared the destination ZFS label.
The executor treated `zpool labelclear` failure on an already-absent label as a
new failure instead of a valid idempotent retry state.

## Corrective boundaries

- Installer-media assembly removes the inherited `/boot/efi` fstab entry and
  fails if it cannot prove the entry is absent.
- Disk discovery and privileged revalidation resolve active GEOM label aliases
  to their backing providers.
- ZFS labels are detected with `zdb -l`, strictly cleared and rechecked when
  present, and skipped when already absent.
- Unit contracts cover alias-mounted targets, successful strict label clearing,
  and an already-cleared retry.

## Real VirtIO evidence

NSTAR-DEV01 received a dedicated 2 GiB QEMU VirtIO-SCSI scratch disk. Before
mutation it was independently bound as `da1`, exactly `2147483648` bytes,
`QEMU QEMU HARDDISK`, `r0w0e0`, unpartitioned, unmounted, outside every pool,
and not swap. The guarded regression then:

1. created a GPT, `NSTAR_EFI` FAT partition, and ZFS partition on `da1`;
2. mounted the EFI filesystem through `/dev/msdosfs/NSTAR_EFI`;
3. proved discovery marked `da1` ineligible through alias resolution;
4. cleared the ZFS label and reproduced `gpart: Device busy` while the alias
   remained mounted;
5. unmounted the alias and proved the absent-label retry could replace GPT; and
6. restored `kern.geom.debugflags` and removed the scratch GPT.

Result:

```text
PASS: real VirtIO alias-mounted failure and already-cleared clean retry succeeded on da1
```

Post-test evidence showed `da1` back at `r0w0e0` with no GPT or `NSTAR_EFI`
alias, while DEV01's `zroot` remained online on `/dev/da0p3` with no errors.

This closes the two reproduced storage regressions only. A new installer image
must still pass a fresh disposable Proxmox installation before M5 acceptance.
