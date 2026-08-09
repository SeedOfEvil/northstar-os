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
`bectl`. Authorized update execution, pre-upgrade boot environments, and
rollback remain protected M4 work.

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
non-mutating safety boundary: it reports whether the publication signature is
verified against the configured trusted fingerprint store, but it remains
blocked from execution even when the policy, fingerprint store, and signature
are valid.
For a valid policy, the controller also exposes a preview of the corresponding
FreeBSD `pkg` repository UCL without writing it to `/etc/pkg` or
`/usr/local/etc/pkg/repos`.

Policy and fingerprint-store structure alone is not cryptographic verification
and does not authorize `pkg` operations. The publication sidecar and signature
envelope provide the read-only verification boundary; a narrow update
authorization boundary and the pre-upgrade `bectl`/rollback flow are still
required before package mutation is exposed.

## M4-C publication metadata and update preview

The companion template at
`packaging/manifests/repository-metadata.json` defines the Northstar
publication sidecar used by the next release-tooling step. It records:

- the manifest schema, repository tag, channel, and target FreeBSD ABI;
- the repository catalogue revision and resolved Northstar source revision;
- the claimed signing status, public-key fingerprint, and relative signature
  envelope path; and
- the relative catalogue filename and its SHA-256 content digest; and
- each Northstar package's name, target version, FreeBSD origin, source input,
  and project revision.

This sidecar describes the output of a signed FreeBSD `pkg` repository; it does
not replace the repository's `meta.conf`, `data.pkg`, or `pkg` verification
path. The shell strictly parses the manifest, checks package provenance and
policy/channel identity, verifies the referenced catalogue file's SHA-256
digest, verifies the signature envelope's RSA signature over that digest, and
requires the envelope's public-key SHA-256 fingerprint to be present in the
trusted store. It compares the target package set with the loaded installed
inventory, so Software Center can show update and install candidates as a
read-only preview. A verified publication still cannot execute an update: the
plan remains blocked until a narrow update-authorization boundary is added.

## M4-D publication signature verification

The `signature_envelope` is a strict JSON sidecar with these fields:

- `schema_version=1` and `type=rsa`;
- `payload`, which must equal the manifest's `catalogue_sha256` value;
- `public_key_pem` and `signature_base64`; and
- `fingerprint_sha256`, which must match both the manifest and the SHA-256 of
  the PEM public-key bytes.

Northstar invokes the host `openssl` verifier in a temporary directory after
checking the relative path, exact keys, trusted fingerprint, payload, and
signature encoding. This is deliberately read-only and does not install
repositories, write pkg configuration, or authorize package mutation. The
envelope format is an application-side verification boundary around the
FreeBSD repository catalogue; it does not replace FreeBSD `pkg`'s own
repository-signature checks.

## M4-E update-authorization and boot-environment preflight

The Software Center now exposes a read-only **Update safety** status. Its
preflight is valid only when the repository policy and fingerprint store are
valid, the publication catalogue digest and signature are verified, an update
preview has been generated, and both `bectl` and `zfs` are available. For a
pending update it derives a bounded boot-environment name such as
`northstar-before-development-r42-abcdef1` and describes the transaction that
a future privileged helper must perform.

This slice never invokes `pkg`, writes repository configuration, creates a
boot environment, or authorizes package mutation. A valid preflight therefore
reports **Preflight only** until the narrow privileged helper and its explicit
user-confirmation contract are implemented. The next gate must prove that the
helper creates the named environment before an N-1 to N transaction and can
select the prior environment for rollback without touching home data.

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
provenance as parsed and verified when the trusted fingerprint matches, or
parsed but not verified when it does not, previews candidate counts, and still
does not invoke package mutation. Confirm that **Update safety** remains
**Blocked** without a valid policy/preview and becomes **Preflight only** only
when the verified plan and `bectl`/`zfs` prerequisites are present. No package
should be installed, removed, or upgraded during this validation.
