# ADR 0017: Installer-media assembly boundary

Status: Accepted

## Context

Northstar has guarded installer target, source, execution, retry, diagnostics,
and boot-environment contracts, but those contracts need reproducible media.
Treating the accepted QCOW2 itself as installer media would provide no signed
rootfs source and no isolated installer session. Letting a build script accept
an arbitrary host disk would also create an unnecessary destructive boundary.

## Decision

The production QCOW2 builder emits a matching rootfs payload and runtime
manifest before any installer-media-only state exists. A separate unprivileged
preparer verifies that binding and signs a strict source manifest with an RSA
private key that resolves outside the project checkout. Only the derived
public key and detached signature enter release outputs.

A second script accepts only the exact QCOW2, image provenance, and signed
source directory. Its preflight is read-only and non-root. Production assembly
requires FreeBSD root plus a protected marker that names the exact project
commit and disposable builder. It converts the QCOW2 into a new raw file,
attaches only that file, and adds a labeled UFS source partition. It has no
host-disk destination option.

The live image receives a dedicated passwordless `northstar-installer`
identity with no reusable credential, plus an SDDM session, fixed trust key,
source mount, and execution marker. A media-only PolicyKit rule authorizes only
the fixed stage, execute, and recovery actions for that local active identity.
This removes an unusable password prompt in the live session without weakening
target confirmation, source verification, protected journaling, or
exact-device revalidation.

The installable rootfs payload is captured first. Therefore the dedicated
identity, autologin, PolicyKit rule, source trust key, media hostname, and
execution marker are absent from the installed target by construction.

## Consequences

Routine pull requests can verify media inputs and boundaries without root or
disk mutation. Actual raw assembly remains restricted to a disposable builder,
and physical-media writing stays outside the project assembler. Accepted
artifacts have a direct commit, payload, runtime-manifest, image, signature,
builder, and SHA-256 provenance chain.

PR86 cannot claim that the generated media boots or installs successfully.
Those destructive and graphical checks remain deferred to the integrated M5
Installer Release Candidate checkpoint.
