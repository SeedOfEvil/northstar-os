# M3 Quick Look validation — 2026-08-10

## Scope

This record covers bounded Quick Look previews from Northstar Files and the
desktop on `codex/m3-quick-look`.

## Automated evidence

- A clean FreeBSD working-tree build completed in the separate
  `/home/northstar/validation/quick-look-pr71/build-pr71` tree on
  NSTAR-DEV01: 256 of 256 build steps completed.
- The focused `northstar-previewcontroller` target passed.
- The complete Qt/offscreen gate passed: 21 of 21 CTest targets.
- The QML surface contract passed.
- Commit `9baa506` was exported as an immutable Git archive and rebuilt in
  `/home/northstar/validation/quick-look-9baa506/build`: 256 of 256 build
  steps completed, all 21 CTest targets passed, and the QML surface contract
  passed again.
- The exact archived artifact was installed into `/home/northstar/.local`.

## Manual 1280x800 noVNC acceptance pending

After deployment, validate:

- Space opens Quick Look for a selected Files item and desktop icon;
- the Files Quick Look button and desktop context-menu action open the same
  preview;
- valid UTF-8 text, raster images, folders, unsupported files, and unavailable
  targets show their explicit bounded states;
- the panel moves, resizes, maximizes/restores, closes, and remains unclipped;
- previewing leaves file contents, Files selection, and associations unchanged;
- Home items work and unrelated paths remain blocked.

This record remains open until the noVNC checklist is accepted. The supervised
session was stopped during the initial deployment, and the installed shell was
subsequently loaded by a new Northstar login.

The first interactive pass found that Quick Look could stop appearing after
its native window was minimized or hidden: reopening used `show()` without
restoring the minimized state, and closing did not return keyboard focus to
the originating Files/Desktop surface. The branch now uses `showNormal()` on
every presentation and restores origin focus after hiding. The incremental
FreeBSD build completed, all 21 CTest targets and QML contracts passed again,
and a controlled shell-only restart activated the fix as PID 34404. Repeated
open/close/minimize manual acceptance remains pending.

## Deferred hardware evidence

This validates the supplemental Proxmox scfb/pixman lane only. Direct DRM/KMS,
GPU rendering and animation quality, multi-display behavior, and physical
Intel/AMD hardware acceptance remain separate gates.
