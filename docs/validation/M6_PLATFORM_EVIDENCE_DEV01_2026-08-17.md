# M6 platform evidence: NSTAR-DEV01

Status: automated native PR92 validation passed; DEV01 remains supplemental.

## Scope

- Validate passive networking, audio, input, ACPI, and suspend-command
  inventory on the established Proxmox/scfb VM.
- Validate the strict six-field operator-observation contract without
  performing the observed actions.
- Prove that a physical Intel or AMD matrix cannot pass without a complete,
  validated physical platform record.
- Keep the VM record supplemental and avoid traffic generation, playback,
  volume mutation, input injection, suspend, reboot, shutdown, or package
  mutation.

## Immutable input

- Candidate commit: `a9bb08d` (`Add M6 platform capability evidence`).
- Archive: `northstar-pr92-a9bb08d.tar`.
- SHA-256:
  `bd5fc184dc989de5d534657c6883386b97c104d43519f425ccd9943eba861914`.
- DEV01 checkout: `/home/northstar/pr92-validation-a9bb08d`.
- The archive hash was verified on FreeBSD before extraction.

## Contract results

- `sh tests/unit/test-platform-evidence.sh`: passed passive capability,
  physical/VM classification, strict observations, privacy, and require-pass
  boundaries.
- `sh tests/unit/test-alpha-matrix.sh`: passed VM, Intel, mismatch,
  malformed-observation, missing-platform, and partial-platform boundaries.
- `sh tests/unit/test-m0-scripts.sh`: passed the diagnostics integration and
  confirmed that passive platform evidence contains no session secret.
- Complete `make test`: passed all shell, installer, recovery, update, image,
  build, and integration contracts.
- CTest: 29 of 29 passed.
- `git diff --check`: clean before the candidate commit and after evidence.

## Native passive record

The DEV01 collector reported:

```text
platform_class=virtual-machine
wired_device_count=1
wired_active_count=1
wifi_device_count=0
wifi_active_count=0
default_route=yes
dns_configured=yes
audio_device_count=0
mixer_available=yes
mixer_readable=no
input_device_count=6
keyboard_available=yes
pointer_available=yes
acpi_available=yes
suspend_command_available=yes
capability_status=supplemental
capability_blockers=audio_device,mixer_access
observations=absent
platform_status=inventory-only
```

The generated platform record, observation template, and integrated matrix
record were all mode `0600`. The VM matrix accepted the record structurally,
retained `preflight_status=pass`, and remained `matrix_status=inventory-only`.

## Interpretation

DEV01 proves the collector and integration contract on native FreeBSD, but it
does not close physical networking, audio, input, or suspend/resume acceptance.
Its active wired route, DNS, and input capabilities are useful supplemental
evidence. The missing audio device and unreadable mixer are expected for the
current Proxmox virtual hardware. PR93 and PR94 must use complete physical
platform records before Intel or AMD matrix admission can pass.
