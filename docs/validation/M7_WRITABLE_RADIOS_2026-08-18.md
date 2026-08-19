# Writable Wi-Fi and Bluetooth — 2026-08-18

PR #103, built from commit `dd79fca` in `/home/northstar/builds/pr103-dd79fca`
on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2) with the project's canonical
`-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.

**Interactive acceptance is open.** It cannot be completed on DEV01, for the
reason in the next section, and is deferred to the physical machine.

## The development VM has no radios

DEV01 reports `vtnet0` and `lo0` and nothing else: `net.wlan.devices` is empty,
there is no `wlan` interface, no `/dev/ubt*`, and the Bluetooth socket family
is unavailable. `pciconf` lists only a virtio network device.

Nothing in this slice can therefore be exercised end to end here. What that
does *not* excuse is claiming otherwise, so the writer is covered by injected
command providers, the privilege boundary is verified directly, and the
observable behaviour on a machine with no radios is verified against the real
install.

## What changed

Settings and Quick Settings could only observe the radios, because
`QuickSettingsController` exposed no writers. Both surfaces said so honestly,
but a desktop that cannot switch its own Wi-Fi off is not a daily driver.

## Privilege shape

Writing goes through `src/power/northstar-radio`, a fixed-argument boundary in
the same shape as the existing `northstar-power`.

- It accepts **exactly two enumerated words** (`wifi|bluetooth` and `on|off`).
  Anything else exits 64 before reaching `sudo`.
- It **resolves the wireless interface itself** from `ifconfig -l`, so no
  caller-supplied string ever reaches `ifconfig`, `service`, or `sudo`. The
  shell cannot construct a command line here even in principle.
- Exit 69 reports absent hardware, which is a different condition from a
  malformed or refused request and is surfaced differently in the shell.

Like `northstar-power`, this relies on the development VM's non-interactive
`sudo`. A production install should replace it with a root-owned helper under a
narrow policy permitting only these exact actions; the comment in the script
says so.

## A control only appears where it can act

A radio becomes a real control only where the boundary is installed, and the
Quick Settings tile is pressable only when the hardware is present as well.
Without either, the entry stays a reading with the reason its own controller
gave. This is the rule Settings v2 already enforces by refusing to register a
control whose accessors cannot back it, applied to the radios.

On DEV01 after this change the boundary *is* installed but the hardware is not,
so the Network entries are toggles that report themselves unavailable with
`No wireless interface detected` and `No Bluetooth adapter detected`.

## A correctness fix found without hardware

`wifiEnabled` previously meant "associated with a network", while the toggle
performs `ifconfig <iface> up`. Association is owned by `wpa_supplicant` and
takes seconds, so switching Wi-Fi on would have left the tile dark and the
toggle reading off for several seconds — looking broken at exactly the moment
it had been used.

`wifiEnabled` now means the interface is administratively up, which is what the
control actually changes. Association is reported in the status text instead.

While making that change, the association test itself proved questionable: it
matched only `status: active`, which is the wired form. FreeBSD reports
`status: associated` for a wireless link, so the pre-existing reading was
probably wrong on real hardware too. Both spellings are now accepted, so the
reading does not depend on which driver is in use. **This is reasoning about
the code, not evidence from a radio**, and is one of the things the interactive
checklist has to confirm.

## Automated evidence

- Clean build, all 394 targets, 0 errors.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr103-dd79fca --output-on-failure`
  — **34/34 suites passed, 0 failed**, on an idle machine.
- An earlier run of the same suite, started immediately after the build while
  the VM was still busy, reported **33/34**. The single failure was
  `FileBrowserControllerTest::copiesAndUndoesEntries`, which waits 5000 ms for
  an asynchronous copy that took 6600 ms under load. The suite took 11.7 s
  loaded against 2.9–3.4 s idle, and passed three times out of three when
  re-run on a quiet machine. It is unrelated to this change and is a real
  latent flake in that test's wait budget, filed separately rather than widened
  here.
- `northstar-quicksettingscontroller` — **12 passed, 0 failed, 0 skipped**, up
  from 6. New cases: no helper installed leaves the radios read-only; a
  successful toggle reaches the boundary with exactly two words; absent
  hardware (exit 69) and a refused change (non-zero) are reported differently,
  as is a helper that could not be started at all; a machine with no wireless
  interface — modelled on DEV01 — refuses the request and never calls the
  helper; and administrative state is reported rather than association across
  four interface readings.
- `northstar-desktopsettings` — **11 passed, 0 failed**.
- `sh tests/unit/test-qml-surfaces.sh` — passed, with new contracts pinning
  both tile actions and their writability gating.
- `git diff --check` — exit 0.

## Evidence against the real install

Run against `~/.local/bin/northstar-radio` as installed, not the source tree.

- Malformed input is refused before `sudo` is reached: no arguments, one
  argument, three arguments, an unknown radio, and `wifi ON` in the wrong case
  all exit **64**.
- `wifi on` and `wifi off` both exit **69** on this machine, correctly
  reporting absent hardware rather than attempting a privileged call.
- The installed shell carries the three new radio symbols.
- The shell was restarted under its supervisor (29896 to 37544) with the
  compositor left up, so this was a shell-only restart.

## Interactive acceptance

**Accepted on the automated and boundary evidence only, with radio behaviour
explicitly untested.** Hector accepted the slice on that basis, knowing no
radio has been switched by this code, and asked that the detailed check be run
when the physical machine arrives.

What that acceptance does and does not cover:

- **Covered:** the privilege boundary's refusal and absent-hardware paths
  against the real install, the writer's behaviour against injected command
  providers, and the observable state on a machine with no radios.
- **Not covered:** any actual radio changing state. Nothing below the
  boundary has been exercised.

This is recorded as a deliberate decision rather than an oversight, so that a
later reader does not mistake this slice for one that had hardware evidence.
Until the checklist below is completed, the honest claim is that the code is
correct where it could be tested and unproven where it could not.

The checklist, to be run on the physical machine (expected 2026-08-20 or
2026-08-21):

1. Quick Settings shows the Wi-Fi tile as pressable, not greyed.
2. Pressing it switches the radio off; the tile dims and `ifconfig` shows the
   interface down.
3. Pressing it again switches the radio on; the tile lights **immediately**,
   before association completes, and the status text moves from
   `Wireless interface on, not connected` to `Connected to <ssid>` on its own.
4. The status text names the real network, confirming which `status:` spelling
   the adapter reports.
5. Settings, Network section: the Wi-Fi and Bluetooth toggles are enabled and
   agree with the tiles.
6. Bluetooth on and off, if an adapter is present; otherwise it must stay
   unavailable with its reason.
7. Nothing in the shell hangs while a radio change is in flight.

## Not claimed

No radio has been switched by this code, and merging does not change that.
Toggling has been proven only against injected command providers and against
the boundary's refusal paths. Whether `ifconfig up`/`down` behaves as expected
on the target adapter, whether the adapter reports `associated` or `active`,
and whether `sudo -n` is permitted for the helper under the machine's policy
are all unestablished and are exactly what the checklist above exists to
settle.

If the checklist fails on real hardware, that is a defect in this merged slice,
not a new one.

Physical Intel/AMD DRM/KMS acceptance, native compositor quality, and
multi-display evidence remain deferred and are not advanced by this change.
