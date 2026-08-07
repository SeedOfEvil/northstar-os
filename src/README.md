# Northstar source

The source tree contains common libraries, the shell, session, launcher,
settings, notifications, and search services. The M1 slice adds the Qt/QML
shell seed under `shell/` and the standard `.desktop` application catalog
under `launcher/`; the M2 slice adds the user-level
`session/northstar-session` supervisor and its installable Wayland session
entry point. Compositor-sensitive setup lives in the small
`LayerShellSurface` adapter so the shell does not depend on undocumented
Wayfire internals.
