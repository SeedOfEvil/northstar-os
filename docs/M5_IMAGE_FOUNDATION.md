# M5 reproducible image foundation

PR75 starts M5 without granting pull-request code access to disks, root image
builders, or signing keys. It establishes the immutable inputs that PR76 will
consume when assembling the first UEFI/ZFS QCOW2 image.

## Locked inputs

The QCOW2 lock records:

- FreeBSD `15.1-RELEASE`, amd64, UEFI, and ZFS;
- the official release `MANIFEST` digest plus `base.txz` and `kernel.txz`
  names, byte sizes, and SHA-256 digests;
- the accepted Northstar `0.1.4` package SHA-256;
- Northstar source commit and signed repository revision 78;
- catalogue, publication metadata, and signing fingerprint digests; and
- a fixed source-date epoch from the FreeBSD release build.

The official FreeBSD `MANIFEST` is the source for release-set hashes. Artifacts
remain outside Git beneath `.artifacts/`.

## Safe preparation

`image/scripts/prepare-image-inputs.sh` has two modes. `--check-lock` validates
the repository contract without artifacts. Normal mode verifies already staged
files and atomically creates a read-only input lock, sorted artifact records,
and resolved image-input manifest. Normal mode requires a clean Git checkout;
its actual `HEAD` must equal the explicit project commit written to the output.
The output also hashes both the lock and artifact record.

The script deliberately has no fetch, extraction, mount, `mdconfig`, `gpart`,
ZFS, package-mutation, conversion, or signing command. Existing output paths
are immutable and rejected.

## Acceptance

PR75 passes when local and native FreeBSD tests prove lock validation,
deterministic output, immutable output refusal, and rejection of tampered,
unresolved, duplicate, unknown, missing, symlinked, or incorrectly sized
inputs. It does not close the M5 image gate.

PR76 will add the disposable privileged assembler, UEFI GPT/root-on-ZFS layout,
QCOW2 conversion, checksum/provenance output, and QEMU/Proxmox boot smoke.
