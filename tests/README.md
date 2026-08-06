# Tests

Tests are organized by evidence level:

- `unit/` for libraries and Qt components;
- `integration/` for session and service behavior;
- `vm/` for clean install, boot, package, update, and rollback checks;
- `screenshots/` for approved visual regression evidence.

PR 2 adds `unit/test-m0-scripts.sh` for deterministic command stubs and `vm/m0-smoke.sh` for the native Wayland/Xwayland package/session preflight and launch checks.

Tests must state the required FreeBSD release, packages, privileges, and cleanup behavior.
