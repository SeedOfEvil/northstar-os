# Contributing to Northstar

Northstar is an implemented pre-alpha desktop operating system. Current work
closes the M6 hardware matrix and hardens the M7 daily-driver desktop. Prefer a
small, reviewable change that completes one declared boundary over speculative
new surface area.

## Before opening a pull request

1. Read the charter, architecture, current roadmap, and quality gates.
2. Start from synchronized `main` on a short-lived `codex/` branch.
3. Keep the change inside one milestone slice and state what is excluded.
4. Add or update tests and user-facing documentation.
5. Record an ADR when the change alters a fixed architectural, privilege, or
   trust boundary.
6. Keep dependencies, source archives, package inputs, and remote CI actions
   pinned; update `THIRD_PARTY_LICENSES.md` when required.
7. Confirm that no secret, signing key, image, package repository, downloaded
   source archive, test credential, or private hardware identifier is staged.

Complete [the pull-request template](.github/pull_request_template.md) with
exact commands, environment, results, manual observations, deferred gates, and
rollback behavior.

## Branches, checks, and merging

`main` is protected. Every change goes through a pull request and must pass the
required `Repository contracts` and `FreeBSD 15.1 build and test` checks.
Force pushes and deletion of `main` are blocked, administrators are subject to
the same protection, and review conversations must be resolved.

Keep local and cloud feature branches synchronized. Do not merge merely
because CI is green: request explicit acceptance for any required physical,
noVNC, installer, storage, or release-candidate gate. After merge, synchronize
local `main` and remove the completed feature branch.

Commit subjects should be short and imperative, for example:

```text
Document the current M6 hardware boundary
```

## Evidence by change type

- Documentation-only changes require repository contracts, `git diff --check`,
  and link/heading inspection. They do not require deployment to DEV01 or the
  physical laptop.
- Portable implementation changes require focused tests plus the protected
  native FreeBSD job.
- Shell and application UX changes require the relevant automated checks and
  focused interactive acceptance on the declared VM or physical lane.
- Session, display, radio, audio, power, suspend, input, installer, image,
  package-mutation, update, and rollback claims require evidence from the
  environment that owns that capability.

NSTAR-DEV01 and Proxmox's X11/pixman fallback are useful supplemental lanes;
they are not direct DRM/KMS evidence. Completed physical tests remain valid
unless the change crosses their tested boundary. Do not rebuild an installer
or repeat a destructive installation for an unrelated application or docs PR.

Run desktop, package, image, or service setup as root only where a documented
narrow authorization boundary requires it. Never copy project files into
`/usr/bin` or `/usr/lib` during development.

## Release identity and licences

The repository-root [`VERSION`](VERSION) file is the sole Northstar product
and package version. Follow [`docs/RELEASE_IDENTITY.md`](docs/RELEASE_IDENTITY.md)
when changing it or producing package, repository, or image provenance.

Prefer upstream FreeBSD packages and protocols. Do not add Apple-owned assets,
proprietary fonts, sounds, logos, trademarks, or reverse-engineered
compatibility requirements. Visual inspiration is not permission to copy
protected assets or create confusing branding.

## Reporting security issues

Do not open a public issue for an undisclosed vulnerability. Follow
[`SECURITY.md`](SECURITY.md). Never include credentials, private keys,
package-signing material, or personal data in an issue or pull request.
