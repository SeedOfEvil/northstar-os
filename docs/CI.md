# Continuous integration design

CI is split by trust boundary and operating-system coverage.

## GitHub-hosted checks

Use GitHub Actions for documentation and repository checks:

- Markdown and link checks;
- ShellCheck;
- CMake configuration and formatting checks;
- clang-format when C++ sources exist;
- licence and secret scanning;
- GitHub metadata validation.

All third-party actions must be pinned to full commit SHAs before the repository becomes public.

## Native FreeBSD checks

Use Cirrus CI for native FreeBSD virtual machines:

- host validation and CMake build;
- Qt unit tests;
- QML surface-contract checks for product-critical shell wiring;
- Ports package tests;
- basic session integration tests.

The exact FreeBSD release and package source are part of the job evidence.

## Protected release builder

Package repositories, image assembly, QEMU smoke tests, and signed artifacts run only from protected branches or manually approved workflows. Use disposable VMs where possible. Public pull requests and forks must not reach persistent privileged runners, package-signing keys, or production repository credentials.

The workflow implementation is planned for PR 3. This document defines the contract without falsely claiming that CI is active in PR 2.
