# Contributor tools

M0 provides `check-host.sh`, `bootstrap-dev.sh`, and `collect-diagnostics.sh`. They use POSIX `/bin/sh`, avoid secrets, and document manual recovery. `run-session.sh` remains deferred until the Northstar session/shell implementation exists.

Use the repository Make targets for the stable interface:

```sh
make check-host
make bootstrap NORTHSTAR_USER=<development-user>
make diagnostics OUTPUT=/tmp/northstar-m0-diagnostics
```
