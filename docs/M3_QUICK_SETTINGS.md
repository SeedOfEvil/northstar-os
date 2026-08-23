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
- Night Light remains disabled until the compositor provides a tested color-
  management boundary.
- Unsupported controls remain visibly unavailable and provide a route to the
  full Settings window.

Every external probe has an 800 ms timeout. No probe or mutation uses root,
shell command evaluation, or user-provided command arguments.

## Do Not Disturb

Do Not Disturb is a shell-local preference stored in Northstar's existing
`preferences.ini`. While enabled, new notifications are retained in history as
already read, so they do not create an unread badge. Disabling it restores
normal unread notification behavior.

## Deferred work

Compositor-backed Night Light, external-monitor brightness, and a privileged
audio broker are intentionally outside this slice. A control must not claim
success until its backend confirms the change.
