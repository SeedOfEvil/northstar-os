# M5 r85 installed-image update and rollback acceptance - 2026-08-15

## Status

In progress on PR89. This is the final M5 closure gate and does not rebuild the
accepted installer media.

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
- Candidate source revision, package digest, catalogue digest, metadata digest,
  and signing fingerprint are recorded after immutable publication.
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

Pending VM execution. M5 remains open until every required item above passes.
