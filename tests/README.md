# Tests

Tests are organized by evidence level:

- `unit/` for libraries and Qt components;
- `integration/` for session and service behavior;
- `vm/` for clean install, boot, package, update, and rollback checks;
- `screenshots/` for approved visual regression evidence.

PR 2 adds `unit/test-m0-scripts.sh` for deterministic command stubs and `vm/m0-smoke.sh` for the native Wayland/Xwayland package/session preflight and launch checks. The M1 shell seed adds CMake/CTest Qt tests for `ShellState`, `ApplicationLauncher`, the standard `.desktop` application catalog, and query filtering; the native shell build is exercised through `make test` on FreeBSD. The M2 tests cover the supervisor, its staged Wayland session entry point, and the opt-in supervised nested-session wrapper. `integration/test-shell-session.sh` checks the live unprivileged Wayland shell and its scoped restart behavior.

Tests must state the required FreeBSD release, packages, privileges, and cleanup behavior.
