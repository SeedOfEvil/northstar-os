# M4 verified update-broker evidence

Environment: `NSTAR-DEV01`, FreeBSD 15.1 amd64, development user
`northstar`. The exact source tested was branch
`codex/m4-update-broker` at commit
`378cfde51a0bd742865dda49c618bb71c7ea6ebc`.

## Focused build and controller test

From a disposable archive of that commit:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF
cmake --build build --target northstar-update-broker --parallel 2
make update-helper-test
```

The broker target built successfully. All seven deterministic update-helper
contract tests passed. A second disposable archive configured
`BUILD_TESTING=ON`, built `test-updateplancontroller`, and ran
`northstar-updateplancontroller`; the focused CTest passed.

## Root-only broker smoke

The native smoke ran the exact built broker with a temporary signed
publication, root-owned policy, fingerprint store, installed snapshot, fake
`bectl`, and fake `zfs`:

```sh
sudo -n env NORTHSTAR_UPDATE_BROKER_BIN="$PWD/build/src/update/northstar-update-broker" \
  /bin/sh "$PWD/tests/vm/update-broker-smoke.sh"
```

The broker independently revalidated policy, trusted fingerprint, catalogue
digest, RSA publication signature, and the pending preview. It staged a
root-owned mode-`0600` request for:

```text
northstar-before-development-r42-abcdef1
```

The request contained `operation=create-before` and
`authorization=interactive-confirmation`. The smoke reported that no `pkg`
or real `bectl` command was run; all fake tools and temporary inputs were
removed after the run.

## Gate status

This closes the independent verified-plan staging gate only. The broker is
not connected to sudoers or D-Bus, does not execute package mutation, and does
not create or activate a real boot environment. The next gate is reviewed
privileged deployment followed by disposable ZFS N-1 update, rollback, shell
recovery, and home-data preservation evidence.
