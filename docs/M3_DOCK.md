# M3 dock and window interaction

Dock v1 is the persistent bottom shell surface for Northstar. It uses the
full available width of the display, keeps pinned shortcuts and utility
actions aligned, and exposes the currently mapped Wayfire application views.

## Current contract

- The dock surface spans the display with a small, consistent outer margin;
  it is not capped at a fixed 980-pixel panel.
- Pinned Terminal and Firefox shortcuts show a running indicator when a
  matching application view is present.
- Files and Trash remain aligned utility shortcuts.
- Open applications show an application icon, title, minimized state, and an
  active-view indicator.
- Clicking an inactive application focuses it; clicking an active or
  minimized application toggles its minimized state.
- The explicit `-`/`+` control remains available for deterministic minimize /
  restore actions.
- When Wayfire IPC is unavailable, the dock reports that window controls are
  unavailable instead of presenting fake state.
- The layout uses responsive Qt Quick Layouts so the app strip can shrink and
  scroll at smaller widths while using the available surface at 1280x800 and
  larger resolutions.

## Acceptance evidence

Native WindowController coverage now records the active-view field when
Wayfire reports `focused`, `active`, `focus`, or `is-focused`. VM acceptance
must confirm the full-width alignment, pinned shortcuts, running indicators,
focus/minimize/restore behavior, Files/Trash actions, and the no-auto-Terminal
startup contract.

The current Proxmox lane remains nested X11/pixman evidence only; this slice
does not claim direct DRM/KMS or GPU support.
