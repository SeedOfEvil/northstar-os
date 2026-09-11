# Removable storage delivery plan

Status: metadata-only discovery implemented; physical UI acceptance pending.
Mount, write and eject controls are not implemented in this slice.

## Current evidence

The accepted Files dialog refactor is merged in PR137. Files currently discovers
mounted volumes through QStorageInfo; this is not an inventory of unmounted USB
devices. File mutation remains home-scoped. Existing mounted-volume visibility
must not be treated as authorization to write, mount or eject a device.

Read-only inspection on 2026-09-10 found the laptop's internal NVMe disk and its
root, home, temporary and EFI mounts, but no attached USB storage device. No
disk was mounted, unmounted, formatted or written during inspection. No disk
serial identifiers are recorded here.

The subsequently attached Kingston DataTraveler Duo is reported at about 58 GiB,
with two GPT Microsoft basic-data partitions labelled Main Data Partition and
UEFI:NTFS. The user identified it as recreatable Northstar installer media.
Neither partition was mounted by this work. Labels do not prove filesystem type.

## Discovery implementation

Files now exposes a Devices dialog separate from mounted Locations. Refresh reads
`kern.disks`, bounded CAM flags and GEOM XML using fixed `/sbin/sysctl` invocations
on a background worker. Only DISK providers with a matching CAM PACK_REMOVABLE
flag are listed, not all da devices. This is a removable-media classification,
not proof of USB transport, mount eligibility or absence of system use.
Names, device names and capacity are displayed as plain text. Serial identifiers
and GEOM pointer identifiers are not exposed. There is no block-device read,
mount, unmount, filesystem probe, file access or privileged command.

At most 32 CAM candidates and 2 MiB command/metadata output are accepted. Processes
have bounded waits, scans cannot overlap, and Refresh clears old results before
starting. The list is an explicit-refresh snapshot, not live hotplug state and
never authority for a future mutation. Non-FreeBSD platforms and detection errors
show explanatory status. Files' existing Home/write boundaries are unchanged.

This narrow discovery backend omits removable devices not exposed through CAM da
(such as some external fixed-media disks). Broader inventory and mutation require
the reviewed service integration described below, rather than widening flags or
using device-name guesses.

## Delivery slices

1. Read-only removable-device discovery and visible states: unavailable service,
   scanning, unmounted, mounted, unsupported filesystem, busy and failed operation.
   Test against a physically attached spare USB device before adding mutation.
2. Explicit authorized mount and safe removal through a reviewed storage service.
   Evaluate FreeBSD bsdisks/UDisks2 before implementing a custom privileged broker.
   Verify actual supported methods, authorization and internal-device exclusions.
3. Copy to/from an approved mounted USB filesystem with bounded destination access,
   progress, conflict handling and errors. Do not relax Home protections globally.
4. Disconnect/reconnect and busy-removal acceptance, then packaging/service setup
   for future images. No immediate installer rebuild is required.

## Safety requirements

- No automatic mount, format, partition, repair or destructive operation.
- No broad vfs.usermount switch or blanket PolicyKit grants.
- Never infer USB/removable status solely from a device name such as da0.
- Exclude internal/system disks and all their partitions, including EFI, swap,
  imported pools and current boot media. An external transport alone is not enough.
- Bind actions to fresh service/device identity; reject stale selections after
  unplugging or device-name reuse. Revalidate filesystem, mount and ownership.
- Do not allow arbitrary device paths, mount destinations or command-line options
  from QML. Show errors instead of falling back to more permissive behavior.
- Begin with one proven filesystem (candidate: FAT32); do not claim exFAT/NTFS
  write support until their drivers and behavior have separate acceptance.
- Refuse busy unmounts; never force-unmount. Report safe removal only after the
  relevant mounts are gone and the service's completion is verified.
- Keep active Files transfers and mount lifetime coordinated; a cached path is
  not continuing authority to write after a device disappears.

## Focused acceptance

Use a spare USB drive with no important data. First collect read-only transport,
partition/filesystem and mount evidence. Later test explicit mount, canceling
authorization, copying disposable files both ways with content verification,
conflict handling, busy refusal and safe removal/reconnection. Keep Ethernet and
the internal system untouched. Physical acceptance and explicit merge approval
remain required before claiming the feature works.

## References

- [FreeBSD storage handbook](https://docs.freebsd.org/en/books/handbook/disks/)
- [bsdisks manual](https://man.freebsd.org/cgi/man.cgi?manpath=FreeBSD+15.1-RELEASE+and+Ports&query=bsdisks&sektion=8)

The handbook warns that allowing arbitrary unprivileged media mounting is not a
safe default. Service reuse is a candidate architecture, not proof that every
UDisks2 operation is implemented or safe on this installed platform.
