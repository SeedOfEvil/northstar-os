# M5 installer source trust and recoverable journal

PR82 binds the protected installer preflight to authenticated release media
and makes staged state recoverable. It remains a non-destructive routine slice:
there is no partitioning, extraction, filesystem creation, or bootloader work.

## Trusted source contract

Production source media is mounted read-only by the future release environment
at `/var/run/northstar-installer/source`. It contains exactly:

- `source-manifest.conf`, a bounded nine-field manifest;
- `source-manifest.conf.sig`, its detached RSA/SHA-256 signature; and
- one path-safe `.txz` payload named by the manifest.

The manifest binds FreeBSD `15.1-RELEASE`, `amd64`, a project commit, payload
name/size/SHA-256, and runtime-manifest SHA-256. The fixed verifier trusts only
`/usr/local/share/northstar/installer/source-signing.pem`. The public key is
provisioned by the protected release process; the private key never enters the
repository, runtime image, installer media, or pull-request runner.

Source directories and files must be root-owned and not group/world writable.
The manifest, signature, key, and payload are size-bounded. The reviewed
manifest digest, detached signature, payload size, and payload digest all pass
before the engine revalidates the selected target.

## Recoverable state

Successful staging creates:

```text
/var/db/northstar/installer/
  active.conf
  transactions/<transaction-id>/
    request.conf
    transaction.conf
    journal.log
  archive/
```

Files are root-owned mode 0600 and directories are mode 0700. The initial
journal sequence records request validation, source verification, target
revalidation, and transaction staging. The transaction explicitly records
`execution=disabled` and `recovery=resume-or-abandon-required`.

Authenticated `--status` reports idle, staged, interrupted, or legacy-blocked
state. If publication was interrupted before `active.conf` became durable,
`--recover <id>` restores the active pointer and appends a journal event.
`--abandon <id> --confirm` removes the active pointer, appends an abandonment
event, and archives the complete transaction without touching any disk.

## Routine validation

```sh
make installer-source-test
make installer-engine-test
make installer-disk-test
make qml-surface-test
make build
ctest --test-dir build --output-on-failure
```

The signing tests create temporary keys outside the fixture source. Engine
tests use a temporary root plus fake GEOM, mount, ZFS, swap, source-verifier,
and state tools. DEV01 runs the same native contracts and only read-only disk
discovery; its system disk remains ineligible and is never staged.

## Deferred image acceptance

Image checkpoint: M5 Installer Release Candidate

Image status: DEFERRED

Routine evidence: exact signed-source parsing, detached-signature and payload
verification, tamper rejection, fixed production paths, source-before-target
staging, root-owned journal state, interrupted-state detection, explicit
recovery and archival abandonment, and static no-disk-mutation enforcement.

Deferred evidence: protected public-key provisioning on release media,
read-only media mounting, final pre-mutation source and target revalidation,
GPT/UEFI/ZFS execution on a disposable destination, payload extraction,
bootloader installation, interruption during destructive phases, installed
boot, and preservation of every non-target disk.

The routine native evidence is recorded in
[`validation/M5_INSTALLER_SOURCE_JOURNAL_2026-08-12.md`](validation/M5_INSTALLER_SOURCE_JOURNAL_2026-08-12.md).
