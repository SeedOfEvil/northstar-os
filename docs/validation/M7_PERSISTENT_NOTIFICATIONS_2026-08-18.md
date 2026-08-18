# Persistent notification history — 2026-08-18

PR #100, validated on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2, Proxmox/noVNC),
built from commit `9d38598` in `/home/northstar/builds/pr100-9d38598`.

This is the last planned M7 slice.

## What changed

The notification centre held its history in memory only. Every login started
with an empty panel, and an unread badge died with the session.

`NotificationStore` now writes the history as a `QSettings` INI array to
`~/.config/northstar-shell/notifications.ini`. The centre loads it on
construction and rewrites it on every mutation — push, mark read, mark all
read, dismiss, and clear.

## Security shape

- The file is written with owner-only permissions. Notifications name which
  applications this account launches and when, so another account on a shared
  machine must not be able to read them. A test asserts no group or other
  access, matching the text editor's recent-document store.
- Nothing executable is stored or replayed. The store holds six plain fields
  and the shell only ever renders them as text.
- Reading is deliberately forgiving. A record with no identifier, no title, or
  an unparseable timestamp is skipped, and a `kind` that is not one of the
  four known values is normalised to `info`. A truncated or hand-edited file
  therefore cannot stop the shell from starting.
- Entries older than the 14-day retention window are dropped at load, so the
  file cannot grow without bound across months of use.

## The defect this slice most needed to avoid

Restored entries keep the identifiers they were dismissed by. If the centre
had gone on counting from 1 at each start, a fresh notification could be
issued an identifier that a restored entry already held — and dismissing the
new one would silently have removed the old one instead. The centre now
derives its next identifier from the highest one it loaded.
`neverReissuesARestoredIdentifier` pins this.

## Two defects found in review before they shipped

Neither would have been caught by any gate in the repository.

1. **The tests would have written the real desktop's history.** Every existing
   `NotificationCenter` construction used the default path, which becomes a
   real history file once the constructor persists. All call sites in
   `test-notificationcenter.cpp` and `test-desktopsettings.cpp` now pass a
   `QTemporaryDir` path.
2. **The hover that reveals the exact time could never fire.** The row-wide
   mouse area is declared after the age label, so it stacked on top of it. The
   label now carries an explicit `z`, and either hover holds the row
   highlight so moving onto it does not blink the highlight off. The label
   still accepts no buttons, so a click continues to fall through and mark the
   entry read. Fixed in `9d38598`.

## A build-hygiene defect found while deploying

The first build of this branch used `-DCMAKE_BUILD_TYPE=RelWithDebInfo`, and
the first 34/34 test run was against that tree. Running `make install-user`
then reconfigured the same tree to the project's canonical `Debug` with
`-DBUILD_TESTING=ON`, because `install-user` depends on `build`, which depends
on `configure`. That started a full 338-target rebuild underneath the tree the
evidence had just been collected from, and it died with the ssh session, so
nothing was installed at all.

Two things were wrong: the evidence described a configuration the project does
not build, and an install step was silently able to rebuild the artifacts it
was supposed to be installing. The tree was wiped and rebuilt with the
canonical flags, the whole suite was re-run against it, and the install is now
done with `cmake --install` straight from the tested tree so it cannot
reconfigure anything. Every number below is from the `Debug` tree that was
actually installed.

## Automated evidence

Run natively on DEV01 against the commit-named build tree, configured with the
project's canonical `-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.

- Clean build, all 394 targets, 0 errors, no warnings in the changed sources.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr100-9d38598 --output-on-failure`
  — **34/34 suites passed, 0 failed**. The count rises from 33 with the new
  store suite.
- `northstar-notificationstore` — **10 cases passed, 0 failed, 0 skipped**:
  owner-only permissions, a full field round trip, the requested limit,
  retention including a file stamped in the future, malformed records, a
  hand-edited `kind`, shrinking the file when history shrinks, and an absent
  file.
- `northstar-notificationcenter` — **12 cases passed, 0 failed, 0 skipped**,
  up from 4. The new cases cover restoration across a restart with read state
  intact, identifier continuity, persisted dismissal and clearing, restoring
  no more than the configured cap, the age wording, and the `displayTime` the
  panel renders.
- `northstar-desktopsettings` — **11 cases passed, 0 failed, 0 skipped**.
- `sh tests/unit/test-qml-surfaces.sh` — passed. Two new contracts pin the age
  label and its hover text. Both were verified to fail when the contract they
  pin is removed and to pass again when restored, so they are not decorative.
- `sh tests/unit/test-session-entrypoint.sh /home/northstar/builds/pr100-9d38598`
  — passed. Note that this script takes the build directory as its first
  argument; invoking it bare makes it look for `build/` inside the canonical
  checkout, which is how a 270 MB tree was once created there by accident.
- `git diff --check` — exit 0.

## Evidence against the live session

Recorded from the running desktop rather than from any test, because PR #99
showed that passing every gate says nothing about whether a change reached the
session.

- The installed binary carries the new code: `nm -C ~/.local/bin/northstar-shell`
  reports 8 `NotificationStore` symbols and `NotificationCenter::relativeTime`,
  and `strings -e l` finds the `notifications.ini` literal. A plain `strings`
  does not, because Qt stores `QStringLiteral` data as UTF-16 — worth knowing
  before treating a bare `strings` result as evidence of absence.
- Build-tree and installed binaries differ by hash (`bfb82861` against
  `11c32ce8`) because CMake rewrites RPATH on install, so hash equality is not
  the right identity check here. The installed hash did change from the
  previous build's `bd2f4f88`, which is what shows the install landed.
- Started against a hand-written history file containing a record with no
  identity and a record with a `kind` of `nonsense`, the installed shell
  self-test exits 0 with no crash indicators. The forgiving read path
  therefore works in the deployed binary, not only in the unit tests.
- The shell was restarted under its supervisor with `SIGTERM` (PID 22341 to
  26439). The compositor stayed up throughout, so this was a shell-only
  restart and did not end the session.
- PR #99's compositor shortcut is not regressed: `northstar-shell-command
  toggle-search` run twice from the compositor's own environment returns exit
  0 both times, against a freshly created `srwx------`
  `northstar-shell-wayland-1.sock`.
- `~/.config/northstar-shell/notifications.ini` is correctly absent at this
  point: the file is written on the first mutation, and no producer has fired
  since the restart. Its creation and permissions are the first item on the
  interactive checklist.

The shell self-test emits 7 QML `TypeError` lines in offscreen mode. The same
7 appear from the pre-change `pr99-1480ec4` build, so they are pre-existing
self-test artifacts of unset context properties and are not introduced here.
They are worth fixing on their own, not in this slice.

## Interactive acceptance

Pending Hector's 1280x800 noVNC checklist.

## Not claimed

`make validation-deployment-audit` is not claimed to pass. DEV01's root-owned
manifest at `/usr/local/etc/northstar/validation-deployment.conf` still
describes PR74/r78, unchanged from PR #95 through #99. This is the sixth
consecutive slice to record it as an exception; it should be repaired rather
than noted a seventh time.

The stale `/usr/local/bin/northstar-session-x11` dated 2026-08-10 is still
present and still shadows the `~/.local` copy at login. It cannot affect this
change, and removing a system-wide file remains a separate deliberate
operation.

Physical Intel/AMD DRM/KMS acceptance, native compositor quality, and
multi-display evidence remain deferred. The Proxmox scfb/pixman VM remains
supplemental product evidence only.
