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
- Resolution and refresh-rate choices come only from the connected output's
  `wlr-output-management` inventory through `wlr-randr`. Northstar validates
  an enumerated mode with `--dryrun`, applies it as a 15-second preview, and
  restores the previous mode unless the user explicitly keeps it. A kept mode
  is written only to that user's matching Wayfire output group; arbitrary
  connector names, custom modelines, and caller-provided command options are
  not accepted.
- Night Light remains disabled until the compositor provides a tested color-
  management boundary.
- Unsupported controls remain visibly unavailable and provide a route to the
  full Settings window.

Every external probe has an 800 ms timeout. No probe or mutation uses root,
shell command evaluation, or unvalidated command arguments.

## Display acceptance

On the physical Intel lane, verify that Settings lists `eDP-1` modes exactly as
reported by Wayfire, a preview visibly changes the panel, Revert restores the
previous mode, the 15-second timeout also restores it, and Keep survives a
shell restart. Confirm that the desktop, panel, dock, and existing application
windows remain usable after each mode change. This focused physical check is
the merge gate; a nested X11 `xrandr` result is not equivalent evidence.

## Do Not Disturb

Do Not Disturb is a shell-local preference stored in Northstar's existing
`preferences.ini`. While enabled, new notifications are retained in history as
already read, so they do not create an unread badge. Disabling it restores
normal unread notification behavior.

## Deferred work

Compositor-backed Night Light, external-monitor brightness, and a privileged
audio broker are intentionally outside this slice. A control must not claim
success until its backend confirms the change.
