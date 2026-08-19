# Date and time — 2026-08-19

PR #108, verified on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2, Qt 6.11.1, Clang
19.1.7) at commit `3b6a131`, built in `/home/northstar/builds/pr108-3b6a131`
with the project's canonical `-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.

The timezone was set once by the first-boot provisioner and could never be
changed again from the desktop. Nothing offered network time at all. Date &
Time is now a settings section.

## What the surface offers

Settings gains a **Date & Time** section holding the current time as read, a
region, a timezone within that region, a switch for keeping the clock set from
the network, what the time daemon is doing, and a one-shot correction.

## The privilege boundary

Changes go through `northstar-clock`, in the same shape as `northstar-power`
and `northstar-radio`. Two of its three actions take enumerated words only.

The third could not. The zoneinfo database holds hundreds of names, so
`timezone` cannot be an enumerated word list, and this is the first boundary in
the project that takes a value rather than a choice. It is therefore validated
**inside** the boundary, against the same conservative shape the first-boot
provisioner already enforces, and then against the database itself:

```sh
printf '%s' "$zone" \
    | grep -Eq '^[A-Za-z0-9][A-Za-z0-9._+-]*(/[A-Za-z0-9][A-Za-z0-9._+-]*)*$' \
    || exit "$usage_error"
[ -f "$ZONEINFO/$zone" ] || exit "$unavailable"
```

Every component must begin with a letter or digit, which is what keeps `..`, a
leading `/`, and a leading `-` out. Nothing the caller sends is expanded by a
shell or reaches an interpreter. The controller independently resolves the name
against the database and refuses anything that escapes it, so a bad name is
rejected before the boundary is called as well as inside it.

### Measured on the machine

Run against the installed helper. None of these reach a privileged command:

| Argument | Exit |
| --- | --- |
| *(empty)* | 64 |
| `../../etc/passwd` | 64 |
| `/etc/passwd` | 64 |
| `-rf` | 64 |
| `America/Denver; rm -rf /` | 64 |
| `Mars/Olympus` | 69 |
| *(no argument)* | 64 |
| unknown action | 64 |
| `ntp maybe` | 64 |

`Mars/Olympus` is the interesting one: it is a well-formed name that this
system does not have, so it is refused as unavailable rather than as malformed.

`state` is read-only and deliberately unprivileged, so the shell can describe
the clock without asking for any authority at all. That is why the timezone and
daemon state are still reported on a system with no boundary installed, where
only the controls are read-only.

## The state this machine was actually in

DEV01 had a correct offset from `/etc/localtime` and an **empty**
`/var/db/zoneinfo`: the system knew what time it was but had no record of which
zone produced it. Rather than show a blank field, the controller reports the
zone as unrecorded and says that choosing one will record it, which setting a
zone then does.

## A guard catching a real defect, again

`registersNoDeadControls` — extended in PR #106 — failed on the first run of
this branch:

```
FAIL! : registersNoDeadControls() 'values.contains(...)' returned FALSE. (datetime.timezone)
```

It was right. With no recorded timezone the control reads back a value that is
not among its options, so it would open showing nothing selected and no reason
why, which is indistinguishable from a broken control.

Two fixes were available and both were wrong. Naming a zone the system is not
in would have been dishonest. Hiding the control removes the one thing that
state needs. So the unset state is **declared** instead, with `allowsUnset`,
and the rule keeps its force: a choice without it must still read back one of
its options, and even with it only *nothing* is permitted through — a value
that is neither empty nor offered is still a failure. Settings renders the
declared state as "Not set".

## Also in this change

The zone list depends on the chosen region and runs to hundreds of entries, so
the catalog gained an optional `optionSource`: a choice can be asked for its
values instead of carrying a fixed list. It is held to the same standard either
way, because a source that answers nothing at registration is refused exactly
as an empty list is.

## Automated evidence

- Clean build, all 408 targets, 0 errors.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr108-3b6a131 --output-on-failure`
  — **36/36 suites passed, 0 failed**.
- `northstar-clockcontroller` is new: 12 cases covering region listing with the
  `posix` and `right` copies excluded, zones nested a further level down
  (`America/Indiana/Knox`), region-less zones gathered under one name, index
  files never offered as zones, refusal of an absent zone and of a name that
  escapes the database, the unrecorded-timezone state, reading a recorded one,
  everything read-only without the boundary, writing through a stub boundary,
  a boundary that refuses, the network-time toggle, and refusal of a one-shot
  correction while the daemon is running.
- The suite was re-run **after** `cmake --install` into `~/.local` and still
  reported 36/36.
- `northstar-shell --qml-self-test` on the installed binary — exit 0, no QML
  warnings.
- The boundary refusal table above, measured on the installed helper.
- `git diff --check` — exit 0.

## Not claimed

**No privileged path was executed by the agent.** Every measurement above stops
before `sudo`. Changing a timezone, switching network time on, and correcting
the clock all require authority this session does not have, so they are
unverified until the interactive walkthrough runs them.

Manual time entry is **not** in this slice. It would need a date and time
editor rather than a catalog row, which is a surface of its own. The intent
behind it — control over what the clock says — is served by the one-shot
correction, which uses the servers this system is already configured with and
accepts no address from the caller. If a free-form clock set is still wanted
after using this, it should be its own slice.

Whether `ntpd` can actually reach a time server is a property of this network,
not of this code. A correction that fails for that reason reports it.

## Interactive acceptance

Status: **open**.
