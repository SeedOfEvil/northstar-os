# M4 Software Center foundation

Northstar now exposes a first-party Software surface from the system menu and
the `Meta+U` shortcut. It reads the installed FreeBSD package inventory with
the non-mutating command:

```sh
pkg query -a '%n|%v|%c'
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

## M4-A package-trust boundary

The next slice adds a read-only repository policy contract at
`packaging/manifests/repository-policy.conf`. A configured policy contains:

- `channel=development` or `channel=stable`;
- a repository name;
- an HTTPS repository URL;
- a `SHA256:` signing-key fingerprint with 64 hexadecimal characters; and
- `trust_mode=required`.

The shell can validate this structure from the user configuration path
`$XDG_CONFIG_HOME/northstar/repository-policy.conf` (or the platform's
equivalent Qt application-config path). Software Center reports whether the
policy is absent, rejected, or structurally valid. **Plan Update** remains a
non-mutating safety boundary: it explicitly reports that repository signature
verification is not connected yet, even when the policy structure is valid.

Structural validation is not cryptographic verification and does not authorize
`pkg` operations. The following M4 slices must connect the policy to signed
FreeBSD repository metadata, a narrow update authorization boundary, and the
pre-upgrade `bectl`/rollback flow before package mutation is exposed.

## VM validation

From the FreeBSD development checkout:

```sh
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

After restarting the shell, open **Software Center** from the Northstar menu
or press **Meta+U**. Confirm that **Refresh** populates installed packages,
that package-name/version/description search is case-insensitive, and that the
surface clearly labels itself read-only. Confirm that **Repository policy**
reports **Not configured** on a default development VM and that **Plan Update**
reports a blocked, non-mutating plan. No package should be installed, removed,
or upgraded during this validation.
