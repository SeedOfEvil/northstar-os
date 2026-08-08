# M3 settings persistence slice

Northstar's existing Appearance setting now survives a shell restart. The
`ShellState` service stores the dark-mode preference in the user-scoped Qt
configuration path returned by `QStandardPaths::AppConfigLocation`, under
`preferences.ini`. It never requires root access and does not write into the
system package or source tree.

The setting is loaded before the shell surfaces are created and saved when the
Appearance page changes it. Other preferences remain intentionally unchanged
until their state and validation requirements are defined.

## Validation

From the FreeBSD development checkout:

```sh
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

In a fresh Northstar session, open **Settings**, switch **Dark appearance**,
end or restart the session, and confirm the same appearance is restored. Then
switch it back and repeat once to verify both values persist.
