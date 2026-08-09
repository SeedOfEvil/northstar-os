# M3 Lunar desktop redesign

The Lunar slice gives Northstar a cohesive modern desktop identity while
preserving the existing FreeBSD, Qt 6, Layer Shell, and Wayfire boundaries.
It is inspired by calm contemporary desktop interfaces, but it does not copy
Apple-owned artwork, controls, or product branding.

## Visual system

`Northstar.Ui 1.0` is the reusable first-party QML module. Its
`LunarPalette.qml` is the desktop-wide source for dark/light colors, translucent
panel approximations, border hierarchy, semantic status colors, spacing, and
corner radii. The VM lane intentionally uses gradients, alpha surfaces,
borders, and restrained motion instead of real-time blur. That keeps the UI
responsive on the current Proxmox scfb/X11/pixman path and leaves native blur
as a future DRM/GPU enhancement.

The redesign includes:

- a layered blue desktop background with the official Northstar mark;
- a compact unified top bar with navigation, routed global search, status
  controls, and clock;
- a left-side system menu with grouped actions and a user identity card;
- a centered searchable application launcher with pinned actions;
- a centered icon-first dock with pinned apps, running-window state,
  focus/minimize behavior, Files, and Trash;
- glass-like Quick Settings and Notification Center panels;
- rounded, movable, resizable Files, Settings, Software Center, Welcome, and
  Text Editor surfaces with the same frame, title bar, and window controls;
- native compositor movement and bottom-right resizing through shared pointer
  handlers, with app content kept independent of the chrome implementation.

## Interaction contract

Global search launches exact system actions for Settings, Software, Terminal,
and Firefox. Matching application queries open the application launcher. A
query with no application match is handed to Files as a home-scoped search.
The existing path boundaries and application-launch validation remain in
force.

Files and Software Center expose minimize, maximize/restore, and close
controls. The dock preserves running indicators and focus/minimize behavior;
right-clicking a running app toggles its minimized state. These are shell
controls over the existing Wayfire IPC integration, not a new compositor.

The desktop background is configured to ignore top-panel and dock work-area
reservations, so it paints the complete physical output behind both shell
surfaces. The panel and dock still reserve their normal application work area;
maximized application windows therefore remain clear of those controls.

Quick Settings, the system menu, Notification Center, and the application
overview can be moved by dragging their title areas. Their positions are
clamped below the top panel and inside the active display. A moved panel keeps
its position for the current shell session, is brought back on-screen after a
resolution change, and uses its standard placement until the user moves it.
Wayland movement is delegated to the compositor through Qt's native system-
move request; coordinate assignment is retained only as the X11 fallback.
Files, Settings, Software Center, Welcome, and Text Editor use the same
frameless Northstar surface and native movement contract from their title bars.
Their minimize, maximize/restore, close, and resize affordances come from the
shared module. Standalone first-party apps read the durable shell appearance
preference at startup so light and dark presentation does not drift.
The shared title-bar `DragHandler` begins movement only after deliberate
pointer travel and can take the grab from non-interactive title content,
avoiding intermittent missed starts without interfering with a normal button
click.

## Acceptance

At 1280x800 in NSTAR-DEV01, verify that the top bar does not overlap, the
left menu remains fully visible, the application launcher is centered, and
the wallpaper continues behind the translucent dock with no black strip.
Drag each shell panel to every display edge and confirm that it remains
reachable, then close and reopen it to confirm position retention. Validate
global search routing, every dock
shortcut, running-app focus/minimize, Files/Settings/Software window controls,
light/dark appearance, text-file opening, and the existing Files mutation and
Trash workflows.

The current VM remains supplemental graphics evidence. Direct DRM/KMS,
real-time background blur, multi-display placement, and GPU animation quality
remain open for the future Intel/AMD hardware lane.

Desktop icon positions are loaded from the persistent user configuration only
after the layer-shell background receives its real output geometry. Validate
this by moving multiple icons, logging out, logging back in, and confirming
that their snapped positions survive the new shell process.
