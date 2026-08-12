# M5 boot-environment recovery

Northstar Recovery is the user-facing bridge between transactional updates and
reboot-time recovery. It intentionally exposes less power than `bectl`.

## Product behavior

- The application lists the boot environment active now, the environment
  selected for next boot, creation time, and reported space.
- Only names produced by Northstar's verified update contract are selectable.
- Scheduling requires typing the exact recovery-point name and completing
  administrator authentication.
- Success is shown only after a second inventory proves the target is selected
  for next boot.
- The application never restarts automatically. The user saves work and uses
  Northstar's existing restart action when ready.
- A bounded diagnostic report can be saved under the user's Documents folder.
  It contains inventory metadata, not command output, logs, private paths, or
  credentials.

System/operator boot environments are visible for orientation but disabled.
The surface has no create, delete, rename, mount, clone, snapshot, package, or
dataset controls.

## Routine validation

Run:

```sh
make boot-environment-recovery-test
make qml-surface-test
cmake --build build
env QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

The helper test uses an unprivileged fake `bectl` and proves bounded inventory,
namespace enforcement, exact confirmation, verified activation, idempotence,
and unsafe-record rejection. The Qt test proves strict model parsing,
selection, activation state, and privacy-bounded diagnostic export. It does not
modify a real boot environment.

## Deferred image gate

Image checkpoint: **M5 Installer Release Candidate**

Image status: **DEFERRED**

At that checkpoint, use a disposable root-on-ZFS installation to create a
known recovery point, schedule it through the graphical application, reboot,
prove the selected environment became active, verify the desktop and user home
data, and then return to the accepted release environment. No such mutation is
allowed on `NSTAR-DEV01`.
