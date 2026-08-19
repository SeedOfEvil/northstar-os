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
- `~/.config/northstar-shell/notifications.ini` was absent until the first
  notification was produced, then created as
  `-rw-------  northstar northstar`. The neighbouring `preferences.ini` in the
  same directory is `-rw-r--r--`, so the tighter mode is this store's own
  doing and not inherited from the directory.

The shell self-test emits 7 QML `TypeError` lines in offscreen mode. The same
7 appear from the pre-change `pr99-1480ec4` build, so they are pre-existing
self-test artifacts of unset context properties and are not introduced here.
They are worth fixing on their own, not in this slice.

## Interactive acceptance

Completed by Hector at 1280x800 over noVNC, against the installed build. The
shell was restarted four times during the run with `SIGTERM` under its
supervisor; the compositor stayed up throughout, so each was a shell-only
restart rather than a new login.

The decisive observations were taken from the history file at each step, so
what follows is what the desktop actually stored, not a description of what it
should have stored.

1. **History survives a restart.** A Firefox launch produced
   `notification-1`. After a restart the panel still showed it, and the file
   was unchanged byte for byte. Before this change the panel came back empty.
2. **Read state survives with it.** The restored entry kept `read=true`, so no
   badge was resurrected for an entry the user had already seen.
3. **Identifiers do not collide after a restart.** A Northstar Welcome launch
   in the restarted shell was issued `notification-2` alongside the restored
   `notification-1`. This is the failure the slice was most concerned with:
   had the counter reset, the new entry would have reused `notification-1`.
4. **Dismissal removes the right entry and stays removed.** Hector dismissed
   the Welcome entry. The survivor was `notification-1`, Firefox — the
   restored one — and it was still there after a restart. Under a colliding
   identifier the row count would have looked correct while the wrong entry
   disappeared, which is why the surviving entry was checked by identity
   rather than by count.
5. **The file shrinks rather than leaving stale records.** `size` went from 2
   to 1 on dismissal with no orphaned keys, confirming
   `shrinksWhenTheHistoryShrinks` against real data.
6. **Clearing persists.** After Clear the file held `[notifications]` and
   `size=0` with no residual records, and the panel was still empty after a
   restart.
7. **Settings.** The searchable **Notification history** entry resolved and
   read back the live count and retention window.
8. **No regression in PR #99's shortcut.** `Ctrl+K` still opens unified
   search from a real keypress, not only from the command-line probe.

Hector's summary across the run: "all looks good" and "works!".

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
