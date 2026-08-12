# M5 first-boot setup

This slice provides Northstar's production first-administrator workflow. It is
a routine development PR under the milestone image-validation policy; it does
not trigger a new QCOW2 cycle.

## Product flow

1. A production image creates the temporary, non-wheel `northstar-setup`
   identity and a root-owned `/var/db/northstar/first-boot.pending` marker.
2. SDDM automatically starts `northstar-first-boot.desktop` only for that
   pending installation.
3. The branded wizard collects the first administrator, password, locale,
   timezone, and keyboard layout.
4. The GUI writes only bounded non-secret profile fields to a mode-0600
   temporary request. It writes the password once to the protected helper's
   standard input and then clears both password fields and its byte buffer.
5. The helper revalidates caller, request, pending state, and every field;
   creates one wheel administrator; applies regional settings; disables the
   temporary setup identity and autologin; and seals completion.
6. After restart, SDDM presents the normal branded login for the new account.

Development images assembled with `--development-autologin` intentionally
keep the existing `northstar` test account and bypass this production flow.

## Routine validation

```sh
make first-boot-provision-test
make qml-surface-test
make image-assembler-test
make build
ctest --test-dir build --output-on-failure
```

The helper test uses an isolated temporary root and fake FreeBSD tools. It does
not create or modify a real user. On DEV01, run the QML self-test and inspect
the wizard; do not apply provisioning against the persistent development VM.

## Deferred image acceptance

Image checkpoint: M5 Installer Release Candidate

Image status: DEFERRED

Routine evidence: controller/unit tests, isolated helper mutation tests, QML
surface and offscreen loading, image-assembler contracts, native FreeBSD build,
and a non-mutating DEV01 visual check.

Deferred evidence: production SDDM enters the setup identity exactly once; the
first administrator can restart and sign in; the temporary identity and
autologin cannot return; wrong, interrupted, and repeated setup attempts are
safe; regional settings persist in the integrated installed image.
