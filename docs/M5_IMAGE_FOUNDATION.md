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

PR76 adds the disposable privileged assembler, UEFI GPT/root-on-ZFS layout,
offline exact-package installation, QCOW2 conversion, checksum/provenance
output, and a bounded snapshot-only QEMU boot smoke. The automated smoke
requires serial evidence for UEFI boot, ZFS root mount, rc startup, and the
multi-user login prompt. It deliberately does not claim graphical acceptance;
the branded greeter and Northstar shell remain a focused Proxmox/noVNC gate.

Run the boot smoke on a disposable FreeBSD builder with headless QEMU and EDK2:

```sh
image/scripts/boot-smoke-qcow2.sh \
  --image /path/to/northstar-15.1-amd64.qcow2 \
  --firmware-code /usr/local/share/edk2-qemu/QEMU_UEFI_CODE-x86_64.fd \
  --firmware-vars /usr/local/share/edk2-qemu/QEMU_UEFI_VARS-x86_64.fd \
  --output /new/boot-smoke-evidence
```

The command runs with `snapshot=on`, a private user network, TCG, and no host
port forwarding. It refuses existing output paths and records the source-image
and serial-log digests. The source QCOW2 and firmware templates remain
unchanged.

The normal image keeps the initial `northstar` account locked pending a future
first-boot account workflow. The explicit `--development-autologin` option
instead enables a passwordless local account so SDDM's FreeBSD PAM account
check can enter the compatibility session. This is recorded in image
provenance and is never enabled implicitly. OpenSSH continues to reject empty
password authentication.

The image also owns a separate `northstar-image-proxmox.desktop` entry. It
binds the accepted compatibility compositor in the image user's home and the
package-managed supervisor and shell in `/usr/local/bin`. Keeping this entry
separate avoids changing package-owned files while making the image independent
of older package wrappers that assumed a fully user-local installation.
