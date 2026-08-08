# Northstar SDDM theme

This directory contains the first branded graphical-login slice. It uses
SDDM's native theme API for PAM-backed authentication, session selection, and
power actions; the theme never handles passwords itself.

The canonical logo source is
[`assets/branding/northstar-logo.png`](../../../assets/branding/northstar-logo.png).
The copy below is the theme-local distribution asset required by SDDM and is
checked against the canonical file by the unit test.

For a non-destructive theme preview on a host with SDDM installed:

```sh
sddm-greeter --test-mode --theme "$PWD/config/sddm/northstar"
```

The theme is installed below `share/sddm/themes/northstar` by CMake. Enabling
the SDDM service remains a separate acceptance step; do not enable it on the
current Proxmox fallback lane until the existing `startx` session has been
stopped and a recovery console is available.

On the current Proxmox VM, select `Northstar (Proxmox X11 fallback)` in the
session list. That session is deliberately separate from the native
`Northstar` Wayland entry and uses nested Wayfire/software rendering.
