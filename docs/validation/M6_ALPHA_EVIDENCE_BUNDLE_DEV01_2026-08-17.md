# M6 alpha evidence bundle: NSTAR-DEV01

Status: automated native PR93 validation passed; DEV01 remains supplemental.

## Scope

- Validate one atomic, privacy-bounded evidence bundle that joins the M6
  readiness, platform, and matrix records.
- Prove cross-record lane, status, and digest agreement.
- Reject tampering, unknown files, duplicate summary fields, existing output
  replacement, and VM physical-pass claims.
- Publish the physical Intel/AMD operator sequence without claiming hardware
  admission from the Proxmox VM.

## Immutable input

- Candidate commit: `66ad128` (`Add M6 alpha evidence bundle workflow`).
- Archive: `northstar-pr93-66ad128.tar`.
- SHA-256:
  `68eb9e86651aa505b703f8eb45b6db56b40f5bafe2d159e40d69a4259c2b8651`.
- DEV01 checkout: `/home/northstar/northstar-pr93-66ad128`.
- The archive hash was verified on FreeBSD before extraction.

## Contract results

- `sh tests/unit/test-alpha-evidence-bundle.sh`: passed atomic collection,
  integrity, alignment, privacy, permissions, physical pass, and VM
  supplemental boundaries. Its expected negative cases rejected a changed
  record, an unknown file, a duplicated summary field, an existing output,
  and VM `--require-pass`.
- PR90-PR92 readiness, matrix, and platform contract tests passed unchanged.
- Complete `make test`: passed all shell, installer, recovery, update, image,
  build, and integration contracts.
- Cold native build: 363 of 363 steps completed.
- CTest: 29 of 29 passed.
- `git diff --check`: clean before the candidate commit.

## Native supplemental bundle

DEV01 produced `/tmp/northstar-pr93-native-66ad128` with directory mode `0700`
and exactly five mode-`0600` files. Verification reported:

```text
lane=vm
readiness_status=supplemental
readiness_claim=vm
platform_status=inventory-only
matrix_status=inventory-only
bundle_status=supplemental
```

The source record digests were:

```text
alpha-readiness.conf  a1904fdd77612753d42abf19fd1a7bdbc5e492c2a161a6aee4ccaa5428afb1f3
platform-evidence.conf  59756c28f58ea2f62e56aa9ed51fb644625282056577b0124653d821c0679301
alpha-matrix.conf  76a3e3637d4f9bc6793f29fd8f336ce07004bf5c9d7804aa7c1981246c98354b
bundle.conf  ca960e4b09b50d15da6a98e67b63816b2a4c5f4630f36e5d39cc987c4104744e
```

Ordinary verification passed. Verification with `BUNDLE_REQUIRE_PASS=1`
failed as required.

## Interpretation

PR93 closes the evidence packaging and operator-handoff slice, not physical
hardware admission. The established VM proves that collection and verification
work on native FreeBSD while remaining supplemental. PR94 must collect and
review a passing Intel bundle on direct or passed-through Intel DRM hardware;
PR95 must repeat the same boundary for AMD.
