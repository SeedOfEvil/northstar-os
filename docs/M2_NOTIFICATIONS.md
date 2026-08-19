# M2 notification center

Northstar now has a small in-shell notification service for user-visible
desktop events. The first producer is `ApplicationLauncher`: successful and
failed launches appear as a short top-bar toast and are also retained in a
bounded history that now survives a restart.

The top bar shows the Northstar notification icon and an unread badge. Opening
the panel marks the current entries read. Each entry displays its title, body,
kind, and age; individual entries can be dismissed, and the full history
can be cleared from the panel or from Settings. The service caps history at 40 entries so a runaway producer
cannot grow the shell indefinitely.

History is kept on disk from M7 onward. The shell writes it to
`notifications.ini` in the account's own configuration directory with
owner-only permissions, so a shared machine does not leak one account's
application history to another. Entries are restored at login with their read
and unread state intact, capped at the same 40 entries, and dropped once they
are older than 14 days. A truncated or hand-edited file is read as far as it
parses: malformed records are skipped rather than blocking startup.

Because history now outlives a session, the panel shows each entry's age
("Just now", "2h ago", "Yesterday") and keeps the exact timestamp on hover.

This remains an in-process, unprivileged service for the development desktop.
It does not execute actions, supervise applications, or claim a desktop-wide
D-Bus notification protocol. A later service boundary can add those
capabilities without changing the shell's basic model.

## Evidence

`NotificationCenterTest` covers insertion order, unread counts, mark-read,
dismissal, clearing, normalization, the bounded history, restoration across a
restart, identifier continuity, and the age wording. `NotificationStoreTest`
covers the on-disk file: owner-only permissions, a full field round trip,
retention, malformed records, and shrinking the file when history shrinks. The native VM
acceptance flow is:

```sh
cd /home/northstar/src/northstar
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

After restarting the shell, launch Terminal or Firefox, confirm the toast and
badge, open Notifications, and verify that marking/clearing entries updates
the panel without affecting the launched application.
