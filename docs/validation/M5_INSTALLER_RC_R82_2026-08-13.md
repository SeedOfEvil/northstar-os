# M5 authoritative installer RC r82 - 2026-08-13

Draft PR 87 produced the authoritative r82 Northstar installer release
candidate after the r81 disposable-install diagnostics were folded back into
clean source, package, repository, runtime, and media inputs. No live hotfix or
historical deployment directory was used by this assembly.

## Immutable source and publication

- package source commit: `58f87d9a39a5d2c0ed34a310c7a073088b8453b9`;
- exact clean RC checkout: `bd5e5bd27fe4f0c498190869a83d00da99110d21`;
- package: `northstar-0.2.2-amd64.pkg`;
- package SHA-256:
  `a43e07db01c3183f5ce58e053eff87a68f5621acd4b229501adc595df551a2b0`;
- compatibility compositor SHA-256:
  `455b16954ddf1df3c43705de43530306b2d87294c867b07053148c2e8dbe3586`;
- signed repository revision: `82`;
- authentic isolated repository refresh: pass;
- altered signed-catalogue rejection: pass;
- private key material in publication output: none; and
- exact offline runtime closure: 236 packages.

The final package includes the system-owned compatibility compositor paths,
first-boot Wayfire configuration, account-specific sudo policy, and complete
FreeBSD/Qt timezone inventory discovered during r81 installation diagnostics.

## Builder and preflight

The disposable FreeBSD 15.1 amd64 builder had four vCPUs, 8 GiB RAM, an ONLINE
ZFS pool, and 43 GiB available before assembly. Builder-only dependencies were
installed from the signed `FreeBSD-ports` repository:

- `git-2.54.0`;
- `qemu-tools-11.0.2`;
- `qemu-nox11-11.0.2`; and
- `edk2-qemu-x64-g202508_2`.

The current FreeBSD package is named `edk2-qemu-x64`, while its firmware files
remain `QEMU_UEFI_CODE-x86_64.fd` and `QEMU_UEFI_VARS-x86_64.fd`.

Preparation and assembly were separate guarded operations. Before assembly:

- the Git bundle was complete and resolved exactly to the RC checkout;
- package, compositor, and bundle hashes matched their pinned records;
- both protected builder markers and the external signing key passed checks;
- the signed repository accepted authentic metadata and rejected alteration;
- all 236 package records and the 16 GiB QCOW2 plan passed preflight;
- no memory disk, stale output, or assembler process existed; and
- no host disk was accepted as an assembler destination.

## Authoritative artifacts

- release-candidate record SHA-256:
  `675830628cea7954c58dd68bdccb65618da01af090d2a0780167275049b5f1a6`;
- QCOW2 SHA-256:
  `9b245a3da421dc85e6bb483421246471708c572476bcccec6b1687f7359a491a`;
- raw installer SHA-256:
  `e3f8148fb13c17e2897cbdea2ece9afdb83ad63449ebfb4ffa1034c05d263747`;
- QCOW2 image end offset: 3,467,706,368 bytes; and
- final assembler exit status: `0`.

The ordered assembler reported successful QCOW2, signed source, raw USB media,
and integrated release-candidate stages. `qemu-img check` reported no errors,
20.17 percent allocation, zero fragmentation, and zero compressed clusters.

## Snapshot-only UEFI boot smoke

The exact QCOW2 booted with q35, TCG, virtio storage, private user networking,
copied UEFI variable state, and `snapshot=on`. It reached ZFS root mount,
Northstar host identity, rc startup, and the FreeBSD multi-user login prompt.
The source QCOW2 was rehashed against the release-candidate record after the
smoke.

- boot-smoke record SHA-256:
  `99a8d6f36b526e4f43291ba264abceb3e5d8d35dc3202776b383c5731e9a44fd`;
- `qemu-img check` evidence SHA-256:
  `065bcaafe501fb7ab07c41e050886c3621dc82c2586bf43075ea8ddb88ad09d0`;
- `qemu-img info` evidence SHA-256:
  `4e75d004514328d428a84849af907cce3473e911cbfbeb93ec4a21d295dc9879`;
- serial log SHA-256:
  `57c9e03e5da9a627e19d60dc364646aef28cd88a40840c08e467bbca42d5d271`.

## Manual gate result: rejected

The automated r82 assembly and snapshot-only boot checks passed, but the exact
raw media failed the disposable Proxmox full-disk reinstall gate. The selected
50 GiB destination was not mounted, active swap, or part of an imported pool;
however, stale ZFS labels on its prior `freebsd-zfs` partition and whole-disk
provider caused the first authorized `gpart destroy` to fail with
`gpart: Device busy`.

Single-user diagnostics proved the bounded remediation in order: temporarily
enable rank-1 GEOM writes, clear labels only from the independently confirmed
target partition and target disk, destroy the old GPT, create a fresh GPT, and
restore `kern.geom.debugflags` to zero. PR 87 now performs that target-scoped
cleanup before partition-table mutation and covers reinstalling over an
existing ZFS disk in the native installer-executor regression.

r82 is rejected and must not be promoted or retried as release media. Its
evidence is retained rather than overwritten. Replacement media must use the
corrected 0.2.3 package and signed repository revision 83, then repeat every
automated and manual acceptance gate.
