# M5 Installer Release Candidate

The M5 Installer Release Candidate is Northstar's integrated image checkpoint.
It closes the deferred claims accumulated by the first-boot, installer target,
source, execution, recovery, boot-environment, and media slices. It is a
milestone operation, not the routine validation lane.

## Release-candidate boundary

`image/scripts/assemble-installer-rc.sh` is the only top-level RC assembler.
It accepts immutable resolved image inputs, primary artifacts, an offline
runtime bundle, an exact clean project checkout, and an external private key.
It executes these stages in order:

1. build the production UEFI/GPT/root-on-ZFS QCOW2 and matching rootfs payload;
2. verify and sign the payload/source manifest without copying the private key;
3. convert the QCOW2 into a raw installer image with a read-only source
   partition and live-media-only installer authorization; and
4. emit a top-level `release-candidate.conf` binding all artifact digests and
   sizes to one project commit.

The driver accepts no disk device. Both child assemblers create only new
file-backed images. Production requires root on FreeBSD, a clean disposable
builder, a root-owned image-builder marker, a separate root-owned media-builder
marker, and a root-owned mode-`0400` or mode-`0600` signing key outside the
project checkout.

## Routine preflight

Preflight validates the immutable QCOW2 inputs and the external signing key
without creating output or requiring root:

```sh
image/scripts/assemble-installer-rc.sh \
  --resolved-inputs /release/resolved-inputs \
  --artifacts /release/primary-artifacts \
  --runtime-bundle /release/runtime-bundle \
  --project-root /release/northstar \
  --project-commit <full-commit> \
  --signing-key /release-secrets/installer-source.pem \
  --output /release/northstar-m5-installer-rc \
  --preflight
```

## Disposable builder

The builder must be a fresh disposable FreeBSD 15.1 amd64 VM with at least
four vCPUs, 8 GiB RAM, and 64 GiB free storage. Install build dependencies and
`qemu-img`; if `qemu-tools` conflicts with `qemu-guest-agent`, remove the guest
agent only on this disposable builder. Do not repurpose DEV01 as the privileged
builder.

Create these root-owned mode-`0600` markers with the exact RC commit:

```text
# /etc/northstar/disposable-image-builder.conf
SCHEMA_VERSION=1
PURPOSE=northstar-disposable-image-builder
ALLOW_IMAGE_ASSEMBLY=YES
EXPECTED_PROJECT_COMMIT=<full-commit>
BUILDER_ID=<builder-id>
```

```text
# /etc/northstar/disposable-installer-media-builder.conf
SCHEMA_VERSION=1
PURPOSE=northstar-disposable-installer-media-builder
ALLOW_INSTALLER_MEDIA_ASSEMBLY=YES
EXPECTED_PROJECT_COMMIT=<full-commit>
BUILDER_ID=<builder-id>
```

Generate the RC-only source key outside the checkout, set it root-owned mode
`0600`, and preserve only the public key, fingerprint, signatures, and accepted
artifacts after assembly. The private key is never transferred with the media
or committed.

Run the preflight first, then the same command without `--preflight`. A passed
output contains:

```text
northstar-m5-installer-rc/
  release-candidate.conf
  image/
    northstar-15.1-amd64.qcow2
    northstar-rootfs-v1-<commit12>.txz
    runtime-manifest.conf
    image-provenance.conf
  installer-source/
    source-manifest.conf
    source-manifest.conf.sig
    source-signing.pem
    installer-source-provenance.conf
  media/
    northstar-15.1-amd64-installer-usb.img
    media-provenance.conf
```

Run `qemu-img check` and the snapshot-only boot smoke against the QCOW2 before
transferring any artifact. Verify every SHA-256 again after transfer.

## Disposable Proxmox acceptance

Import the raw installer image as the boot disk of a new UEFI/q35 VM with
Secure Boot disabled and add a separate empty target disk of at least 32 GiB.
The installer media and target disk must remain visibly distinct. Never attach
DEV01's disk or any retained accepted-image disk as the installation target.

The acceptance sequence is:

1. boot the dedicated installer session at 1280x800;
2. verify source signature/provenance and target eligibility;
3. select only the disposable empty disk and enter the exact erasure
   confirmation;
4. inject one post-mutation failure, export sanitized diagnostics, reboot,
   and prepare a clean retry without resuming the failed transaction;
5. complete a fresh installation and detach the installer-media disk;
6. boot the installed target, complete first-administrator setup, and reach
   the branded greeter and desktop;
7. validate input, shell supervision, Files, Text Editor, Software Center,
   Recovery, shutdown, and absence of the media-only identity/rule/marker;
8. perform the signed N-1 to N update, injected-failure recovery, explicit
   boot-environment rollback, and `/home` preservation checks; and
9. record exact VM configuration, artifact hashes, screenshots, logs, journal
   summaries, package versions, and pass/fail results.

The PR remains open until the manual noVNC checks pass. Accepted RC artifacts
are retained by commit and SHA-256; failed outputs are quarantined with their
evidence and are never silently replaced.
