# Packaging

Northstar uses the FreeBSD package ecosystem. `packaging/ports/` will contain the project Ports overlay, `packaging/poudriere/` will contain clean-jail build configuration, and `packaging/repository/` will contain only generated local output.

The first release model is:

```text
FreeBSD base and kernel -> official FreeBSD update mechanism
Third-party desktop dependencies -> pinned FreeBSD package source
Northstar components -> signed Northstar pkg repository
Major upgrade -> ZFS boot environment created with bectl first
```

The shell's current Software Center is intentionally limited to a read-only
`pkg query` inventory, verified publication preview, and update-safety
preflight. It must not be treated as repository or update execution support;
an actual signed `pkg` repository, narrow privileged authorization, and
`bectl` rollback remain required before any package mutation is exposed.

Do not commit package repositories, signing keys, or unsigned release claims.

## Signed development channel

The production-shaped development publisher is documented in
[`docs/M4_SIGNED_DEVELOPMENT_CHANNEL.md`](../docs/M4_SIGNED_DEVELOPMENT_CHANNEL.md).
It consumes resolved immutable package artifacts and source locks, delegates
both catalogue and sidecar signing to external executables, and emits an
atomic repository plus policy, provenance, manifest digest, and fingerprint
records. Generated repositories and all private keys remain untracked.

## Native publication smoke gate

On FreeBSD, `make pkg-repository-smoke` creates a temporary repository from an
already-installed fixture package, invokes `pkg repo` with a disposable
external RSA signer, and checks the v2 `meta.conf` plus signed `data.pkg`
members. It removes the temporary package, repository, key, and fingerprint
store on exit. This is publication-contract evidence only; it is not a
development or stable release repository and does not use Poudriere inputs.

To include the client-side trust check, run the explicit root-scoped target:

```sh
sudo -n make pkg-repository-smoke NORTHSTAR_PKG_CLIENT=1
```

The client path uses only a temporary root-owned `file://` repository and
isolated `PKG_DBDIR`/`PKG_CACHEDIR` values. It runs `pkg update -f` to prove
that `SIGNATURE_TYPE=FINGERPRINTS` accepts the catalogue, but never invokes
`pkg install`, `pkg upgrade`, repository configuration writes, or a system
package-database operation. The root requirement is a FreeBSD `pkg` ownership
check for local `file://` repositories, not a product authorization boundary.
