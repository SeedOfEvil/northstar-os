# M4 update-helper contract evidence

Environment: `NSTAR-DEV01`, FreeBSD 15.1 amd64, development user
`northstar`. The exact source tested was branch
`codex/m4-update-helper-contract` at commit
`84fef93cef82a4de265f37a83ce698b8f72ad311`.

## Request validation

From a disposable archive of that commit:

```sh
make update-helper-test
```

The unit test passed. It verified the capabilities response, a valid
`create-before` dry run, rejection of unsupported operations, bounded
boot-environment names, rejection of unverified plans, duplicate-field
rejection, and the root-only apply boundary. The test did not invoke `bectl`,
`pkg`, `sudo`, or a system mutation.

## Root-owned request mapping

The native smoke used a temporary request with mode `0600` and root ownership,
plus a temporary fake `bectl` selected only through the test override:

```sh
sudo -n env NORTHSTAR_UPDATE_BECTL_PATH="$TMPDIR/bectl" \
  sh src/update/northstar-update-helper --apply "$TMPDIR/root.request"
sudo -n env NORTHSTAR_UPDATE_BECTL_PATH="$TMPDIR/bectl" \
  sh src/update/northstar-update-helper --apply "$TMPDIR/root.rollback.request"
```

The fake executable recorded exactly:

```text
create northstar-before-development-r42-abcdef123456
activate northstar-before-development-r42-abcdef123456
```

All request files, the fake executable, and its log were removed after the
run. The real `/sbin/bectl` was never called, no boot environment was created
or activated, and no package command was executed.

## Gate status

This closes the request-validation and allowlisted-operation contract only.
The helper is not installed or connected to sudoers, D-Bus, or Software
Center. A future privileged broker must create requests only after rechecking
the verified publication and obtaining explicit user confirmation, then the
real N-1 update, boot-environment creation, rollback, and home-data tests can
begin in a disposable ZFS environment.
