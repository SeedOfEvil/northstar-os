# M6 alpha readiness: NSTAR-DEV01

Status: pending native validation on PR90.

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

Pending.
