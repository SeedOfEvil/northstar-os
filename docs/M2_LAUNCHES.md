# M2 application launch tracking

The shell's `ApplicationLauncher` is the first user-facing M2 launch service.
It keeps the existing `.desktop` catalog and launch APIs, but now gives every
launch path the same result handling:

- terminal and browser shortcuts use the same controller as overview entries;
- desktop-entry launches preserve the catalog desktop ID and display name;
- successful launches capture the detached process PID when Qt provides it;
- failures produce a visible shell notification and a failed launch record;
- commands are passed directly to `QProcess` and are never evaluated by a
  shell.

Launch records are written to the user-private
`$XDG_STATE_HOME/northstar/launch.log` path, or
`$HOME/.local/state/northstar/launch.log` when `XDG_STATE_HOME` is unset. The
parent directory is owner-only and each record contains a UTC timestamp,
desktop ID, display name, program, PID, and `started` or `failed` result. No
arguments or environment variables are recorded.

The shell exposes the most recent result through `ApplicationLauncher` QML
properties and displays a short success/failure notification. The in-shell
Notification Center also keeps a bounded, persistent history of those
events with mark-read and clear controls. Notifications are feedback only; they
do not supervise or terminate the launched application.

Deterministic unit tests cover PID propagation, result properties, launch log
contents, catalog arguments, invalid desktop IDs, and the Notification
Center's bounded-history, read, dismiss, and clear behavior. The live VM check
should launch Terminal and Firefox from the menu or overview, confirm the
top-bar notification badge, open the Notification Center, and inspect the
private log without requiring direct DRM/KMS graphics.
