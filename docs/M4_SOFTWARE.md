# M4 Software Center foundation

Northstar now exposes a first-party Software surface from the system menu and
the `Meta+U` shortcut. It reads the installed FreeBSD package inventory with
the non-mutating command:

```sh
pkg query -a '%n\t%v\t%c'
```

The surface supports:

- a searchable installed-package list with name, version, and description;
- an explicit refresh action and bounded status feedback;
- a clear package-manager availability state; and
- the Northstar visual language, movable/resizable window behavior, and
  read-only boundaries used by the other desktop surfaces.

This is an inventory slice, not an update implementation. It never invokes
`pkg install`, `pkg upgrade`, repository mutation, privileged helpers, or
`bectl`. Signed repository metadata, update planning, pre-upgrade boot
environments, and rollback remain the M4 acceptance work ahead.

## VM validation

From the FreeBSD development checkout:

```sh
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

After restarting the shell, open **Software Center** from the Northstar menu
or press **Meta+U**. Confirm that **Refresh** populates installed packages,
that package-name/version/description search is case-insensitive, and that the
surface clearly labels itself read-only. No package should be installed,
removed, or upgraded during this validation.
