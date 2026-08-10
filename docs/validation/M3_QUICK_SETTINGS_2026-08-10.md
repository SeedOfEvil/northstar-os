# M3 Quick Settings validation - 2026-08-10

## Scope

This record covers capability-backed Quick Settings and persistent shell-local
Do Not Disturb on `codex/m3-quick-settings`.

## Observed VM capability baseline

NSTAR-DEV01 exposes Ethernet (`vtnet0`) but no `wlan` interface, no confirmed
`ubt0hci` Bluetooth controller, no usable mixer device, and no ACPI LCD
brightness sysctl. The accepted UI for this VM must therefore show Wi-Fi,
Bluetooth, sound, display brightness, and Night Light as unavailable rather
than allowing mock state changes.

## Automated evidence

- The isolated working-tree build completed all 262 Ninja steps in
  `/home/northstar/validation/quick-settings-pr72-working/build` on
  NSTAR-DEV01.
- The focused Quick Settings controller and notification tests passed.
- The complete Qt/offscreen gate passed: 22 of 22 CTest targets.
- The QML surface contract passed.
- Immutable commit rebuild and installed-artifact smoke evidence remain
  pending.

## Manual 1280x800 noVNC acceptance

Pending user validation:

- the capability panel opens, moves, closes, and remains unclipped;
- unavailable VM hardware is clearly disabled and does not pretend to change;
- the Settings route opens the full Settings window;
- Do Not Disturb toggles, survives a controlled shell restart, and causes new
  notifications to remain read in history without an unread badge;
- disabling Do Not Disturb restores unread notification badges;
- both Lunar themes remain readable.

## Deferred hardware evidence

Actual wireless, Bluetooth, mixer, brightness, Night Light, direct DRM/KMS,
multi-display, and physical Intel/AMD behavior require suitable hardware and
remain separate acceptance evidence.
