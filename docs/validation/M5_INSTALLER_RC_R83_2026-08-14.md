# M5 rejected installer RC r83 - 2026-08-14

Draft PR 87 produced release candidate r83 after r82 was rejected by the
disposable full-disk reinstall gate. R83 is also rejected: the Proxmox install
again stopped at the first partition-table mutation with `gpart: Device busy`.

## Immutable inputs

- package source commit: `b728271af4112c8b484c7b414cb52b629651923f`;
- exact clean assembly commit: `27fcc6e0291c89373ec33faec62fe589572d59a4`;
- package: `northstar-0.2.3-amd64.pkg`;
- package SHA-256:
  `c20d2b5eb27d6c9a7ddb3dfa0f216340f3f12ad7c0caf94e5a81f6639e10bffe`;
- signed repository revision: `83`;
- catalogue SHA-256:
  `6302019bafbb03ac15a7e48b4945fe099af10d930d1c86fd13a4bee08276bdf1`;
- repository metadata SHA-256:
  `f868f0920184a2d4e60a194f1cfac5ecb6b6ab1860991a9cfec83e1ad7c35f66`;
- signing fingerprint:
  `64def739dd467ced96594d4c52d53f3c39381d8b233fde82537ddc12fbff6c42`;
- authentic repository refresh: pass;
- altered signed-catalogue rejection: pass;
- private key material in publication or public export: none; and
- exact offline runtime closure: 236 packages.

The package source passed all 29 native FreeBSD Qt tests and the guarded
installer-executor regression. The image and reused runtime closure are bound
to the same deterministic epoch, `1786587951`.

## Assembly and automated acceptance

The privileged preparation verified the package, release sets, runtime
closure, signed repository, source bundle, clean checkout, external signing
key, builder markers, available space, and absence of stale protected outputs.
Assembly then completed with exit status zero.

- release-candidate record SHA-256:
  `9d7c84df215f5f739b9306950a8dc89930b204384de968717eed4222c6251ea0`;
- QCOW2 SHA-256:
  `6ac1860e8b52f77625e214d806beba4472f4c627c0631561827d7a7f4fe324de`;
- QCOW2 file size: 3,330,342,912 bytes;
- raw installer SHA-256:
  `9aa522d7d4b19ad57c7116b95825350052c37e26f5d178c7b25b135390de546a`;
- raw installer size: 21,474,836,480 bytes;
- rootfs payload SHA-256:
  `ce0518a0783e71ef3afc9a61a2e0c1eb775dc6c7354379872c78fd69a6c33d75`;
- runtime manifest SHA-256:
  `9c975fbe1324e6eb8d65e4c29be8bdac4521c23f6ac1ad61f7df19763752a756`;
- installer source manifest SHA-256:
  `ff30052b6fafac16d289535806221b53acddc1fc9328e24a91d63ebb849c9c49`;
- installer source signature SHA-256:
  `fe493b6975939363ac70768b2323994e349e06c5c1a1adb35125268483365c2d`.

`qemu-img check` found no errors: 19.38 percent allocated, zero
fragmentation, and zero compressed clusters. Snapshot-only q35/UEFI boot smoke
reached ZFS multi-user boot without mutating the source image.

- boot-smoke record SHA-256:
  `f5eab93b8de324f897292c08032eb952f12e8cac43f013bb0a66562f2069168a`;
- `qemu-img check` evidence SHA-256:
  `08f042dde73d626cf5b8d1c19d98b8708236c8c6041da243afcca5a0fcf027ae`;
- `qemu-img info` evidence SHA-256:
  `60918f40af00f55b7196582d144bcd267c944aa73fd08e92f4838016d90c754c`;
- serial log SHA-256:
  `1a92e7670603b3ebafad541f28a41124ba54d11d044d5e5565e4944ddd6a8065`.

## Quarantined local archive and rejection

The immutable public export was copied to
`.artifacts/accepted/m5-installer-rc/r83`. All 14 files named by its inventory
were independently rehashed on Windows.

- compressed installer SHA-256:
  `40e2ead8060eccd26b159a2bacc19885aceb8f0ba37b9c3db3b9ad437b63010a`;
- export inventory SHA-256:
  `60ca62d35b75634c0235d3a7edd455f292d401872c6fe74011439abc6961482c`;
- local inventory verification: 14 of 14 files passed.

Automated assembly and boot smoke passed, but full-disk acceptance failed.
This directory is therefore retained only as immutable rejected evidence and
must not be described or promoted as an accepted installer.

## Root cause

The reused 236-package runtime bundle contained `northstar-0.2.2`, while the
resolved primary artifact and signed repository pinned `northstar-0.2.3`.
The QCOW2 assembler verified the primary package but never installed it; it
installed the stale runtime-bundle package instead. Consequently, r83's rootfs
did not receive the executor correction from `0.2.3` even though provenance
claimed that package as a verified primary input.

The diagnosis was reproduced directly from immutable records:

- `runtime-package-records` names `northstar-0.2.2-amd64.pkg`;
- the resolved lock names `northstar-0.2.3-amd64.pkg`; and
- the assembler's only `pkg add` input was built from the runtime records.

A separate root-only FreeBSD diagnostic created a new 2 GiB file-backed `md`
disk, built and exported a real ZFS pool on its partition, enabled bounded
GEOM Rank-1 writes, cleared the partition label, destroyed the GPT, and created
a replacement GPT. That test passed, confirming that the reset algorithm works
on real FreeBSD storage and isolating the failure to assembled artifact content.
The resulting committed gate, `make installer-zfs-reset-smoke`, subsequently
passed on the builder with:

```text
PASS: real FreeBSD ZFS target reset and GPT replacement succeeded on md0
```

The replacement assembler contract must install the locked primary Northstar
package in place of any runtime-bundle copy, verify its exact installed version,
and compare the installed executor digest with the executor in that package.
No replacement RC may be assembled until the real `md` ZFS-reset smoke also
passes as a committed project gate.
