# M4 signed pkg-repository smoke evidence

Environment: `NSTAR-DEV01`, FreeBSD 15.1 amd64, development user
`northstar`. The exact source tested was branch
`codex/m4-pkg-publication-smoke` at commit
`9e79569aa4aca36288c3f68002e4bedd29a91b05`.

## Publication and client checks

From a disposable archive of that commit:

```sh
sh -n tests/vm/pkg-repository-smoke.sh
make pkg-repository-smoke NORTHSTAR_PKG_FIXTURE_PACKAGE=qterminal
sudo -n make pkg-repository-smoke NORTHSTAR_PKG_CLIENT=1 \
  NORTHSTAR_PKG_FIXTURE_PACKAGE=qterminal
```

Both modes passed. The unprivileged mode created a FreeBSD v2 `meta.conf` and
signed `data.pkg` containing `data`, `data.sig`, and `data.pub`. The client mode
created a temporary root-owned `file://` repository, used
`SIGNATURE_TYPE=FINGERPRINTS`, processed one package, and completed `pkg update
-f` successfully. The package, repository, RSA key, fingerprint store,
temporary package database, and cache were removed after the run. No package
was installed, upgraded, removed, or registered in the system database.

## Native regression checks

The same disposable source archive passed these shell-level groups:

```sh
sh tests/unit/test-m0-scripts.sh
sh tests/unit/test-nested-wayfire-session.sh
sh tests/unit/test-console-autostart.sh
sh tests/unit/test-sddm-theme.sh
sh tests/unit/test-sddm-fallback.sh
sh tests/unit/test-session-script.sh
```

The existing FreeBSD build cache passed the Qt suite with the required headless
platform setting:

```sh
env QT_QPA_PLATFORM=offscreen \
  ctest --test-dir /home/northstar/src/northstar/build --output-on-failure
```

Result: `9/9` CTest cases passed. The canonical VM checkout was left untouched
because it contains unrelated uncommitted development work; the new branch's
archive was used for all publication and shell-script checks.

## Gate status

This closes the native FreeBSD `pkg repo` publication/signature/client
contract smoke gate. It does not claim a release repository. Protected
Poudriere inputs, development/stable hosting, signing-key custody, package
provenance, privileged update authorization, and N-1/rollback evidence remain
open M4 gates.
