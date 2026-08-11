# M5 installed-image update and rollback validation - 2026-08-11

## Scope

PR77 validates update and rollback from the accepted PR76 QCOW2. The persistent
`NSTAR-DEV01` VM is limited to immutable source build, tests, package creation,
and signed candidate publication. All actual package mutation, injected
failure, boot-environment activation, and reboot evidence belongs to a fresh
disposable import of the accepted image.

## Deterministic evidence

- `tests/unit/test-image-update-rollback-gate.sh` simulates both reboot
  boundaries and passes failure recovery, successful update, explicit rollback,
  and home-sentinel preservation.
- The production driver requires root, FreeBSD, the installed-image marker,
  and a separate `/home` dataset.
- The exact source archive for commit
  `1bf935dd2bbfe3e81d77d9dc81e9cec2fed903db` had SHA-256
  `71fd055b5df417fb6e55e32bc6c77df5a2822d9e844c67ff5eeea594608f246e`.
  It completed the full native script gate and all 22 Qt/CTest targets on
  `NSTAR-DEV01` without using the canonical build tree.
- The immutable source produced `northstar-0.1.5-amd64.pkg`, origin
  `x11/northstar`, SHA-256
  `80ae5d7d337c7bd6ea90aa1876bc2a5c4158d050a01ece62e890aa4b9b9da193`.
  Its package manifest includes the installed image acceptance driver.
- Signed development repository revision 79 records catalogue SHA-256
  `2885f81f6296f1f6337ad059494de971eeef18f3fe10fb5f15bd99ceff850f20`,
  metadata SHA-256
  `8bec63b4757e77741ef62bc51709763ef272901447f86355b30c105a84189c2e`,
  and fingerprint
  `cb4ef72e20b344943e986175e7fa5ecb0957d17780b299f44f8dec981f961a24`.
  An isolated client accepted exactly Northstar `0.1.5`; the package also
  passed the standard altered-catalogue rejection smoke. Disposable private
  signing material was removed after publication.

## Disposable-image evidence

The first disposable execution exposed an image-layout defect and is retained
as failed evidence rather than promoted:

- injected package failure created and activated
  `northstar-before-development-r79-1bf935dd2bbf`; reboot restored the baseline
  and preserved the home sentinel;
- the real signed transaction upgraded `0.1.4` to `0.1.5` and explicit rollback
  activated the same verified pre-update environment;
- after reboot, `/usr/local/bin/northstar-shell` matched the accepted `0.1.4`
  SHA-256 `6f836f14774ebd76ddbdfa56685e37626a4a8121d0a383358a75ab8143065ddb`,
  proving root files rolled back;
- `pkg` still reported `0.1.5` because `/var/db/pkg/local.sqlite` lived on the
  shared `nstar_41d1b8ba8f8b/var` dataset and retained its post-update state.

PR77 therefore removes the separate `/var` dataset from image assembly and
requires `/var` to resolve to the active root boot-environment dataset before
the destructive gate may start. A rebuilt QCOW2 must repeat the complete
failure, update, rollback, and home-preservation sequence and reach
`stage=passed` before promotion.
