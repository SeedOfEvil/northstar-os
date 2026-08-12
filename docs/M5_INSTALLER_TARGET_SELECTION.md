# M5 installer target selection

This routine M5 slice introduces the branded installer destination flow while
keeping all disk operations read-only.

## Current capability

- Enumerate whole FreeBSD disk devices without root.
- Show capacity and a bounded device description.
- Mark disks used by mounted filesystems, ZFS, or swap as the current system
  and prevent their selection.
- Reject destinations below the 16 GiB minimum.
- Require the exact device name and an explicit permanent-erasure
  acknowledgement.
- Produce a GPT, UEFI, and root-on-ZFS review plan.

The final action remains visibly disabled. This slice does not run `gpart`,
`newfs`, `zpool create`, `dd`, package installation, or file copying.

## Routine validation

```sh
make installer-disk-test
make qml-surface-test
make build
ctest --test-dir build --output-on-failure
```

The disk helper test uses fake read-only FreeBSD commands. It does not inspect
or mutate a real disk. A DEV01 visual check may exercise selection and review
using a fixture discovery command or a deliberately attached empty test disk,
but no execution boundary exists in this PR.

## Deferred image acceptance

Image checkpoint: M5 Installer Release Candidate

Image status: DEFERRED

Routine evidence: native build and tests, bounded discovery fixtures,
controller confirmation tests, offscreen QML loading, and non-destructive
DEV01 presentation review.

Deferred evidence: boot installer media, independently revalidated physical
target identity, destructive confirmation, GPT/UEFI/ZFS creation, interrupted
installation recovery, installed-system boot, and preservation of every
non-target disk.
