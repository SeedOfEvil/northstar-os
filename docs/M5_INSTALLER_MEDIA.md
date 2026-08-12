# M5 installer media

PR86 establishes the reproducible boundary that turns an accepted production
Northstar QCOW2 into raw USB installer media. It does not write a physical
disk and does not claim boot or installation acceptance before the named M5
Installer Release Candidate checkpoint.

## Artifact pipeline

The pipeline has three immutable stages:

1. `assemble-qcow2-image.sh` builds the production QCOW2 and, before adding
   any live-installer identity or privilege, emits the matching
   `northstar-rootfs-v1-<commit>.txz` and `runtime-manifest.conf`.
2. `prepare-installer-source.sh` verifies that the payload contains that exact
   runtime manifest, derives a public key from an external RSA private key,
   signs the bounded source manifest, and records source provenance.
3. `assemble-installer-usb.sh` verifies the accepted image, image provenance,
   signed source, runtime manifest, and matching project commit. On a marked
   disposable FreeBSD builder it converts the QCOW2 to a new raw file and adds
   a UFS partition labeled `NSTAR_SOURCE`.

The source partition is mounted read-only by label at
`/var/run/northstar-installer/source`. The fixed verifier, installer engine,
and guarded executor consume it there. The raw assembler accepts no disk
device argument; copying the final image to removable hardware is a separate,
explicit operator action.

## Prepare signed source

Signing keys stay outside the repository, pull-request environment, QCOW2,
raw image, and signed-source output. Starting from an accepted image output:

```sh
image/scripts/prepare-installer-source.sh \
  --payload /release/northstar-rootfs-v1-<commit12>.txz \
  --runtime-manifest /release/runtime-manifest.conf \
  --project-root /release/northstar \
  --project-commit <full-commit> \
  --signing-key /release-secrets/northstar-installer-source.pem \
  --output /release/northstar-installer-source-<commit12>
```

The project checkout must be clean and exactly match the full commit. The
private key path must resolve outside that checkout. The output contains only
the payload, reviewed runtime manifest, public trust key, signed manifest,
detached signature, and provenance.

## Verify without mutation

Preflight is suitable for the routine validation lane. It performs digest,
schema, signature, provenance, capacity, and read-only `qemu-img` checks. It
creates no output and does not require root:

```sh
image/scripts/assemble-installer-usb.sh \
  --image /release/northstar-15.1-amd64.qcow2 \
  --image-provenance /release/image-provenance.conf \
  --installer-source /release/northstar-installer-source-<commit12> \
  --output /release/northstar-installer-usb-<commit12> \
  --preflight
```

## Assemble on a disposable builder

Production assembly is root-only on FreeBSD and requires a root-owned mode
`0400` or `0600` marker, normally
`/etc/northstar/disposable-installer-media-builder.conf`:

```text
SCHEMA_VERSION=1
PURPOSE=northstar-disposable-installer-media-builder
ALLOW_INSTALLER_MEDIA_ASSEMBLY=YES
EXPECTED_PROJECT_COMMIT=<full-commit>
BUILDER_ID=<recorded-builder-id>
```

Run the same command without `--preflight`. The assembler creates and attaches
only its new raw file through `mdconfig`, requires the expected third partition
provider, injects the signed-source mount and installer session, exports the
ZFS pool, detaches the file-backed device, verifies the source QCOW2 remains
unchanged, and emits SHA-256 plus media provenance.

The media session uses a dedicated passwordless `northstar-installer` identity
that exists only in the live image and has no reusable credential. A media-only
PolicyKit rule permits that local active identity to invoke only the three
fixed Northstar installer actions. Device discovery, eligibility checks,
typed whole-disk confirmation, protected transaction state, source
verification, and executor revalidation still apply. The rootfs payload is
captured before this identity, session, rule, trust key, and execution marker
are added, so an installed target cannot inherit live-media authorization.

## Acceptance status

Image checkpoint: **M5 Installer Release Candidate**

Image status: **DEFERRED**

Routine PR86 evidence covers signed-source construction, tamper rejection,
strict provenance binding, disk-device-free preflight, static mutation
boundaries, and native FreeBSD project gates on DEV01. The checkpoint must
still build the new production QCOW2 and rootfs payload on a clean disposable
builder, assemble the raw media, boot a fresh disposable VM from it, perform a
real confirmed installation to a separate target disk, validate first boot,
and exercise interrupted-install recovery.
