# M0 development bootstrap

This document is the operational runbook for PR 2. It prepares a stock FreeBSD 15.1 amd64 host for Northstar desktop development without installing Northstar files into the base system. For the Proxmox installation and media-transfer path, see [`docs/M0_PROXMOX.md`](M0_PROXMOX.md).

## Supported host

The host validation requires:

- FreeBSD 15.1-RELEASE, including supported patch levels;
- amd64 architecture;
- UEFI boot (`sysctl machdep.bootmethod` reports `UEFI`);
- root mounted from ZFS and usable `bectl` boot-environment support;
- network access to the configured FreeBSD package repository;
- an existing non-root development account;
- root access through `sudo` or `doas` for the bootstrap step.

The bootstrap intentionally does not install `drm-kmod`. Graphics kernel modules remain a hardware-specific decision for the Intel/AMD validation lane.

## Run the bootstrap

Run these commands from the Northstar checkout on the FreeBSD host:

```sh
make check-host
sudo make bootstrap NORTHSTAR_USER=<development-user>
```

The bootstrap performs `pkg update`, verifies every package in [`packaging/manifests/bootstrap-packages.txt`](../packaging/manifests/bootstrap-packages.txt) with an exact repository search, installs only packages that are not already present, enables `dbus` and `seatd`, starts them if necessary, and adds the selected account to `video`.

The package set includes QTerminal, Firefox, and `xterm` so the first
Wayland/Xwayland smoke path is deterministic. It also installs SDDM for the
Northstar graphical-login slice, but bootstrap does not enable SDDM or change
the active display-manager policy.

Log out and back in after the first successful run so the new `video` group membership is present in the desktop session.

## Dry-run and capture

Review the planned mutations without changing packages, services, rc.conf, groups, or capture files:

```sh
sudo sh tools/bootstrap-dev.sh \
    --user <development-user> \
    --dry-run
```

The normal run writes a non-secret capture to `/var/log/northstar/m0-bootstrap.txt`. Override it when collecting test evidence:

```sh
sudo sh tools/bootstrap-dev.sh \
    --user <development-user> \
    --capture /tmp/northstar-m0-bootstrap.txt
```

The capture records the project commit when available, FreeBSD userland/kernel versions, architecture, boot method, root source, requested packages, and the installed package names/versions. It does not record credentials or the complete environment.

## Native smoke test

Start a Wayfire session as the unprivileged development user. A minimal manual session command is:

```sh
dbus-run-session -- wayfire
```

From a terminal inside that session, run the preflight:

```sh
make vm-smoke
```

Then run the process-level launch checks:

```sh
sh tests/vm/m0-smoke.sh --launch
```

For visual evidence, launch each application and close it normally:

```sh
QT_QPA_PLATFORM=wayland qterminal
MOZ_ENABLE_WAYLAND=1 firefox
xterm
```

The acceptance evidence must show a native Wayland Qt application, Firefox, and an X11 application through Xwayland. The shell itself is not part of M0; `make run-shell` remains a later milestone target.

## Diagnostics

Collect sanitized evidence without capturing the complete environment or process command lines:

```sh
make diagnostics OUTPUT=/tmp/northstar-m0-diagnostics
```

The output contains `host.txt`, `packages.txt`, `services.txt`, and `session.txt`. Inspect it before sharing and remove any unrelated local information.

## Recovery

The bootstrap does not remove packages, overwrite unrelated rc.conf entries, install GPU modules, or copy project files into `/usr/bin` or `/usr/lib`.

If package preflight fails, fix the repository configuration or package name and rerun the command. The script stops before package installation and service/group changes.

If a service does not start, inspect its status and collect diagnostics:

```sh
service dbus status
service seatd status
make diagnostics OUTPUT=/tmp/northstar-m0-diagnostics
```

If the account is not yet in `video`, log out and back in. If a maintainer intentionally needs to undo a setting added by this bootstrap, inspect the current value first and change only the affected setting:

```sh
sysrc dbus_enable
sysrc seatd_enable
```

Do not disable a service that was already enabled for another desktop or administrator workflow.

## Acceptance checklist

- [ ] `make check-host` passes on the exact supported host.
- [ ] First bootstrap completes with an explicit development user.
- [ ] Second bootstrap completes without package installation, service starts, or duplicate group modification.
- [ ] Package capture exists and contains resolved installed versions.
- [ ] User can start Wayfire without root.
- [ ] QTerminal launches through Wayland.
- [ ] Firefox launches.
- [ ] `xterm` launches through Xwayland.
- [ ] Diagnostics contain no secrets.
- [ ] No Northstar files are present in `/usr/bin` or `/usr/lib`.

## Upstream references

The implementation follows the FreeBSD documentation for [Wayland, seatd, and the `video` group](https://docs.freebsd.org/en/books/handbook/wayland/), [exact package catalogue searches](https://man.freebsd.org/cgi/man.cgi?query=pkg-search&sektion=8), [`sysrc`](https://man.freebsd.org/cgi/man.cgi?sysrc(8)), [`service`](https://man.freebsd.org/cgi/man.cgi?query=service&sektion=8), [`pw`](https://man.freebsd.org/cgi/man.cgi?query=pw&sektion=8), and [`bectl`](https://man.freebsd.org/cgi/man.cgi?bectl).
