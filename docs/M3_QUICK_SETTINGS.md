# M3 capability-backed Quick Settings

Northstar Quick Settings reports observed FreeBSD capabilities instead of
presenting visual-only toggles. The controller uses bounded, unprivileged
status probes and exposes explicit available, unavailable, enabled, and status
properties to QML.

## Capability contract

- Wi-Fi is reported only when `ifconfig -l` exposes a `wlan` interface. The
  tile reports link state and an observed SSID; it does not claim to configure
  networks in this slice.
- Bluetooth is available only when `hccontrol` confirms `ubt0hci`.
- Volume is available only when `mixer -s vol` returns a parseable value. A
  slider mutation succeeds only if a second mixer read confirms the requested
  value within two percentage points.
- Display brightness uses FreeBSD's native `backlight(8)` interface when it is
  available. A mutation succeeds only after a second query confirms the
  requested value. The legacy ACPI brightness sysctl remains a read-only
  fallback.
- Native resolution and refresh-rate choices come from the connected output's
  `wlr-output-management` inventory through `wlr-randr`. For an internal eDP
  panel, Northstar may also offer its fixed 1600x900, 1366x768, and 1280x720
  16:9 lower modes when they are below the panel's largest reported mode.
  They use an output mode at compositor scale 1.0, not a Wayland UI scale;
  the fixed-resolution LCD still stretches a lower fullscreen signal to its
  physical pixels. Every choice is validated with `--dryrun`, applied as a 30-second preview,
  and restored unless the user explicitly keeps it. A kept native mode is
  written only to that user's matching Wayfire output group. A kept custom
  lower mode is stored in Northstar preferences and reapplied with
  `--custom-mode` at shell startup, avoiding Wayfire's native-mode config
  reload. Arbitrary connector names, caller-provided modes, and caller-provided
  command options are not accepted. Controller-driven modesets keep the same
  connector and resize the live shell, Settings, and confirmation windows in
  place. The screen-signal burst from that transaction is therefore excluded
  from the suspend/resume recovery path, which remains responsible for fully
  rebuilding surfaces after genuine output loss. The modal confirmation is the
  only Keep/Revert decision surface; Settings does not duplicate those actions.
  Both modal actions call the controller synchronously so no deferred UI
  callback can be lost.
- Night Light remains disabled until the compositor provides a tested color-
  management boundary.
- Unsupported controls remain visibly unavailable and provide a route to the
  full Settings window.

Ordinary external probes have an 800 ms timeout. `wlr-randr` gets up to five
seconds because a real output modeset can outlive the ordinary capability
probe budget. No probe or mutation uses root, shell command evaluation, or
unvalidated command arguments.

## Display acceptance

On the physical Intel lane, verify that Settings lists the native `eDP-1`
timings plus the bounded lower-mode choices, a preview visibly changes the panel,
the modal countdown remains visible through the output rebuild, Revert
restores the previous mode, the 30-second timeout also restores it, and Keep
survives a shell restart. Confirm that the desktop, panel, dock, and existing
application windows remain usable after each mode change. This focused
physical check is the merge gate; a nested X11 `xrandr` result is not
equivalent evidence.

## Do Not Disturb

Do Not Disturb is a shell-local preference stored in Northstar's existing
`preferences.ini`. While enabled, new notifications are retained in history as
already read, so they do not create an unread badge. Disabling it restores
normal unread notification behavior.

## Deferred work

Compositor-backed Night Light, external-monitor brightness, and a privileged
audio broker are intentionally outside this slice. A control must not claim
success until its backend confirms the change.
