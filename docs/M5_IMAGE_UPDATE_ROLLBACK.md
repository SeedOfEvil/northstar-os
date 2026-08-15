# M5 installed-image update and rollback

PR89 closes the remaining update/rollback portion of the M5 Installer Release
Candidate gate. It does not reuse `NSTAR-DEV01` for destructive evidence.
DEV01 builds and publishes candidate `0.2.6` in signed repository revision 86;
the accepted r85 installation on VM 104 performs the actual package and ZFS
boot-environment mutations. The installer media is not rebuilt for this gate.

## Safety boundary

The production gate is
`/usr/local/libexec/validate-image-update-rollback.sh`. A copy from the exact
candidate source is staged beneath `/home` so the same driver survives both
root-dataset rollbacks. Preparation binds its state to the accepted image
commit, repository revision, candidate source, catalogue digest, and signing
fingerprint. It refuses to run unless all of these are true:

- the host is FreeBSD and the caller is root;
- `/var/db/northstar/image-build.conf` identifies an assembled image;
- the state path is beneath `/home`;
- `/home` and `/` are different datasets; and
- `/var` and `/` are the same boot-environment dataset; and
- `pkg`, `bectl`, and the fixed Northstar transaction runner are executable.

The state directory is root-owned mode `0700`; state and the home sentinel are
mode `0600`. The sentinel and phase record survive root boot-environment
rollback because they reside on the separate home dataset.

Never run the production phases on `NSTAR-DEV01`, a workstation, or an
unprotected installed system. VM 104 must have a current Proxmox snapshot of
the accepted r85 installation before staging begins.

## Acceptance sequence

Publish signed repository revision 86 containing Northstar `0.2.6`. As root on
VM 104, stage that immutable repository and the exact PR89 gate without package
or boot-environment mutation:

```sh
/path/to/stage-installed-image-update-candidate.sh \
  --repository /path/to/development-channel-r86 \
  --gate /path/to/validate-image-update-rollback.sh \
  --baseline-version 0.2.5 --candidate-version 0.2.6 \
  --image-commit d561e06519cd78aef9e2918fadd22fc3fe0ee4d1 \
  --repository-revision 86 --candidate-source PR89_COMMIT
```

Use the catalogue digest and fingerprint printed by that staging command for
the bound preparation phase, then run:

```sh
sudo /home/.northstar-update-candidate/validate-image-update-rollback.sh \
  --prepare --baseline-version 0.2.5 --candidate-version 0.2.6 \
  --image-commit d561e06519cd78aef9e2918fadd22fc3fe0ee4d1 \
  --repository-revision 86 --candidate-source PR89_COMMIT \
  --catalogue-sha256 CATALOGUE_SHA256 \
  --signature-fingerprint SIGNATURE_FINGERPRINT

sudo /home/.northstar-update-candidate/validate-image-update-rollback.sh --inject-failure
sudo shutdown -r now

sudo /home/.northstar-update-candidate/validate-image-update-rollback.sh --verify-failure-recovery
sudo /home/.northstar-update-candidate/validate-image-update-rollback.sh --normalize-after-failure

sudo /home/.northstar-update-candidate/validate-image-update-rollback.sh --apply-update
sudo /home/.northstar-update-candidate/validate-image-update-rollback.sh --schedule-rollback
sudo shutdown -r now

sudo /home/.northstar-update-candidate/validate-image-update-rollback.sh --verify-rollback
```

The injected failure uses a private package wrapper that permits read-only
queries and fails only the upgrade operation. The transaction runner must
create and activate the verified pre-update boot environment. After reboot,
the gate verifies the original package and home sentinel, then normalizes the
disposable boot-environment names before the successful lane.

The successful lane performs the real signed repository transaction, verifies
the candidate package, schedules explicit rollback through the same fixed
transaction runner, reboots, and verifies the baseline package and unchanged
home sentinel. The terminal record is:

```text
stage=passed
```

## Evidence and cleanup

Record the accepted r85 installer and installed-image identity, baseline and
candidate package hashes, repository revision and catalogue digest, source
commit, each command output, `bectl list -H` before and after each reboot,
installed package versions, and the final schema-2 state file. Do not include
signing keys or authentication material.

After evidence is copied out and PR89 is accepted, remove the temporary
candidate staging tree and transaction boot environment, retain only the
accepted deployment plus its immediate predecessor, run guest TRIM, and remove
the Proxmox snapshot. Preserve the accepted r85 installer artifact; do not
replace it with the mutated validation disk.
