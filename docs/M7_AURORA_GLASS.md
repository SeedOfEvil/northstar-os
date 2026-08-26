# M7 Aurora Glass

Aurora Glass is Northstar's modern visual system. The authoritative reference
is `assets/design/aurora/aurora-glass-reference.png`: a restrained midnight
desktop with a translucent top bar, compact floating dock, soft layered
windows, a sidebar-led Settings layout, and a compact control center.

The official `assets/branding/northstar-logo.png` remains unchanged. Aurora
uses that mark in the top bar and dock and does not add upstream operating
system branding to the desktop.

## Generated artwork

The built-in image-generation workflow produced:

- `assets/icons/northstar-icons-aurora.png`, the installed transparent 4-by-3
  application atlas;
- `assets/icons/northstar-icons-aurora-source.png`, its source render;
- `assets/icons/northstar-system-icons-aurora.png`, the complete 5-by-4
  Settings, status, Quick Settings, and utility atlas;
- `assets/icons/northstar-system-icons-aurora-source.png`, its unprocessed
  generated source for provenance;
- `assets/icons/northstar-activity-aurora.png`, an eight-frame transparent
  activity animation for operations whose progress needs to be visible;
- `assets/icons/northstar-system-icons-aurora-light.png` and
  `assets/icons/northstar-activity-aurora-light.png`, the theme-aware companions
  for pale frosted surfaces, with their generated sources retained;
- `assets/wallpapers/aurora-glass.png`, the clean installed 1920-by-1080
  orbital desktop background extracted from the approved composition;
- `assets/design/aurora/aurora-button-states.png`, the normal, hover, and
  pressed material reference used to implement scalable QML buttons; and
- `assets/design/aurora/aurora-control-icons-source.png`, the system-control
  glyph reference.

Buttons remain scalable QML controls so text, focus, localization, and varying
window sizes do not blur a fixed raster. Their material, borders, states, and
corner treatment are taken from the generated button reference.

The top bar follows the approved app-menu composition, with the official mark,
NorthStar wordmark, Settings/File/Edit/View/Window/Help strip, compact hardware
status glyphs, battery, and date. The same glass, spacing, radii, and cyan focus
treatment carry through popups, Settings, Quick Settings, and the Dock.

Aurora window chrome is functional, not decorative. Normal application windows
offer compositor-native resizing from all four edges and all four corners,
retain their minimum usable dimensions, and expose draggable title bars.
Quick Settings keeps the compact control-centre shape from the reference while
providing a restrained drag affordance and constraining its remembered session
position to the usable desktop. Its primary hierarchy is Wi-Fi, Bluetooth, and
Dark Mode; separate Display and Sound cards; then confirmed battery, sleep,
power/session, and Settings actions. Features that do not fit that hierarchy
remain available in Settings instead of crowding the popup.

## Acceptance boundary

The change must preserve every existing controller and workflow. Focused gates
cover QML loading, shared controls, Settings, session asset installation, and
the shell. Physical acceptance on the Intel laptop remains the merge gate and
must check the top bar, dock, Settings, Quick Settings, dark/light switching,
window controls, scrollable content, and readable status/error states.
