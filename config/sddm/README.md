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

The first branded greeter is in `config/sddm/northstar`. It uses SDDM's PAM
authentication proxy, session model, and power callbacks while displaying the
official logo from `assets/branding/northstar-logo.png`. CMake installs the
theme to `share/sddm/themes/northstar` and the canonical logo to
`share/northstar/branding`.

On a host with SDDM installed, preview the theme without enabling the display
manager:

```sh
sddm-greeter --test-mode --theme "$PWD/config/sddm/northstar"
```

The system service remains a deliberate acceptance step. Keep a recovery
console available, stop the current `startx` session, and disable console
autostart before enabling SDDM so two session owners cannot race at boot.

The current Proxmox VM uses the explicit X11 compatibility session:

```text
Northstar (Proxmox X11 fallback)
```

It launches `northstar-session-x11` with nested Wayfire, `WLR_BACKENDS=x11`,
and `WLR_RENDERER=pixman`. Enable it only after installing the project to the
system prefix and preserving a recovery path:

```sh
sudo cmake --install build --prefix /usr/local
make disable-console-autostart
sudo sh tools/install-sddm-fallback.sh --enable
```

The installer writes a managed SDDM drop-in and `sddm_enable="YES"`; it does
not enable autologin. The normal `Northstar` Wayland session remains available
for a future DRM-capable host.

The current development VM also has an explicit console-login convenience
installer, `make install-console-autostart`. That hook is limited to local
`ttyv0` through `ttyv7` logins and is not a substitute for SDDM integration.
Autologin remains deferred until a lock screen and a manual recovery path are
validated.

The current Proxmox basic-VGA lane remains a `startx`/nested-X11 fallback and
does not prove display-manager or direct DRM/KMS acceptance.
