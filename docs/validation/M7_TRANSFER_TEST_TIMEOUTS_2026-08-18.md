# Transfer test failure deadlines — 2026-08-18

PR #104, verified on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2) at commit `a73c102`,
built in `/home/northstar/builds/pr104-a73c102` with the project's canonical
`-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.

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

## Automated evidence

- Clean build, all 394 targets, 0 errors.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr104-a73c102 --output-on-failure`
  — **34/34 suites passed, 0 failed**, idle.
- The A/B under load recorded above.
- `git diff --check` — exit 0.

## Also in this change

The header of `M7_WRITABLE_RADIOS_2026-08-18.md` still read "Interactive
acceptance is open" after the acceptance section below it had been updated to
record acceptance with radio behaviour untested. Corrected.

## Not claimed

This changes only test wait budgets and one documentation header. No product
behaviour is affected, so there is no new binary for a user to accept and no
interactive checklist.

The radio checklist from PR #103 remains outstanding and is unaffected by this
change.
