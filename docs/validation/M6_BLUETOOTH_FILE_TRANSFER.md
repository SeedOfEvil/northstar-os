# M6 Bluetooth file transfer

## Scope

This follow-up adds one Bluetooth profile slice after Secure Simple Pairing:
on-demand OBEX Push file transfer with an already paired device. Bluetooth HID
and audio remain separate work because they use different FreeBSD services and
need different physical acceptance devices.

## Runtime and trust boundary

- `obexapp` is an explicit image runtime root; its dependency closure remains
  part of the locked offline runtime bundle.
- `sdpd` starts at boot so profile servers can publish service records.
- Receiving uses a dedicated PolicyKit helper because FreeBSD permits only a
  root-EUID process to register an SDP service. The helper derives the desktop
  identity from `PKEXEC_UID`; no username or path is accepted from the GUI.
  `obexapp` then chroots to Downloads and drops to the signed-in user.
- The protected helper owns a single per-user receiver PID, advertises the
  standards-defined Laptop + Object Transfer device class while receiving, and
  restores the prior class when protected Stop terminates that exact server.
- Sending passes a validated paired address and a readable, non-symlink local
  file to `obexapp` through `QProcess` argument boundaries. No shell command is
  constructed.
- Pairing keys, passwords, and file contents are not copied into privileged
  request files or diagnostic output.

## FreeBSD 15.1 RFCOMM listener correction

Physical Android testing proved that the stock 15.1 RFCOMM client path works,
but its listener never dequeues a completed inbound L2CAP connection. The
kernel receives the RFCOMM handshake on PSM 3 and leaves it unread until the
phone times out. `solisten()` uses the dedicated listener upcall, while
`ng_btsocket_rfcomm` installs only receive/send sockbuf upcalls.

`patches/freebsd/15.1/ng-btsocket-rfcomm-listener-upcall.patch` installs the
same RFCOMM task callback through `solisten_upcall_set()` after the listener is
created. A patched `ng_btsocket.ko` must match the exact installed FreeBSD
kernel build. It is not accepted into an image merely because it compiles;
Android inbound transfer, outbound transfer, reboot loading, and rollback to
the stock module remain physical gates.

The image input lock pins the accepted module to FreeBSD source revision
`96841ea08dcf95628a104c68f31d9a7856de4da0`, kernel ABI `1501000`, SHA-256
`7e0794b0f430c77a58ce7a700c04750e217a10c3774820a2dde0d64fdc126ca8`, and
size 214184 bytes. Assembly first verifies the release kernel's stock module
digest, preserves it as `/boot/kernel/ng_btsocket.ko.northstar-stock`, installs
the accepted module mode 0444, and regenerates linker hints. Both module
digests are recorded in the installed runtime manifest and image provenance.

## Automated gates

The focused controller test uses an isolated fake OBEX executable to prove the
server lifecycle and the exact OPUSH client argument shape. Existing Bluetooth
pairing, scanning, configuration, and QML tests remain required. The image
assembler test requires both the `obexapp` runtime root and `sdpd_enable=YES`.

## Physical merge gate

Before merge on the accepted Intel laptop:

1. the existing Android bond still appears as Paired;
2. Receive Files starts and stops through the expected administrator prompt,
   and repeated starts publish only one OPUSH/FTP service pair;
3. Android can share a small non-sensitive file into the user's Downloads
   directory, with its name and content preserved;
4. Send File can push a small non-sensitive file to the paired Android device;
5. pairing and file-transfer state remain sane after reboot.

This document records the gate; it does not claim acceptance until those steps
are completed on physical hardware.

## Physical acceptance evidence (2026-08-22)

The Intel Alpha laptop passed the profile gate with the pinned patched module:

- the existing Android bond remained paired across reboot;
- Android sent `image-1787451574077.jpg` into Downloads as the desktop user
  (221480 bytes; SHA-256
  `7a29bee0db1241b591bca27cafd704b22ac831cbe53cab08f60e55ac0ba95d17`);
- Northstar sent a file to the Android phone and the phone reported success;
- Stop removed the exact receiver and both OBEX records and restored the prior
  device class;
- restarting Receive Files produced one receiver, one RFCOMM channel-30
  listener, and one OPUSH/FTP service pair; and
- reboot loaded the accepted module while retaining the exact stock rollback
  copy.

This accepts the module/profile behavior on the physical laptop. It does not
yet accept a newly assembled installer image; that remains the PR merge gate.
