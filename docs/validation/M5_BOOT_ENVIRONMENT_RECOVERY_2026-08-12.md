# M5 boot-environment recovery validation — 2026-08-12

## Result

**PASS for the routine PR lane.** The first-party Recovery application,
bounded inventory helper, strict Qt model, exact-confirmation activation
request, PolicyKit boundary, package layout, and sanitized diagnostic export
passed their applicable source-level and native FreeBSD gates. No real boot
environment was activated, created, deleted, renamed, mounted, or rebooted.

Image checkpoint: **M5 Installer Release Candidate**

Image status: **DEFERRED**

## Immutable source

- Implementation commit: `24e5d103ef81`
- Archive: `northstar-pr85-24e5d103ef81.tar.gz`
- SHA-256: `bf34df017c968bd256768f3b827e1656af461cb726daff2d86a7e8ff0205f0c2`
- Native validation host: `NSTAR-DEV01`, FreeBSD 15.1 amd64 routine lane
- Extracted validation tree: `/tmp/northstar-pr85-1355349039e1`
  (the final checksummed archive replaced the source files in the retained
  immutable build tree before the final build and test pass)

## Automated evidence

Local contract checks passed:

- `git diff --check`
- `sh -n apps/recovery/northstar-boot-environment`
- `sh -n tests/unit/test-boot-environment-recovery.sh`
- `sh tests/unit/test-boot-environment-recovery.sh`
- `sh tests/unit/test-qml-surfaces.sh`

The final archive then passed on native FreeBSD:

- complete `make test` script and build gate;
- `29/29` CTest targets with zero failures;
- `northstar-bootenvironmentcontroller` strict parser, selection,
  confirmation, activation-result, and diagnostic-export tests;
- `northstar-recovery-qml` offscreen surface load;
- unsafe inventory, non-Northstar target, and mismatched-confirmation
  rejection; and
- idempotent fake activation with verified next-boot state.

The staged install under `/tmp/northstar-pr85-stage-24e5d103ef81` contains:

- `share/northstar/apps/NorthstarRecovery.app/Contents/Executable/northstar-recovery`;
- the bundle manifest and project-owned recovery icon;
- `libexec/northstar-boot-environment`; and
- `share/polkit-1/actions/org.northstar.recovery.policy`.

The staged executable's offscreen self-test passed without launching the
inventory helper. The staged helper's read-only status action returned:

```text
BOOT_ENVIRONMENT_RECOVERY=1
COUNT=2
ENTRY_0=default|NR|/|4.43G|2026-08-05 22:35|yes|yes|no|no
ENTRY_1=northstar-before-development-r78-017fc81040bb|-|-|1.18G|2026-08-10 19:48|no|no|yes|yes
```

This proves the live FreeBSD output is parsed correctly: `default` remained
active now and at next boot, while the inactive verified Northstar pre-update
environment was the only activatable recovery point. Repeating the read-only
inventory after validation showed the same state.

## Deferred acceptance

The M5 Installer Release Candidate must install the final package on a
disposable root-on-ZFS VM, open Recovery at 1280x800, authenticate and select a
known Northstar recovery point, reboot, prove it became active, verify the
desktop and `/home`, and restore the accepted forward environment. This is not
safe evidence to collect from persistent `NSTAR-DEV01`.
