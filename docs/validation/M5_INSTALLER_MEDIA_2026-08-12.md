# M5 installer media validation — 2026-08-12

## Scope

PR86 adds deterministic signed-source preparation and raw USB installer-media
assembly without accepting or writing a host disk. This is routine-lane
evidence for the media boundary, not a complete image acceptance result.

Image checkpoint: **M5 Installer Release Candidate**

Image status: **DEFERRED**

## Exact source

- Branch: `codex/m5-installer-usb-media`
- Implementation commit: `3a3aa8104360030029dc21f24232fe4a9b7a0376`
- Archive: `northstar-pr86-3a3aa8104360.tar.gz`
- Archive size: `14107953` bytes
- Archive SHA-256:
  `c9777becc1800fe13275fa5aa5c83e9e1d00227faa5b00e4ff054484eb3326b5`
- Native host: `nstar-dev01`, FreeBSD `15.1-RELEASE-p2`, amd64
- Native extraction: `/tmp/northstar-pr86-3a3aa8104360`

The archive was produced with `git archive` from the implementation commit.
DEV01 independently computed the same SHA-256 before extraction.

## Routine evidence

Local and native focused checks passed:

- shell syntax for the source preparer, raw assembler, QCOW2 assembler, and
  related tests;
- `tests/unit/test-installer-media.sh`;
- `tests/unit/test-image-assembler.sh`; and
- `tests/unit/test-installer-source-verify.sh`.

The installer-media contract generated an external disposable RSA key and
verified the detached signature. It rejected a signing key inside the project,
a runtime manifest differing from the payload, an unknown provenance field, a
development-autologin source image, and a modified QCOW2. Preflight created no
media output.

The complete native `make test` invocation reached its clean CMake/Ninja build
after all pre-build shell contracts passed. The SSH client timed out after 15
minutes while Ninja was still compiling continuously; no test failure occurred.
The exact same build directory was resumed with `cmake --build build --parallel
2`, completed all 46 remaining compile/link steps, and passed
`test-session-entrypoint.sh`. A separate `QT_QPA_PLATFORM=offscreen ctest
--test-dir build --output-on-failure` then passed all 29 tests with zero
failures.

`shellcheck` was unavailable on DEV01. Native `/bin/sh -n`, the behavioral
contracts above, and the complete project gate passed.

## Safety result

- No root command was used on DEV01.
- No `mdconfig`, GPT, UFS, ZFS, mount, raw-image assembly, host-device write,
  package mutation, service restart, or persistent deployment was performed.
- DEV01 changes were limited to the commit-named archive and extracted tree
  under `/tmp`.
- The production assembler has no destination-device option and requires a
  protected disposable-builder marker before its privileged path.
- The rootfs payload is emitted before media identity, autologin, PolicyKit,
  trust-key, mount, and execution-marker injection.

## Deferred evidence

At the M5 Installer Release Candidate checkpoint, rebuild the production
QCOW2 and matching rootfs payload from pinned inputs on a clean disposable
builder; prepare the signed source with release key custody; assemble and hash
the raw media; boot it in a fresh disposable Proxmox VM; install to a separate
target disk; validate first-administrator setup and installed graphical boot;
and exercise interrupted-install diagnostics, clean retry, update, rollback,
home preservation, and shutdown.
