# Tests that depended on the machine running them — 2026-08-18

PR #104, verified on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2) at commit `59a05ee`,
built in `/home/northstar/builds/pr104-59a05ee` with the project's canonical
`-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.

Two defects of the same shape: a test whose result depended on the machine
rather than the code. The first depended on how *busy* the machine was, the
second on what was *installed* on it.

## The failure

During PR #103 a full `ctest` run reported 33/34. The failure was:

```
FAIL!  : FileBrowserControllerTest::copiesAndUndoesEntries() QTestLib: This test case
check (...notes.txt...) failed because the requested timeout (5000 ms) was too short,
6600 ms would have been sufficient this time.
```

The run had been started immediately after a build, so the machine was still
busy. The suite took 11.7 s there against 2.9–3.4 s idle, and passed three
times out of three when re-run on a quiet machine. The logic was never wrong.

## Why the number was wrong by construction

`QTRY_*` returns as soon as its condition holds. A generous timeout therefore
costs a passing run nothing; it only decides how long a genuine hang takes to
report. It is a **failure deadline, not an expected duration**.

The 5000 ms budget sat close to how long an asynchronous copy actually takes,
so any contention pushed the real duration past it. The named constants now say
what the value is for:

```cpp
constexpr int TransferTimeoutMs = 30000;
constexpr int WatchTimeoutMs = 10000;
```

## Every case was exposed, not just the one that failed

The same `5000` literal appeared **six times across five test cases**:
`copiesAndUndoesEntries`, `movesAndUndoesEntries`,
`resolvesPasteConflictsWithKeepBoth`, `keepsClipboardStableDuringTransfer`, and
`copiesFromMountedLocationButRejectsCut`. The filesystem watch in
`listsDesktopEntriesForTheDesktopSurface` had the same shape of risk at
2000 ms.

This is not a theoretical concern. See the control run below: under load the
pre-fix binary failed on **`keepsClipboardStableDuringTransfer`**, a different
case from the one that originally failed. Repairing only the line named in the
original failure would have left the flake alive and moved it somewhere else.

## Proving the fix rather than assuming it

A timeout raised until a test stops failing is a number, not a fix. The
pre-fix binary from PR #103 was kept on the machine and used as a control, so
both were run under identical load.

**First attempt, and why it proved nothing.** A parallel rebuild at 8 jobs
reached load average 4.08. Both binaries passed, and the suite took 3515 ms —
barely above idle. That run demonstrated only that a mild load does not trigger
the flake; it was not evidence of a fix, and is recorded here because reporting
it as one would have been false.

**Second attempt.** CPU oversubscribed with a 16-job rebuild on 8 vCPU, plus
six concurrent `dd` writers saturating `zroot/tmp`, which is the pool the
temporary directories actually live on. The first attempt had been CPU-heavy
and I/O-light, which is why it missed. Load average reached 10.26.

| Binary | Wait budget | Result under identical load |
| --- | --- | --- |
| PR #103, pre-fix | 5000 ms | **FAIL** — `keepsClipboardStableDuringTransfer`, 23 passed / 1 failed, 21135 ms |
| PR #104, fixed | 30000 ms | **PASS** — 24 passed / 0 failed, 25463 ms |

The control fails and the fixed binary passes under the same conditions, which
is the evidence that was missing from the first attempt.

## Margin

Under that deliberately pathological load the whole suite took 25.5 s, which is
the sum across all twenty-four cases; no single wait approached 30 s. The load
applied was well beyond anything a normal run encounters — sixteen compiler
jobs and six disk writers on an eight-processor VM — so the remaining margin is
adequate rather than tight.

## The second defect: tests that read the real filesystem

The idle `ctest` run for this branch reported **32/34**, and both failures came
from PR #103, merged earlier the same day.

`QuickSettingsController::radioHelperPath()` falls back to `~/.local/bin` and
then `PATH`. The new radio tests expressed "no helper installed" by *unsetting*
the `NORTHSTAR_RADIO_HELPER` override, which fell through to the real
filesystem. Once `northstar-radio` was installed on DEV01 during that same
pull request's own verification, two suites began to fail:

- `QuickSettingsControllerTest::leavesRadiosReadOnlyWithoutTheHelper` asserted
  no radio control was available and found the freshly installed helper.
- `DesktopSettingsTest::reportsMissingWirelessHardwareHonestly`, which is
  **pre-existing from Settings v2 (#96)**, expects the Wi-Fi entry to be a
  reading and found it declared as a toggle.

PR #103's suite passed only because it ran before the install. The ordering
concealed it.

This is the same defect that was caught during persistent notifications, where
tests constructing a controller with its default path would have written the
real desktop's history, and it was reintroduced two slices later. The trap is
already recorded in `docs/VM_VALIDATION_DEPLOYMENT.md`; writing it down did not
prevent it.

### It was not only a test bug

A configured `NORTHSTAR_RADIO_HELPER` naming a path that did not exist
previously reported radio control as **available**. The shell would have
offered a Wi-Fi toggle backed by a helper that was not there, which is exactly
the failure this surface is built to avoid.

A configured override is now authoritative including when it names nothing, so
it can say "no helper here" as well as "this one". That is what makes the tests
isolatable and fixes the shell behaviour at the same time.

Every radio case now pins the override, installing an executable stub where the
boundary is meant to exist. The Settings suite pins it to an absent path so the
declaration is deterministic, and gains a case for the branch that was
previously untested: boundary installed, radios declared as controls, still
honestly unavailable without hardware.

### Verified in both directions

Machine independence is the claim, so both machine states were measured rather
than one:

| Machine state | `quicksettingscontroller` | `desktopsettings` |
| --- | --- | --- |
| Helper installed in `~/.local/bin` | 12 passed, 0 failed | 12 passed, 0 failed |
| Helper removed from the machine | 12 passed, 0 failed | 12 passed, 0 failed |

## Automated evidence

- Clean build, all 394 targets, 0 errors.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr104-59a05ee --output-on-failure`
  — **34/34 suites passed, 0 failed**, run with the helper installed, which is
  the condition that exposed the second defect.
- The load A/B and the installed/removed A/B recorded above.
- `git diff --check` — exit 0.

## Also in this change

The header of `M7_WRITABLE_RADIOS_2026-08-18.md` still read "Interactive
acceptance is open" after the acceptance section below it had been updated to
record acceptance with radio behaviour untested. Corrected.

## Not claimed

Product behaviour **is** affected, in one narrow way: a configured
`NORTHSTAR_RADIO_HELPER` that names a path which does not exist now reports no
radio control, where it previously reported control as available. On a machine
with no override set, which is every ordinary session, the behaviour is
unchanged, so there is no user-visible difference to accept and no interactive
checklist. This is stated rather than filed under test changes because the
first draft of this document claimed no product behaviour was affected, which
was wrong.

The radio checklist from PR #103 remains outstanding and is unaffected by this
change. Nothing here brings it closer to being settled.
