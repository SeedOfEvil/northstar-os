# M4 Software Center foundation

Northstar now exposes a first-party Software surface from the system menu and
the `Meta+U` shortcut. It reads the installed FreeBSD package inventory with
the non-mutating command:

```sh
pkg query -a '%n|%v|%c'
```

The surface supports:

- a searchable installed-package list with name, version, and description;
- selectable package rows with a details view showing the installed version and
  the explicit mutation gate;
- an explicit refresh action and bounded status feedback;
- a clear package-manager availability state; and
- the Northstar visual language, movable/resizable window behavior, and
  read-only boundaries used by the other desktop surfaces.

This is an inventory slice, not an update implementation. It never invokes
`pkg install`, `pkg upgrade`, repository mutation, privileged helpers, or
`bectl`. Authorized update execution, pre-upgrade boot environments, and
rollback remain protected M4 work.

Selecting a package is intentionally useful even while mutation is gated: the
details view identifies the installed package and explains why Install and
Remove remain disabled. This keeps the surface honest and testable instead of
presenting buttons that silently do nothing.

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

## M4-F native `pkg` publication smoke gate

`tests/vm/pkg-repository-smoke.sh` closes the first native publication-contract
gap without claiming a release repository. It selects one installed fixture
package, creates a FreeBSD v2 repository with `pkg repo`, signs the catalogue
through the external-command interface, checks `meta.conf` and the signed
`data.pkg` members (`data`, `data.sig`, and `data.pub`), and records the
temporary public-key fingerprint in a FreeBSD-style trusted store. With the
explicit `--client` mode, an isolated root-owned `file://` client runs
`pkg update -f` using `SIGNATURE_TYPE=FINGERPRINTS` and temporary package DB and
cache paths.

The smoke gate creates no project package repository, persists no key, and
does not install, upgrade, remove, or configure packages. It proves the
FreeBSD publication/signature/client contract only. Protected Poudriere
inputs, development/stable repository hosting, key custody, package
provenance, and the update/rollback helper remain open M4 release gates.

## M4-G narrow update-helper request contract

`src/update/northstar-update-helper` defines the first privileged boundary for
boot-environment work. Its capabilities are intentionally small: a verified,
root-owned mode-0600 request may ask for `bectl create` before an update or
`bectl activate` for rollback. The request binds the operation to the channel,
repository revision, source revision, catalogue digest, signature fingerprint,
and the exact bounded boot-environment name derived from those values. The
helper's read-only `--dry-run` path validates the contract; `--apply` requires
root and never invokes `pkg` or accepts shell text.

This slice adds no sudoers rule, D-Bus broker, package transaction, or real VM
boot-environment mutation. The request producer must be a future privileged
broker that rechecks the verified update plan and obtains explicit user
confirmation. Until that broker and the N-1/rollback fixture exist, Software
Center correctly remains **Preflight only**.

## M4-H independent verified-plan broker

`northstar-update-broker` reuses the tested trust and update-plan core in a
separate process. Its first operation, `--stage-create-before`, requires root,
explicit `--confirm`, root-owned non-group-writable policy/metadata/snapshot
inputs, and a root-owned request directory. It independently verifies the
publication policy, trusted fingerprint store, catalogue digest, RSA signature,
and pending package preview before writing the helper's mode-0600 request.

The broker checks `bectl` and `zfs` availability through the existing preflight
but does not call either tool or invoke `pkg`. No sudoers rule, D-Bus service,
package transaction, or real VM boot-environment mutation is added. The next
gate is a reviewed privileged deployment plus a disposable ZFS N-1 update and
rollback transaction.

## VM validation

From the FreeBSD development checkout:

```sh
env QT_QPA_PLATFORM=offscreen make test
make update-helper-test
make install-user NORTHSTAR_PREFIX="$HOME/.local"
make pkg-repository-smoke
sudo -n make pkg-repository-smoke NORTHSTAR_PKG_CLIENT=1
make build
sudo -n make update-broker-smoke
```

After restarting the shell, open **Software Center** from the Northstar menu
or press **Meta+U**. Confirm that **Refresh** populates installed packages,
that package-name/version/description search is case-insensitive, and that the
surface clearly labels itself read-only. Select a package row and confirm its
details view shows the installed version and explains why **Install** and
**Remove** are disabled. Confirm that **Repository policy**
reports **Not configured** on a default development VM and that **Plan Update**
reports a blocked, non-mutating plan. If a test publication manifest is placed
at the user configuration path, confirm that Software Center reports its
provenance as parsed and verified when the trusted fingerprint matches, or
parsed but not verified when it does not, previews candidate counts, and still
does not invoke package mutation. Confirm that **Update safety** remains
**Blocked** without a valid policy/preview and becomes **Preflight only** only
when the verified plan and `bectl`/`zfs` prerequisites are present. No package
should be installed, removed, or upgraded during this validation.
