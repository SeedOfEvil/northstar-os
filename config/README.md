# Configuration

This tree holds Northstar's Wayfire, session, D-Bus, display-manager, and
default configuration overlays. Configuration must be explicit, documented,
and safe to apply idempotently. It must not silently replace unrelated
administrator settings. `session/northstar.desktop` is the standard Wayland
session descriptor for the user-level `northstar-session` entry point.
The `xinitrc.nested-wayfire-supervised` file is an explicit opt-in fallback
for rehearsing that supervisor through the current nested X11 lane.
