# ADR 0009: Image input and privileged builder boundary

Status: Accepted

## Context

M4 proves signed package update and ZFS rollback, allowing M5 image work to
begin. Image assembly combines untrusted downloads, root-owned disk devices,
partitioning, filesystems, package installation, and release signing. Mixing
input discovery with privileged mutation would make reproducibility difficult
to audit and could expose persistent builders or unrelated host disks.

## Decision

Northstar separates image production into two boundaries.

The first boundary is unprivileged and non-mutating. A strict repository-owned
lock pins the FreeBSD release, architecture, firmware, root filesystem,
official release-set URLs/names/sizes/hashes, accepted Northstar package hash,
package source revision, repository revision, catalogue/metadata hashes, and
signature fingerprint. The current project commit is supplied explicitly and
must match `HEAD` in a clean selected Git checkout when artifacts are verified.
Preparation writes a new immutable resolved-input
directory atomically and refuses unresolved, duplicate, unknown, symlinked,
missing, incorrectly sized, or digest-mismatched inputs.

The second boundary, beginning in PR76, consumes only a passed resolved-input
record on a disposable FreeBSD builder. It owns image creation, UEFI GPT
partitioning, ZFS installation, package configuration, conversion, checksums,
and boot smoke tests. Public pull-request jobs never reach that privileged
builder or release signing keys.

## Consequences

- Input validation can run safely in normal developer and CI accounts.
- Downloading and disk mutation cannot silently change the locked inputs.
- A changed package or release set requires a new lock and review.
- PR75 does not claim that a bootable Northstar image exists.
- Builders must create new output paths and record their exact project commit.

## Alternatives considered

Using a moving FreeBSD `Latest` VM image was rejected because contents could
change without review. Performing downloads inside a root assembly script was
rejected because it couples network trust to disk mutation. Customizing the
long-lived development VM in place was rejected because it is not a clean or
disposable image builder.

## Validation

The input fixture gate proves accepted locks and artifacts produce stable
records, immutable output paths cannot be replaced, and tampered artifacts,
unresolved values, duplicate keys, unsafe names, and private key material are
rejected.
