# M6 matrix runner: NSTAR-DEV01

Status: automated native PR91 validation passed; physical Intel execution is
pending.

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

- Corrected source revision: `c798350`.
- Immutable archive SHA-256:
  `da5d3325766bc9a5e4039a2bc0394d2efbeccbd7214c7f94eddd6b296a515f22`.
- Validation checkout: `/home/northstar/pr91-validation-c798350`.
- `sh tests/unit/test-alpha-matrix.sh`: passed supplemental VM, complete Intel,
  failed, pending, deferred, missing-preflight, lane-mismatch, malformed-record,
  privacy, and mode-0600 contracts.
- The first candidate `8c89766` exposed only a negative-test fixture plumbing
  error: its missing-shell value was not forwarded to the child process. No
  production preflight ran after that failure. Commit `c798350` explicitly
  forwards every fixture value and passed the same negative contract.
- Native `make alpha-matrix MATRIX_LANE=vm`: passed automatic preflight and
  wrote both template and matrix records at mode 0600.
- Complete `make test`: all script gates passed, the clean Qt build completed,
  and 29 of 29 CTest tests passed.

The native matrix record reported:

```text
expected_lane=vm
hardware_status=supplemental
hardware_claim=vm
hardware_graphics_lane=vm-supplemental
shell_available=yes
session_available=yes
desktop_entry_available=yes
firefox_available=yes
terminal_available=yes
diagnostics_available=yes
display_manager_available=yes
wayland_session=absent
x11_session=absent
preflight_status=pass
preflight_blockers=none
observations=absent
matrix_status=inventory-only
```

Session variables were absent because collection ran over SSH; their values
were not captured. No operator observations were fabricated. Deterministic
contracts prove a completed VM record remains `supplemental` and cannot satisfy
`--require-pass`. Physical Intel and AMD acceptance remain open.
