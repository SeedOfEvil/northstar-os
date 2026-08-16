# M6 alpha readiness: NSTAR-DEV01

Status: automated native validation passed on PR90; physical matrix evidence
remains pending.

## Scope

- Run deterministic alpha-readiness classification contracts from an immutable
  branch archive.
- Collect the privacy-bounded native capability record on NSTAR-DEV01.
- Confirm the Proxmox scfb/pixman lane remains supplemental and does not claim
  Intel or AMD direct DRM/KMS.
- Confirm diagnostics includes `alpha-readiness.conf` with mode 0600.

## Expected boundary

NSTAR-DEV01 is a development and interactive noVNC lane. A supplemental result
is expected unless its virtual hardware is deliberately changed to expose a
real DRM card and render node. Physical Intel and AMD evidence remains pending.

## Evidence

- Source commit: `291c2a5` (`Add M6 alpha readiness inventory`).
- Immutable archive SHA-256:
  `11a8dbec47f317eae0273eaf431a2e943850c05670d67ef1275658fff95880c4`.
- Validation checkout: `/home/northstar/pr90-validation-291c2a5`.
- `sh tests/unit/test-alpha-readiness.sh`: passed VM, Intel, AMD,
  unsupported-DRM, wrong-base, incomplete-lane, privacy, and mode contracts.
- `sh tests/unit/test-m0-scripts.sh`: all M0 script tests passed, including the
  diagnostics-bundle integration.
- `make test`: complete script gate, clean native build, and 29 of 29 CTest
  tests passed.
- Native diagnostics record mode: `0600`.

The native record reported:

```text
release=15.1-RELEASE-p2
architecture=amd64
boot_method=UEFI
root_filesystem=zfs
platform_class=virtual-machine
drm_card_count=0
drm_render_count=0
drm_driver=none
direct_drm_kms=no
graphics_lane=vm-supplemental
wired_interface_count=1
audio_device_count=0
input_device_count=5
matrix_claim=vm
alpha_status=supplemental
blockers=direct_drm_kms,audio
```

This closes the PR90 VM inventory gate without claiming physical graphics
acceptance. No full image assembly or installed-image mutation was required.
