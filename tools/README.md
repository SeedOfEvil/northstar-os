# Contributor tools

M0 provides `check-host.sh`, `bootstrap-dev.sh`, `collect-diagnostics.sh`, and
the optional `build-nested-wayfire.sh`. They use POSIX `/bin/sh`, avoid
secrets, and document manual recovery. `run-session.sh` remains deferred until
the Northstar session/shell implementation exists.

Use the repository Make targets for the stable interface:

```sh
make check-host
make bootstrap NORTHSTAR_USER=<development-user>
make diagnostics OUTPUT=/tmp/northstar-m0-diagnostics
```

On the Proxmox basic-VGA validation lane, the stock FreeBSD Wayfire package
cannot start without a DRM render node. After installing the optional build
dependencies documented in `docs/M0_PROXMOX.md`, use:

```sh
make nested-wayfire
```

This keeps the package-managed Wayfire intact and installs the patched binary
under the development user's home directory.
