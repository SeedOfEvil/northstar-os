# M5 image-input validation - 2026-08-10

## Scope

PR75 establishes the unprivileged input boundary for the first Northstar
FreeBSD 15.1 amd64 UEFI/ZFS QCOW2 image. It does not assemble, mount, boot, or
publish an image.

## Locked provenance

- FreeBSD release: `15.1-RELEASE`, amd64.
- Official release manifest SHA-256:
  `70ba0347054099662f22434797f1f33be289fc868ba58d6069a490b3e395d684`.
- `base.txz`: 164624792 bytes, SHA-256
  `3768988b151c20f965679062b065c63a977d6bbb9f47fd83695ec2c40790c18f`.
- `kernel.txz`: 45113908 bytes, SHA-256
  `49fea8ee25d0b1c523a02309efe4711d44a21ed0b36fb4191936a27a09324313`.
- Northstar package: `northstar-0.1.4-amd64.pkg`, 7084952 bytes,
  SHA-256
  `6b2adc9a0b67049f1c4a9a97fc75aea63d5773f3fa92bcbf74eb5f553c4c2601`.
- Accepted package source: commit
  `017fc81040bb33879596b6a3dde630212e30524f`, signed development repository
  revision 78.

## Native FreeBSD evidence

The exact pushed implementation commit `c577d8e8b9f494c5a05977c22ea278374e3572f6`
was cloned into a disposable `/tmp` checkout on NSTAR-DEV01. The native fixture
proved pinned-lock acceptance, deterministic output, immutable-output refusal,
and rejection of tampered, unresolved, duplicate, unknown, incorrectly sized,
and symlinked inputs. All existing shell/QML/integration contracts and all 22
CTest targets passed from a separate disposable Release build.

A final exact-commit rehearsal exposed provenance drift when a syntactically
valid but incorrect project commit was supplied. The preparer now requires a
clean selected Git checkout and refuses a declared commit that differs from
its actual `HEAD`; the regression fixture covers both mismatch and dirty-tree
rejection.

The real official release sets and accepted r78 package were staged in `/tmp`
and prepared twice. Both outputs compared byte-for-byte equal:

- input lock SHA-256:
  `8cbede3724600ad2aa0323b68bc638b7ff0d3221f0ad9fc6e8536084c80200a3`;
- artifact records SHA-256:
  `381a035771808251b5083024ac34a9794a93fcd535d4eed9c1e7ebe220e0cb5b`;
- resolved-input manifest SHA-256:
  `d8796344eb5f9f3ec19fe07c74c7da2e6df4e0753f435323fe420902a8e05bb3`.

No disk image, mount, memory disk, partition table, ZFS pool, package mutation,
or signing key was created or accessed by the preparation script. The accepted
PR74 deployment audit remained separate and untouched.

## Deferred gate

ShellCheck was unavailable on NSTAR-DEV01; POSIX `sh -n` and native execution
passed. PR76 remains responsible for a disposable privileged builder, UEFI GPT
and root-on-ZFS assembly, QCOW2 conversion, checksums, and boot validation.
