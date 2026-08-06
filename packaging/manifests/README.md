# Build manifests

Build manifests are inputs to release automation, not a record of whatever happened to be installed on a developer workstation.

`upstream.lock` identifies the FreeBSD release, architecture, Ports revision, resolved package versions, and project commit. A release builder must replace every `UNSET` or `RESOLVED_BY_BUILDER` value before it can produce a release artifact. It must also emit a machine-readable provenance document containing package versions, build host, compiler, and artifact SHA-256.

The bootstrap package list is a starting manifest for M0. It does not authorize installation on arbitrary hosts; `bootstrap-dev.sh` must verify the host and package availability first.
