# M6 hardware matrix runner

PR91 supplies one repeatable evidence interface for the VM, Intel, and AMD M6
lanes. It does not automate destructive or user-visible acceptance actions.
The operator performs those actions, records fixed outcomes, and the runner
validates the record against the automatic readiness and application preflight.

## Prepare a lane

```sh
make alpha-matrix \
  MATRIX_LANE=intel \
  MATRIX_TEMPLATE=/tmp/northstar-intel-observations.conf \
  MATRIX_OUTPUT=/tmp/northstar-intel-matrix.conf
```

The automatic preflight requires the expected readiness claim and checks for
the installed Northstar shell, supervisor, Wayland desktop entry, Firefox,
terminal, diagnostics collector, and running display manager. It records only
yes/no presence and session-variable presence, never executable paths,
arguments, environment values, hardware identifiers, or user paths.

## Record observations

The schema contains exactly these outcomes:

- graphical login;
- direct compositor operation;
- connected display output;
- native Qt application;
- Xwayland application;
- Firefox;
- Files;
- Settings;
- networking;
- audio;
- input;
- shell crash recovery;
- update and rollback;
- clean shutdown.

Each value is `pass`, `fail`, `pending`, or `deferred`. Free-form notes are not
accepted in the machine record; use the hardware entry document for reviewed
details and limitations.

```sh
make alpha-matrix \
  MATRIX_LANE=intel \
  MATRIX_OBSERVATIONS=/tmp/northstar-intel-observations.conf \
  MATRIX_OUTPUT=/tmp/northstar-intel-matrix.conf \
  MATRIX_REQUIRE_PASS=1
```

A physical pass requires matching ready Intel/AMD inventory, successful
preflight, and fourteen `pass` observations. A failed observation yields
`fail`; pending observations yield `pending`; deferred observations yield
`partial`. A VM always yields `supplemental` after completed observations and
cannot close a physical gate.

## Safety boundary

The runner and readiness probe are read-only. They do not start applications,
kill or crash the shell, alter the package database, create boot environments,
schedule rollback, reboot, or shut down the host. Output and templates are
atomic mode-0600 files, and symbolic-link targets are rejected.
