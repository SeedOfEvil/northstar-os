# M6 platform capability evidence

PR92 adds one passive, privacy-bounded evidence interface for the networking,
audio, input, and suspend/resume requirements shared by the Intel and AMD M6
hardware lanes. It does not turn VM behavior into physical-hardware evidence.

## Passive inventory

Create a platform observation template and collect the automatic inventory:

```sh
make platform-evidence \
  PLATFORM_TEMPLATE=/tmp/northstar-platform-observations.conf \
  PLATFORM_OUTPUT=/tmp/northstar-platform-evidence.conf
```

The collector records only normalized capability classes, bounded counts, and
yes/no states. It omits interface names, addresses, SSIDs, device identifiers,
serial numbers, command lines, environment values, and user paths. Collection
does not transmit network traffic, play audio, alter volume, inject input,
suspend, resume, reboot, or shut down the host.

The passive checks cover:

- active wired or Wi-Fi capability, a default route, and configured DNS;
- audio device presence plus readable mixer control;
- bounded input-device presence plus keyboard and pointer capability;
- ACPI and suspend-command availability on physical hardware.

VM output is always `supplemental`. Missing physical capabilities are
`blocked`; inventory alone is never a physical pass.

## Operator observations

Perform the visible tests, then change exactly these six template values from
`pending` to `pass`, `fail`, or `deferred`:

- `network_connectivity`;
- `audio_playback`;
- `volume_control`;
- `keyboard_input`;
- `pointer_input`;
- `suspend_resume`.

Validate a physical record with:

```sh
make platform-evidence \
  PLATFORM_OBSERVATIONS=/tmp/northstar-platform-observations.conf \
  PLATFORM_OUTPUT=/tmp/northstar-platform-evidence.conf \
  PLATFORM_REQUIRE_PASS=1
```

`PLATFORM_REQUIRE_PASS=1` succeeds only for physical hardware with complete
passive capabilities and six passing operator observations.

## Matrix integration

The PR91 runner now requires the validated platform record before an Intel or
AMD matrix can pass:

```sh
make alpha-matrix \
  MATRIX_LANE=intel \
  MATRIX_OBSERVATIONS=/tmp/northstar-intel-observations.conf \
  PLATFORM_EVIDENCE=/tmp/northstar-platform-evidence.conf \
  MATRIX_OUTPUT=/tmp/northstar-intel-matrix.conf \
  MATRIX_REQUIRE_PASS=1
```

A missing, partial, blocked, failed, supplemental, or non-physical platform
record adds the `platform_evidence` preflight blocker. This prevents graphics
evidence alone from closing a physical alpha lane.
