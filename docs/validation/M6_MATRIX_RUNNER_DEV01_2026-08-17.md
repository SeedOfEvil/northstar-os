# M6 matrix runner: NSTAR-DEV01

Status: pending native PR91 validation.

## Scope

- Validate deterministic VM, Intel, mismatch, failed-observation, malformed
  record, privacy, and file-mode contracts.
- Run the exact immutable PR91 checkout on NSTAR-DEV01.
- Prepare a VM observation template and automatic preflight record.
- Confirm the VM result cannot become a physical matrix pass.
- Run the complete FreeBSD project gate.

## Expected result

DEV01 uses the Proxmox scfb/pixman fallback without guest DRM card/render
nodes. Its matrix status must remain `inventory-only` before observations and
`supplemental` after completed applicable observations. Intel hardware
acceptance remains pending.

## Evidence

Pending.
