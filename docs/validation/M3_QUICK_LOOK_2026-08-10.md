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
- The same gates will be repeated from the immutable commit archive before
  deployment.

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

This record remains open until the noVNC checklist is accepted.

## Deferred hardware evidence

This validates the supplemental Proxmox scfb/pixman lane only. Direct DRM/KMS,
GPU rendering and animation quality, multi-display behavior, and physical
Intel/AMD hardware acceptance remain separate gates.
