# Removable storage delivery plan

## Startup prerequisite acceptance (2026-09-13)

The operator confirmed Mount & Browse works after loading fusefs. Live checks
confirmed `/dev/fuse`, a read-only NTFS mount, and persisted `kld_list="i915kms fusefs"`.
This verifies the saved startup setting, not a completed reboot test.
Future image configuration includes fusefs alongside i915kms, and runtime roots
include bsdisks and fusefs-ntfs. The installer payload check expects both drivers.
The protected helper now gives specific missing-driver/package errors rather than
the generic mount-confirmation warning. No image rebuild or merge is implied.

Status: metadata-only discovery physically accepted and merged in PR138.
Protected read-only NTFS mount/unmount is under development on
`codex/m7-removable-mount-eject`; native and physical gates are tracked below.
Writes and whole-drive eject remain out of scope.

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
Neither partition was mounted during discovery. Labels do not prove filesystem type.

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

### Mount/eject preparation (2026-09-11)

After detection acceptance and PR138 merge, bsdisks 0.40 was installed by the
operator. Read-only ObjectManager inspection confirmed the Kingston drive's
USB ConnectionBus and removable flags; the main partition reports ntfs and the
small UEFI:NTFS helper reports vfat. Neither was mounted by this work.

Do not rely on HintSystem alone: this version reports false for the installed
internal NVMe Block objects as well. Future action eligibility must require
positive USB/removable evidence, reject ignored/boot/helper partitions, inspect
all siblings for system mounts and pool/swap use, and bind actions to fresh
identity. Keep boot helper partitions inaccessible through action buttons.

At the start of preparation the NTFS mount prerequisite was missing. The package dry-run proposed
only fusefs-ntfs and its three dependencies (fusefs-libs, libublio, libuuid), with
no existing-package upgrades. The fusefs kernel module was not loaded. Installing
these prerequisites is not mount/write acceptance. Begin with explicit read-only
mount testing; never repair a dirty NTFS volume or force-unmount to bypass errors.

Although the service advertises Eject and PowerOff methods, introspection is not
proof of their implementation or safe completion. Verify backend behavior and
post-operation mount/device state before displaying a safe-removal claim.

Source review of bsdisks 0.40 found a concrete blocker: `BlockFilesystem::Mount`
accepts an options map but does not consume it or capture it in its authorization
callback. Its NTFS path invokes ntfs-3g with only device and mountpoint. Passing
`ro` through this API therefore cannot establish a read-only mount. Do not expose
or invoke this path as read-only. A corrected service or narrow helper is needed.

Evidence: the FreeBSD port at e9e40e40c5d926e4d17c156665b69e8073cc863b identifies
bsdisks-0.40.tar.bz2 with SHA-256
66b23d93ee4886face3b27b8fc51dc05273d14f13924f9bd2a69dc2f23f91030. The reviewed
archive matched that digest exactly. The relevant file is blockfilesystem.cpp.
No upstream code was copied into this repository. The operator loaded fusefs;
/dev/fuse is now present. A direct driver-only test must request
`ro,norecover,nosuid,noexec`, verify the resulting mount flags and not be claimed
as acceptance of the future graphical mount/eject workflow.

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

### Read-only NTFS implementation in progress

The `codex/m7-removable-mount-eject` working tree adds a narrow PolicyKit helper
and Files controls for explicit read-only NTFS mount and non-forced unmount.
Native checks passed on 2026-09-11; physical GUI acceptance remains pending. No runtime deployment or
new installer is implied. bsdisks is used for discovery only, never its Mount
method, because the installed version ignores the supplied read-only options.

The helper requires fresh USB/removable metadata, serial/partition identity,
kernel GEOM identity and filesystem checks. It rejects protected partition
layouts and allows only fixed mount options and root-controlled destinations.
The GUI closes its dialog before authentication and reports the operation result.
Whole-drive eject, writes, formatting and repair remain unsupported.

Verified on the Intel laptop: native shell/helper build; storage-access and
volume-catalog tests; offscreen shell QML self-test. The opt-in read-only live
inventory test identifies da0p1 as eligible and rejects da0p2. No mount was
performed by these checks. Local repository and QML surface contracts also pass.

Pending gates: authorization cancellation;
read-only mount and browsing as the desktop user; verified native mount flags;
busy unmount refusal; successful unmount; stale identity refusal after replug.
The existing direct driver-only mount test does not satisfy these GUI gates.

### Browse follow-up

The first GUI mount reported a completion warning despite a later independent
SSH check confirming a read-only mount. That helper completion issue remains
open; it is not physical acceptance of the entire operation.

The Browse action now stays present for eligible partitions: unmounted media
offers Mount & Browse through the existing authorization flow, while mounted
media offers Browse. Refresh retains disabled rows rather than removing buttons
during scanning. Automatic browsing requires the same device/identity and a
ready, read-only QStorageInfo mount with the exact expected source/destination;
unverified mounts do not navigate to an empty mount-point directory.
The action row wraps on narrow windows. Physical button acceptance is pending.

The subsequent flow fix starts read-only discovery when the controller enters
the event loop and refreshes before the Devices popup is shown. Scan-in-progress
state is now owned by the UI thread until the result is published; a dedicated
scan-completed signal drives deferred browsing, not intermediate property changes.
Storage requests are dispatched by the persistent dialog after its close event,
instead of deferred callbacks owned by list delegates. Native tests cover automatic
inventory publication and overlapping scans; the offscreen QML test uses a fake
controller to verify one request after closing and one navigation only after the
verified result arrives. Actual mount completion warnings remain separately open.

- No automatic mount, format, partition, repair or destructive operation.
- No broad vfs.usermount switch or blanket PolicyKit grants.
- Never infer USB/removable status solely from a device name such as da0.
- Exclude internal/system disks and all their partitions, including EFI, swap,
  imported pools and current boot media. An external transport alone is not enough.
- Bind actions to fresh service/device identity; reject stale selections after
  unplugging or device-name reuse. Revalidate filesystem, mount and ownership.
- Do not allow arbitrary device paths, mount destinations or command-line options
  from QML. Show errors instead of falling back to more permissive behavior.
- Begin with one proven filesystem (this slice: NTFS read-only); do not claim exFAT/NTFS
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
