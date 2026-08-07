# SDDM integration boundary

Northstar's display-manager contract is the standard Wayland session file:

```text
share/wayland-sessions/northstar.desktop
```

The CMake install and later FreeBSD package should place that file below the
active package prefix, normally:

```text
/usr/local/share/wayland-sessions/northstar.desktop
```

The descriptor launches the unprivileged `northstar-session` executable. It
does not enable SDDM, configure autologin, or change an administrator's
existing display-manager settings. A future login acceptance run must select
Northstar manually from a fresh console session, verify the user identity and
Wayland environment, then log out cleanly before any autologin policy is
considered.

The current Proxmox basic-VGA lane remains a `startx`/nested-X11 fallback and
does not prove display-manager or direct DRM/KMS acceptance.
