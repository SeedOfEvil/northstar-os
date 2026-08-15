# M5 installer RC preparation - 2026-08-12

Draft PR 87 prepared the first integrated M5 installer release-candidate inputs
at project commit `fa17d59ca1c64749ce4192bc3322fd61c55cfd21`. This is preparation
evidence only; it does not claim image assembly or installation acceptance.

## Package and signed repository

- Northstar package: `northstar-0.2.0-amd64.pkg`, 7,201,612 bytes
- Package SHA-256: `e866a00df71deb5b5fafc76e2ac77b9755b69fde185c592a2c4118846f5418ea`
- Package source commit: `475ac7a48e1a7356625227239e58a5f70750372d`
- Repository revision: `80`
- Catalogue SHA-256: `d6d45339cc008ca3f6bf261951a1061527e27508cd9dd58704eb2219ad120812`
- Metadata SHA-256: `8c6c5e4ba53d834901ad4210dc04fc1025248da545baaa4e501dcdf5070cc9ba`
- Signing fingerprint: `894937b31ba0cd7c3dbff9df97c774dceb2c3c27b6607841a8444cb023e242a4`

An isolated root-owned FreeBSD package database accepted the authentic r80
repository and exposed exactly Northstar 0.2.0. The same client did not expose
Northstar after the signed catalogue payload was altered. The published tree
was checked for private-key material; only the public fingerprint was retained.

## Immutable image inputs

The exact clean Git checkout prepared resolved image inputs using the official
FreeBSD 15.1-RELEASE amd64 `base.txz` and `kernel.txz` artifacts and the r80
Northstar package. The resulting records include:

- resolved input-lock SHA-256: `4b1b04b835645762759412040ae5eb85b99d8182832dc4afe44bb71c5d02b6ce`
- artifact-records SHA-256: `51f29b91967f3b41af19341b4de9fab6542f34f21d94bcee9258f3df1f928c28`
- runtime package count: `236`
- runtime-records SHA-256: `2380d63bf543cbaadd8f6f0d75d2b8f1ce16cf08fb41d16c0c81273f9237acf3`

The retained public transfer bundle is
`.artifacts/accepted/m5-installer-rc/r80/m5-rc-r80-builder-inputs.tar`:

- size: `1,067,714,560` bytes
- SHA-256: `d307b711da3c4dd8fc7e2c611aa901ed7c8d941e6799aa4bbc335ca2890841ba`
- private signing key included: `no`

The private RC key remains external to the checkout and retained artifact
bundle pending direct transfer to the disposable builder.

## Automated gates

- integrated RC orchestration contract: pass locally and on FreeBSD;
- image-input contract: pass;
- exact signed repository acceptance and tamper rejection: pass;
- clean immutable FreeBSD build: pass; and
- Qt/CTest suite: 29 of 29 passed.

## Follow-on milestone evidence

Disposable-builder assembly, `qemu-img check`, and snapshot-only boot smoke
subsequently passed at commit `0d38a445f7efd9a2cd5e8c9dd9a8c870d3c76f53`.
The resulting hashes and serial evidence are recorded in
[`M5_INSTALLER_RC_ASSEMBLY_2026-08-12.md`](M5_INSTALLER_RC_ASSEMBLY_2026-08-12.md).
Raw-media transfer and the complete Proxmox/noVNC installation,
failure/retry, first-boot, desktop, update, rollback, and home-preservation
checklist still block promotion and merge. PR 87 remains draft until that
evidence is added.
