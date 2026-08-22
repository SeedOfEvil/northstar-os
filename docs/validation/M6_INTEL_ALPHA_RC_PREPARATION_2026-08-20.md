# M6 Intel Alpha RC preparation — 2026-08-20

Status: **IN PROGRESS — not accepted installer media**

This checkpoint prepares a current Northstar installer for the first physical
Intel hardware candidate. It reuses the accepted PR #88/r85 installer pipeline
and makes every r82-r84 rejection a required pre-assembly or acceptance gate.

## Package candidate

- Package source revision:
  `61b9b00618cf0f0221f3c5e4b82beb7cae0ca4ce`
- Package: `northstar-0.2.7-amd64.pkg`
- Package SHA-256:
  `0208758979f1c7ea33b1711b249a82ffb31093538a1fc1612ff4a2bf289b6dc1`
- Package size: `22037016` bytes
- ABI and origin: `FreeBSD:15:amd64`, `x11/northstar`

The package was built from a clean no-remote checkout on NSTAR-DEV01 using
FreeBSD 15.1-RELEASE-p2, Qt 6.11.1, and Wayfire 0.10.1. The native build
completed 423/423 steps. After applying the same explicit `0.2.7` package
version used by the earlier candidate, the canonical offscreen CTest run
passed 38/38 suites in 18.96 seconds; QML surface, session supervisor, and
installed session-entry gates also passed. An initial package produced with
the source default `0.1.0` was rejected before staging and is not an RC input.

The package inventory includes the guarded installer executor, the PR #115
third-party package transaction helper, PolicyKit action, fixed source
configuration, the native Wayfire template, and the corrected First Boot
provision/session scripts.

The recaptured runtime bundle contains 240 exact packages and has record digest
`2ac63720f3d1be3cfafb088b256ff4691b5e505935c403d4d701a59fa3df5fea`.
It adds only the verified Intel Alpha roots: `drm-66-kmod` at
`6.6.25.1501000_8`, Kaby Lake firmware at `20250109.1501000`, AC 9560 firmware
at `20260410`, and `xrandr` at `1.5.4`. The capture host did not load `i915kms`.

## Signed development repository r87

- Repository revision: `87`
- Source lock SHA-256:
  `aae94ffd331772523b2b55834bf87aeea09852d5f2505d6ae7a92da00d4008f2`
- Catalogue SHA-256:
  `9c3a1f6ccee515e3b45475ab7c3292a710be1a1443bf2474eb5f287efd87ae19`
- Metadata SHA-256:
  `2f5ede1d1fa7c46a450e5365b458d69a2a9c94849e3ff0c1786c1df086819dc9`
- Signature fingerprint:
  `aa2240078b86af8ff1fee426942e9ec16c251b6ff61b9a80d1dfba125a7b8325`
- Ports branch and commit: `2026Q3`,
  `e2df2f3b1ae51e64ea850e458fd33eb0d7c292ba`

Publication ran on the disposable builder through an external root-only key
and fixed signer wrappers. The repository output contains no private key
material. An isolated exact-r87 client accepted the authentic repository and
exposed Northstar 0.2.7; the same client rejected altered signed catalogue
data.

## Disposable builder

`NSTAR-BLD01` is a new FreeBSD 15.1-RELEASE amd64, UEFI, root-on-ZFS VM with
8 vCPUs, 16 GiB RAM, and a 60 GiB disk. It reported 55 GiB free before builder
dependencies and 54 GiB after installing Git, QEMU tools, EDK2 x64 firmware,
and zstd. The prior successful builder had 43 GiB free, so this is a bounded
but viable assembly lane. It must not retain redundant failed outputs.

SSH host identity was verified out-of-band against the Proxmox console before
the key was accepted. The temporary passwordless sudo rule and all signing
material belong only to this disposable builder and must be destroyed with it.

## Current-source installer contracts

Against the exact package source, all non-mutating gates passed:

- installer disk discovery;
- signed installer-source verification and tamper rejection;
- protected installer-engine staging;
- guarded execution and clean retry;
- recovery diagnostics;
- disk-device-free USB-media assembly contract;
- integrated RC orchestration;
- immutable image-input validation;
- exact runtime-bundle validation;
- QCOW2 assembler preflight; and
- snapshot-only boot-smoke contract.

## Required before promotion

This record does not yet claim that an r87 raw installer exists or boots. The
candidate must still pass the real file-backed ZFS/GPT reset gate, the real
VirtIO alias-mounted interruption/retry regression, resolved-input preparation,
authoritative RC assembly, `qemu-img check`, snapshot-only UEFI boot, complete
disposable Proxmox installation, First Boot, graphical login, and physical
Intel laptop acceptance. Any failed output is quarantined by commit and digest
and is never promoted or silently overwritten.
