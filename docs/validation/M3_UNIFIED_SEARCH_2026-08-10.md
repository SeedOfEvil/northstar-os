# M3 unified search validation — 2026-08-10

## Scope

This record covers the keyboard-first Northstar Search slice on
`codex/m3-unified-search`. The slice adds categorized system actions,
validated applications, and bounded Home-scoped file and folder results.

## Automated evidence

- A clean FreeBSD build completed successfully in the separate
  `/home/northstar/validation/unified-search-pr70/build-final` tree on
  NSTAR-DEV01: 248 of 248 build steps completed.
- The focused `northstar-searchcontroller` test passed.
- The complete Qt/offscreen gate passed: 20 of 20 CTest targets.
- The QML surface contract passed after correcting its stale controller-call
  assertion.
- `git diff --check` passed in the Windows source checkout.
- The accepted binaries were installed into `/home/northstar/.local`.

## Manual 1280x800 noVNC acceptance pending

The installed build is ready for focused interactive validation in the
Proxmox scfb/pixman lane:

- click the top search affordance and press `Ctrl+K` to open the overlay;
- verify responsive typing with representative content under Home;
- navigate with Up and Down, activate with Enter, and dismiss with Escape;
- activate Settings, Software Center, Terminal, Firefox, Files, and
  Applications results;
- launch a catalog application and open a Home-scoped file and folder;
- confirm no command execution or web-query behavior is exposed.

Because the supervised session was stopped at deployment time, the newly
installed shell will load on the next Northstar login. This record remains
open until the noVNC checklist is accepted.

## Deferred hardware evidence

This slice makes no direct DRM/KMS, GPU rendering, multi-display, or animation
quality claim. Those remain physical Intel/AMD hardware gates.
