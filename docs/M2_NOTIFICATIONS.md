# M2 notification center

Northstar now has a small in-shell notification service for user-visible
desktop events. The first producer is `ApplicationLauncher`: successful and
failed launches appear as a short top-bar toast and are also retained in a
bounded session-scoped history.

The top bar shows the Northstar notification icon and an unread badge. Opening
the panel marks the current entries read. Each entry displays its title, body,
kind, and timestamp; individual entries can be dismissed, and the full history
can be cleared. The service caps history at 40 entries so a runaway producer
cannot grow the shell indefinitely.

This is deliberately an in-process, unprivileged service for the development
desktop. It does not persist notifications, execute actions, supervise
applications, or claim a desktop-wide D-Bus notification protocol. A later
service boundary can add those capabilities without changing the shell's
basic model.

## Evidence

`NotificationCenterTest` covers insertion order, unread counts, mark-read,
dismissal, clearing, normalization, and the bounded history. The native VM
acceptance flow is:

```sh
cd /home/northstar/src/northstar
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

After restarting the shell, launch Terminal or Firefox, confirm the toast and
badge, open Notifications, and verify that marking/clearing entries updates
the panel without affecting the launched application.
