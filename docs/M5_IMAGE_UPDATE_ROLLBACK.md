# M5 installed-image update and rollback

PR77 closes the update/rollback portion of the first QCOW2 gate. It does not
reuse `NSTAR-DEV01` for destructive evidence. DEV01 builds and publishes the
candidate; a temporary VM imported from the accepted PR76 QCOW2 performs the
actual package and ZFS boot-environment mutations.

## Safety boundary

The production gate is
`/usr/local/libexec/validate-image-update-rollback.sh`. A copy from the exact
candidate source may be staged beneath `/home` when the baseline package does
not yet contain it. It refuses to run unless all of these are true:

- the host is FreeBSD and the caller is root;
- `/var/db/northstar/image-build.conf` identifies an assembled image;
- the state path is beneath `/home`;
- `/home` and `/` are different datasets; and
- `pkg`, `bectl`, and the fixed Northstar transaction runner are executable.

The state directory is root-owned mode `0700`; state and the home sentinel are
mode `0600`. The sentinel and phase record survive root boot-environment
rollback because they reside on the separate home dataset.

Never run the production phases on `NSTAR-DEV01`, a workstation, or the sole
copy of an image. Use a disposable VM whose source QCOW2 checksum matches the
accepted PR76 artifact.

## Acceptance sequence

Publish a signed candidate repository containing Northstar `0.1.5`, configure
the disposable baseline image to trust that repository, and stage the exact
PR77 gate script under `/home/northstar`. Then run:

```sh
sudo /home/northstar/validate-image-update-rollback.sh \
  --prepare --baseline-version 0.1.4 --candidate-version 0.1.5

sudo /home/northstar/validate-image-update-rollback.sh --inject-failure
sudo shutdown -r now

sudo /home/northstar/validate-image-update-rollback.sh --verify-failure-recovery
sudo /home/northstar/validate-image-update-rollback.sh --normalize-after-failure

sudo /home/northstar/validate-image-update-rollback.sh --apply-update
sudo /home/northstar/validate-image-update-rollback.sh --schedule-rollback
sudo shutdown -r now

sudo /home/northstar/validate-image-update-rollback.sh --verify-rollback
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

Record the source QCOW2 SHA-256, baseline and candidate package hashes,
repository revision and catalogue digest, source commit, each command output,
`bectl list -H` before and after each reboot, installed package versions, and
the final state file. Do not include signing keys or authentication material.

After evidence is copied out, delete the disposable VM and its test-only
signing material. Preserve the accepted PR76 QCOW2 and its offline input set;
do not replace that baseline artifact with the mutated validation disk.
