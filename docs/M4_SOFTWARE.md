# M4 Software Center foundation

Northstar now exposes a first-party Software surface from the system menu and
the `Meta+U` shortcut. It reads the installed FreeBSD package inventory with
the non-mutating command:

```sh
pkg query -a '%n|%v|%c'
```

The surface supports:

- a searchable installed-package list with name, version, and description;
- an explicit refresh action and bounded status feedback;
- a clear package-manager availability state; and
- the Northstar visual language, movable/resizable window behavior, and
  read-only boundaries used by the other desktop surfaces.

This is an inventory slice, not an update implementation. It never invokes
`pkg install`, `pkg upgrade`, repository mutation, privileged helpers, or
`bectl`. Signed repository metadata, authorized update execution, pre-upgrade
boot environments, and rollback remain protected M4 work.

## M4-A package-trust boundary

The M4-A slice adds a read-only repository policy contract at
`packaging/manifests/repository-policy.conf`. A configured policy contains:

- `channel=development` or `channel=stable`;
- a safe `repository_tag` used for the FreeBSD `pkg` UCL repository key;
- a repository name;
- a `pkg+https` repository URL and `mirror_type`;
- `signature_type=fingerprints` and an absolute `fingerprints_path` containing
  the `trusted/` and `revoked/` fingerprint files; and
- `trust_mode=required`.

The shell can validate this structure from the user configuration path
`$XDG_CONFIG_HOME/northstar/repository-policy.conf` (or the platform's
equivalent Qt application-config path). Software Center reports whether the
policy is absent, rejected, or structurally valid, and whether its trusted and
revoked fingerprint store is structurally valid. **Plan Update** remains a
non-mutating safety boundary: it explicitly reports that repository signature
verification is not connected yet, even when the policy and fingerprint store
are valid.
For a valid policy, the controller also exposes a preview of the corresponding
FreeBSD `pkg` repository UCL without writing it to `/etc/pkg` or
`/usr/local/etc/pkg/repos`.

Structural validation is not cryptographic verification and does not authorize
`pkg` operations. The following M4 slices must connect the policy to signed
FreeBSD repository metadata, a narrow update authorization boundary, and the
pre-upgrade `bectl`/rollback flow before package mutation is exposed.

## M4-C publication metadata and update preview

The companion template at
`packaging/manifests/repository-metadata.json` defines the Northstar
publication sidecar used by the next release-tooling step. It records:

- the manifest schema, repository tag, channel, and target FreeBSD ABI;
- the repository catalogue revision and resolved Northstar source revision;
- the claimed signing status and public-key fingerprint; and
- the relative catalogue filename and its SHA-256 content digest; and
- each Northstar package's name, target version, FreeBSD origin, source input,
  and project revision.

This sidecar describes the output of a signed FreeBSD `pkg` repository; it does
not replace the repository's `meta.conf`, `data.pkg`, or `pkg` verification
path. The shell strictly parses the manifest, checks package provenance and
policy/channel identity, verifies the referenced catalogue file's SHA-256
digest, and compares its target package set with the loaded installed
inventory. Software Center can therefore show update and install candidates as
a read-only preview. The signature field remains an untrusted claim until a
verifier consumes the actual repository catalogue, so the plan is always
blocked from execution in this slice.

## VM validation

From the FreeBSD development checkout:

```sh
env QT_QPA_PLATFORM=offscreen make test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
```

After restarting the shell, open **Software Center** from the Northstar menu
or press **Meta+U**. Confirm that **Refresh** populates installed packages,
that package-name/version/description search is case-insensitive, and that the
surface clearly labels itself read-only. Confirm that **Repository policy**
reports **Not configured** on a default development VM and that **Plan Update**
reports a blocked, non-mutating plan. If a test publication manifest is placed
at the user configuration path, confirm that Software Center reports its
provenance as parsed but not verified, previews candidate counts, and still
does not invoke package mutation. No package should be installed, removed, or
upgraded during this validation.
