# Contributing to Northstar

Northstar is early-stage infrastructure work. Small, reviewable changes are more valuable than speculative implementation. The current milestone is architectural preparation; the first source implementation is deliberately deferred until the M0 host lane is defined.

## Before opening a pull request

1. Read the charter, architecture, roadmap, and quality gates.
2. Confirm that the change belongs to the milestone and issue scope.
3. Add or update tests and documentation for user-visible behavior.
4. Record an architectural decision when a change alters a fixed boundary.
5. Check that dependencies, source archives, and build inputs are pinned.
6. Check that no secret, signing key, image, package repository, or downloaded source archive is included.

Every pull request should complete the template in [`.github/pull_request_template.md`](.github/pull_request_template.md).

## Working branches and commits

Use a short-lived branch from `main`. Keep commits focused and use imperative subjects, for example:

```text
docs: define the initial package and update model
```

Do not rewrite shared branches. The intended GitHub workflow is protected `main`, pull requests for all changes, required checks, and squash merges.

## Native FreeBSD evidence

Changes that touch shell, session, packaging, image, or host tooling need evidence from the supported FreeBSD lane. State the exact FreeBSD release, architecture, package source, command, and result in the pull request. A Linux-only result is useful during development but is not native acceptance evidence.

Do not run desktop, package, image, or service setup as root unless the operation is explicitly designed for narrow authorization. Project files must not be copied into `/usr/bin` or `/usr/lib` during development.

## Dependencies and licences

Prefer upstream FreeBSD packages and protocols. New external dependencies require a version or commit, checksum where applicable, licence review, and an entry in [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md). GitHub Actions must use full commit SHA pinning before activation in a public repository.

Do not add Apple-owned assets, proprietary fonts, sounds, logos, trademarks, or reverse-engineered compatibility requirements. Visual inspiration is not a licence to copy protected assets or confusing branding.

## Reporting security issues

Do not open a public issue for an undisclosed vulnerability. Follow [`SECURITY.md`](SECURITY.md). Never include credentials, private keys, package-signing material, or personal data in an issue or pull request.
