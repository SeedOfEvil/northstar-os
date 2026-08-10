# M4 signed development package channel

Northstar can publish its native FreeBSD package as a signed development
repository without placing a private key in the source tree, package input, or
repository output.

## Package artifact

CMake configures CPack's FreeBSD generator to produce a single `northstar`
package containing the installed shell, session, first-party applications,
branding, and update broker. The package records the project version,
`x11/northstar` origin, BSD-2-Clause license, and its direct Qt, Wayland, and
Layer Shell package dependencies.

Run the native package build on FreeBSD:

```sh
make package
pkg query -F build/northstar-0.1.0-amd64.pkg '%n|%v|%o'
```

## Publication boundary

`tools/publish-development-repository.sh` accepts an immutable directory of
`.pkg` files and requires:

- a fully resolved source lock naming FreeBSD, architecture, Ports branch and
  exact commit, Qt and Wayfire versions, and the exact Northstar source commit;
- a non-negative repository revision and explicit UTC build timestamp;
- an external `pkg repo` signing-command executable;
- a separate external publication signer accepting payload and output paths;
- the corresponding public key, `pkg+https` channel URL, and installed
  fingerprint-store path.

The publisher never accepts a private-key path. It atomically creates a v2
FreeBSD repository, copies the immutable package archives, records every
package name/version/origin/project revision, writes the resolved policy and
client configuration, and rejects unresolved locks, duplicate packages,
unsafe metadata, missing Northstar packages, or failed signature verification.

The generated `publication-record.conf` records the source-lock digest,
publication-manifest digest, catalogue digest, signing fingerprint, dependency
versions, source revision, timestamp, and repository revision. Private-key
filenames are rejected from the output as a final containment check.

## Manifest-bound signatures

Signature envelope schema 2 signs the SHA-256 digest of the complete
`repository-metadata.json` document. That manifest contains the signed
catalogue digest and package provenance, so changing a package version,
revision, channel, ABI, timestamp, or catalogue digest invalidates the
publication signature. Northstar continues to read legacy schema-1 envelopes
for existing test fixtures, but all new development publications use schema 2.

Software Center exposes the verified channel, repository revision, manifest
digest, ABI, and package provenance in its read-only update-plan review.
Install, remove, upgrade, and rollback actions remain disabled.

## Key custody and protected builders

The repository contains no persistent signing key. Development/stable signing
executables and private keys belong to a protected publication environment
outside pull-request execution. The VM acceptance gate uses a disposable key
that is destroyed with its temporary directory.

This PR proves publication from an immutable native package artifact and a
resolved Ports/dependency lock. A protected Poudriere jail remains the required
source of production release package artifacts; it feeds this same publisher
without changing the trust or Software Center contracts.

## Acceptance

On FreeBSD, after `make package`, run:

```sh
sudo -n make signed-development-repository-smoke \
  NORTHSTAR_SOURCE_REVISION="$(git rev-parse HEAD)"
```

The test uses isolated package databases and repository configuration. It
must accept the authentic repository, reject altered signed catalogue
metadata, confirm package provenance, and find no private key in the output.
It never invokes package installation, removal, or upgrade.
