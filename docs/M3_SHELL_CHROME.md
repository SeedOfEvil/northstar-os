# M3 shell chrome slice

Northstar now has a branded desktop shell surface around the existing session
and application controls:

- The top bar carries the Northstar mark, Desktop/Files/Apps navigation, a
  global application search entry point, quick-settings access, and the clock.
- Application Overview is a centered launcher card with searchable tile-style
  application entries.
- Quick Settings provides the first visual control surface for connectivity,
  focus, display, sound, and media status. Hardware-backed controls remain a
  later integration slice.
- The bottom dock is a centered capsule with pinned launchers, Files and Trash
  shortcuts, and open-window controls.
- The Appearance setting persists the dark/light preference in the user's
  Northstar configuration and restores it when the shell starts again.
- The project-owned system menu and shell shortcuts share one tested command
  catalog for the first Apps, Files, Settings, launch, refresh, and dismiss
  actions.

The visual language is intentionally Northstar-specific: rounded surfaces,
deep blue-gray backgrounds, the official Northstar mark, and restrained blue
accents. It is a directional shell design rather than a pixel-for-pixel copy
of another desktop.

## VM validation

After installing the user build and starting a fresh graphical session, verify:

1. The top bar shows the Northstar mark, navigation, search, quick settings,
   and clock without covering one another.
2. **Apps** opens the centered launcher card; selecting an app still launches
   it and the search field filters the catalog.
3. The top-right quick-settings control opens and closes its panel, and each
   preview toggle/slider responds.
4. The bottom dock remains centered at the current resolution, launches
   Terminal/Firefox/Files, opens Trash, and retains open-window controls.

The current Quick Settings toggles and sliders are visual preview controls;
their direct hardware/session integration is a later slice.

The durable Appearance setting is documented in [`docs/M3_SETTINGS.md`](M3_SETTINGS.md).
The keyboard and menu command mapping is documented in [`docs/M3_KEYBOARD.md`](M3_KEYBOARD.md).
