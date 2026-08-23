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
- Receiving runs as the signed-in desktop user and is rooted in that user's
  Downloads directory. It does not run through PolicyKit or the root Bluetooth
  configuration broker.
- Sending passes a validated paired address and a readable, non-symlink local
  file to `obexapp` through `QProcess` argument boundaries. No shell command is
  constructed.
- Pairing keys, passwords, and file contents are not copied into privileged
  request files or diagnostic output.

## Automated gates

The focused controller test uses an isolated fake OBEX executable to prove the
server lifecycle and the exact OPUSH client argument shape. Existing Bluetooth
pairing, scanning, configuration, and QML tests remain required. The image
assembler test requires both the `obexapp` runtime root and `sdpd_enable=YES`.

## Physical merge gate

Before merge on the accepted Intel laptop:

1. the existing Android bond still appears as Paired;
2. Receive Files starts and stops without administrator authorization;
3. Android can share a small non-sensitive file into the user's Downloads
   directory, with its name and content preserved;
4. Send File can push a small non-sensitive file to the paired Android device;
5. pairing and file-transfer state remain sane after reboot.

This document records the gate; it does not claim acceptance until those steps
are completed on physical hardware.
