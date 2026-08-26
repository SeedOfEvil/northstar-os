# Continuous integration design

CI is split by trust boundary and operating-system coverage.

## Pull-request checks

The active [GitHub Actions workflow](../.github/workflows/ci.yml) runs:

- repository, canonical release-version, and top-level documentation truth
  contracts plus POSIX shell syntax on GitHub-hosted Ubuntu; and
- the complete non-privileged `make test` gate in a disposable FreeBSD 15.1
  amd64 virtual machine.

All remote actions are pinned to full commit SHAs. Jobs receive only read access
to repository contents and no release credentials. Markdown/link, formatting,
licence, and secret-scanning improvements may be added after they have an
accepted clean baseline and stable check name.

## Native FreeBSD checks

The native job uses a disposable FreeBSD virtual machine for:

- CMake configuration and build;
- Qt unit tests;
- QML surface-contract checks for product-critical shell wiring;
- non-privileged shell and image-boundary contract tests.

The exact FreeBSD release and package source are part of the job evidence.
Root access inside that disposable guest is used only to install build
dependencies. Northstar configuration, compilation, and tests run as the
dedicated unprivileged `northstarci` account.

## Protected release builder

Package repositories, image assembly, QEMU smoke tests, and signed artifacts run only from protected branches or manually approved workflows. Use disposable VMs where possible. Public pull requests and forks must not reach persistent privileged runners, package-signing keys, or production repository credentials.

Protected capability does not make complete image assembly a per-PR gate.
Routine image/installer PRs run non-destructive contracts and may use bounded
automated fixture or boot smoke where justified. Promotable image artifacts
and manual Proxmox/noVNC acceptance are produced at the named M5/M6 release-
candidate checkpoints in
[`docs/MILESTONE_IMAGE_VALIDATION.md`](MILESTONE_IMAGE_VALIDATION.md).

Package publication, complete image assembly, destructive update/rollback,
interactive VM acceptance, and physical hardware acceptance are intentionally
not pull-request CI jobs.

## Main branch protection

Protection is active on `main`. It requires pull requests, strict success from
the stable `Repository contracts` and `FreeBSD 15.1 build and test` checks,
resolved review conversations, and applies to administrators. Force pushes and
branch deletion are disabled. The zero-approval setting permits the project
owner's solo workflow; explicit manual acceptance is still required wherever a
PR declares a physical, noVNC, installer, storage, or release-candidate gate.

An automated green check never substitutes for a documented capability gate.
Conversely, documentation-only changes do not require DEV01 deployment or
physical acceptance when they alter no runtime boundary.
