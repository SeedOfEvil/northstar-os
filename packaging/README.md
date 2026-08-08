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
`pkg query` inventory. It must not be treated as repository or update support;
signed metadata, narrow privileged authorization, and `bectl` rollback remain
required before any package mutation is exposed.

Do not commit package repositories, signing keys, or unsigned release claims.
