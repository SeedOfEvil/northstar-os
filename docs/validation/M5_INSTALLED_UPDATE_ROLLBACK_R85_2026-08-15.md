# M5 r85 installed-image update and rollback acceptance - 2026-08-15

## Status

Accepted on PR89. This final M5 closure gate used the installed r85 system
directly and did not rebuild the accepted installer media.

## Accepted baseline

- Proxmox VM: 104 (`NSTAR-TEST01`)
- Installed image: accepted Installer RC r85
- Image project commit: `d561e06519cd78aef9e2918fadd22fc3fe0ee4d1`
- Baseline package: `northstar-0.2.5-amd64.pkg`
- Baseline package SHA-256:
  `e5ff9873d0fdae9285b29b0a5c6e21286fbdf1ace05ca14a118c07dccb4fdd15`
- Installer-media SHA-256:
  `ce7ccb3008995d4aafb6c99f6e51db48a38308fc050fe71b6d976cd53f2e667e`
- `/home` is a separate ZFS dataset; `/var` belongs to the active root boot
  environment.

## Candidate contract

- Candidate package version: `0.2.6`
- Signed repository revision: `86`
- Candidate source revision:
  `448b297a44d809142db80f5e18d9e9ed3910cfe2`
- Candidate package SHA-256:
  `1ffc9a370fbc5d471a78e8d5991813c05b7b7e39af9d130dbea6bfb0f215c258`
- Catalogue SHA-256:
  `b9560540ecea01fd342e734d5422a962ec51ab1cd094f629b4e8315a8073b423`
- Publication metadata SHA-256:
  `685d8b84f4d9e77dc79d80d8b04308bb018c92b08429a066b32d5ae1e17860c8`
- Signing fingerprint:
  `2edc7ddb14ea4daf91266d836758956dc9c1164ecdb924b42968aeeca7ed28dc`
- The isolated FreeBSD package client accepted the authentic r86 catalogue,
  exposed Northstar `0.2.6`, and rejected a modified signed catalogue.
- VM staging bundle SHA-256:
  `cf6756ebea0f08fd5639b916d4796b463970614c442c149f44d4e0db8ebef995`
- `tools/stage-installed-image-update-candidate.sh` stages trust and repository
  inputs without invoking `pkg upgrade` or `bectl`.
- `image/scripts/validate-image-update-rollback.sh` records schema-2 state bound
  to the accepted image and exact signed candidate identities.

## Required execution evidence

1. Capture the pristine package, image marker, datasets, boot environments,
   repository state, and `/home` sentinel digest.
2. Inject a package failure, confirm rollback scheduling, and reboot.
3. Verify baseline package recovery and unchanged `/home`, then normalize boot
   environment names.
4. Apply the real signed `0.2.5` to `0.2.6` update and verify the candidate.
5. Schedule explicit rollback and reboot.
6. Verify package `0.2.5`, the original root boot environment, unchanged
   `/home`, and terminal `stage=passed`.
7. Preserve sanitized command and provenance evidence, retire temporary state
   only after acceptance, and run guest TRIM.

## Result

Passed on protected Proxmox VM 104 with snapshot
`pr89-r85-pre-update` retained during execution:

1. Non-mutating staging exposed the exact signed Northstar `0.2.6` candidate
   and printed `MUTATION=none`.
2. Preparation identified baseline boot environment `default` and installed
   package `0.2.5`.
3. The injected package failure activated
   `northstar-before-development-r86-448b297a44d8`; reboot recovery verified
   `0.2.5` and the preserved home sentinel, then normalized the active boot
   environment back to `default`.
4. The authentic repository upgraded Northstar from `0.2.5` to `0.2.6`; the
   transaction reported `UPDATED=yes`, `ROLLBACK_AVAILABLE=yes`, and the gate
   reported `UPDATE_VERIFIED=yes`.
5. Explicit rollback activated the recorded pre-update environment. After
   reboot the gate reported `IMAGE_UPDATE_ROLLBACK_GATE=PASS`, baseline
   `0.2.5`, candidate `0.2.6`, and `HOME_PRESERVED=yes`.
6. Terminal schema-2 state is `stage=passed`. The independently rehashed home
   sentinel remained
   `040c4eb605fc349aab1c3eb8b4932a7c84639c81aeddb7a6c9d6965806d49fa2`.

M5 update, failure recovery, explicit rollback, and home-preservation evidence
is complete.
