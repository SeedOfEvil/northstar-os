# M6 physical hardware operator runbook

PR93 defines the repeatable handoff from an installed Northstar system to one
reviewable Intel or AMD hardware evidence bundle. It does not admit hardware by
itself and does not permit VM results to satisfy a physical lane.

## Preconditions

Use a physical x86-64 UEFI machine running the supported FreeBSD release with
root on ZFS. The candidate must have direct Intel or AMD DRM card and render
nodes, wired networking, audio, keyboard, and pointer input. Record exact model
details only in the reviewed hardware-entry document; never place addresses,
serial numbers, credentials, or private network configuration in machine
records.

Start from a clean project checkout matching the installed Northstar package.
The commands below are unprivileged and create files only beneath `/tmp`.

## 1. Create fixed observation templates

For Intel, use:

```sh
make platform-evidence \
  PLATFORM_TEMPLATE=/tmp/northstar-platform-observations.conf \
  PLATFORM_OUTPUT=/tmp/northstar-platform-inventory.conf

make alpha-matrix \
  MATRIX_LANE=intel \
  MATRIX_TEMPLATE=/tmp/northstar-intel-observations.conf \
  MATRIX_OUTPUT=/tmp/northstar-intel-inventory.conf
```

Use `MATRIX_LANE=amd` and AMD filenames for an AMD candidate.

## 2. Perform the interactive checks

Complete every applicable observation as `pass`, `fail`, `pending`, or
`deferred`. Do not delete a failing result to make the bundle pass.

Platform checks:

1. Confirm wired network connectivity and DNS with an ordinary browser or
   approved network destination.
2. Play audible media through the intended output.
3. Change volume and mute state, confirming the audible result.
4. Type in Text Editor and the search overlay.
5. Confirm pointer movement, clicking, dragging, and scrolling.
6. Suspend from the physical system, resume it, and repeat input, network,
   audio, and display checks.

Matrix checks:

1. Boot to the branded greeter and log in.
2. Confirm the direct Wayland compositor and connected display output.
3. Launch a native Qt application and an Xwayland application.
4. Exercise Firefox, Files, and Settings.
5. Confirm networking, audio, and input remain functional.
6. Exercise the documented shell crash-recovery flow.
7. Complete the signed update and explicit rollback acceptance flow.
8. Shut down cleanly and boot once more.

Record reviewed hardware identity, limitations, and human-readable notes with
[`validation/M6_HARDWARE_ENTRY_TEMPLATE.md`](validation/M6_HARDWARE_ENTRY_TEMPLATE.md).

## 3. Build the evidence bundle

Intel:

```sh
make alpha-evidence-bundle \
  BUNDLE_LANE=intel \
  BUNDLE_MATRIX_OBSERVATIONS=/tmp/northstar-intel-observations.conf \
  BUNDLE_PLATFORM_OBSERVATIONS=/tmp/northstar-platform-observations.conf \
  BUNDLE_OUTPUT=/tmp/northstar-intel-evidence \
  BUNDLE_REQUIRE_PASS=1
```

The output directory is created atomically and must not already exist. It
contains exactly:

- `alpha-readiness.conf`;
- `platform-evidence.conf`;
- `alpha-matrix.conf`;
- `bundle.conf`;
- `SHA256`.

All files are mode `0600`; the directory is mode `0700`. The summary binds the
three record statuses and digests to one lane. A physical pass requires ready
matching hardware, a passing physical platform record, and a passing matrix.

## 4. Verify before review

```sh
make alpha-evidence-verify \
  BUNDLE_VERIFY=/tmp/northstar-intel-evidence \
  BUNDLE_REQUIRE_PASS=1
```

Copy the bundle through an approved secure channel, verify it again at the
review destination, and record its summary and file digests in the hardware
entry. Any altered, missing, duplicated, unknown, cross-lane, or inconsistent
record is rejected.

## Supplemental VM use

The same command may use `BUNDLE_LANE=vm` without observations for development
inventory. Its result is always `supplemental`, and `BUNDLE_REQUIRE_PASS=1`
must fail. VM evidence cannot close Intel, AMD, suspend/resume, or direct
DRM/KMS acceptance.
