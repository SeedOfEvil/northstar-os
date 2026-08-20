# ADR 0018: Pinned trust boundary for third-party packages

Status: Accepted

## Context

Northstar must let a user install and remove ordinary FreeBSD applications
without becoming a new package manager or treating every configured repository
as equally trusted. The current Software Center can distinguish requested
packages from dependencies and can inspect a signed Northstar publication, but
it does not yet define which third-party packages it may mutate.

The signed Northstar repository attests only to Northstar packages. FreeBSD
packages have their own repository identity, signatures, origins, licences, and
maintenance lifecycle. Re-signing or copying them into the Northstar repository
would blur that provenance and make Northstar responsible for an unnecessary
second distribution channel. Conversely, accepting package names or repository
configuration from the desktop across a privileged boundary would allow an
untrusted session process to choose system mutation targets or enroll new trust.

[ADR 0004](0004-package-and-update-model.md) requires a pinned FreeBSD package
source and a separate signed project repository. [ADR 0006](0006-update-helper-boundary.md),
[ADR 0007](0007-update-broker-verification.md), and
[ADR 0008](0008-transactional-update-and-rollback.md) require independently
verified plans, fixed privileged arguments, pre-mutation boot environments, and
recoverable transactions. This decision extends those constraints to
third-party install and remove operations.

## Decision

For the first release, Software Center may install or remove a third-party
package only when all of these conditions hold:

- the candidate comes from the single FreeBSD package source pinned by the
  Northstar release definition, including its ABI, branch, repository identity,
  and resolved catalogue state;
- FreeBSD `pkg` has authenticated that repository through its native signature
  policy, and the privileged broker has refreshed and rechecked the catalogue;
- the candidate is identified by the broker from that authenticated catalogue,
  not by a caller-supplied command, repository URL, origin, path, or package
  name; and
- the complete `pkg` transaction preview is still current at authorization
  time and is shown for explicit user confirmation and administrator
  authentication.

The Northstar project repository remains a separate signed source for
Northstar-owned packages. Northstar does not mirror, re-sign, or represent
FreeBSD packages as project packages. Package details must name the source and
origin and must not imply that repository authentication is an application
sandbox, a code review, or a Northstar security endorsement.

The privileged boundary accepts only an opaque, short-lived plan identifier.
It independently resolves that identifier to a root-owned plan, revalidates
repository identity and catalogue state, rejects a changed preview, and invokes
`pkg` with its own fixed repository and package targets. It creates a named ZFS
boot environment before every install or remove transaction. Only one package
transaction may be pending or running at a time.

Install is limited to an explicit package offered by the authenticated
catalogue; `pkg` may add the dependencies in the confirmed preview. Remove is
limited to an installed, explicitly requested package. Dependencies,
automatically installed packages, locked packages, the package manager itself,
and FreeBSD base or kernel components are not directly removable through
Software Center. Dependency cleanup, repository enrollment, repository
switching, package holds, local package files, Ports builds, and arbitrary
third-party repositories are outside this boundary.

Packages from repositories that do not match the release-pinned FreeBSD source
or the signed Northstar source remain visible in inventory with their source
identified, but Software Center does not install, remove, or update them.

## Consequences

Users get a deliberately smaller catalogue than raw `pkg` configuration might
offer, but every enabled mutation has one attributable source and a reproducible
preview. Existing custom repository packages remain usable outside Software
Center and are not silently removed or enrolled into Northstar trust.

Install and remove require a new broker/transaction operation rather than
reusing the update helper with caller-selected package text. Catalogue refresh,
plan expiry, locked-package handling, concurrent transaction exclusion,
PolicyKit policy, and boot-environment state become implementation and release
gates. Rollback protects the root filesystem and package database; it does not
roll back or delete user data under `/home`.

The pinned FreeBSD source must be republished or deliberately advanced as part
of a Northstar release decision. A stale or unavailable source blocks mutation
and must be explained to the user rather than falling back to another mirror,
branch, repository, or unsigned package.

## Alternatives considered

- Allow every repository already configured for `pkg`: rejected because local
  configuration alone does not make a source part of Northstar's reviewed trust
  and recovery contract.
- Mirror or re-sign FreeBSD packages in the Northstar repository: rejected
  because it obscures upstream provenance and creates a second distribution and
  patch-responsibility boundary.
- Pass a selected package name or `pkg` arguments from QML to a privileged
  helper: rejected because it makes the unprivileged desktop authoritative for
  mutation targets.
- Support local package files, Ports builds, and repository enrollment in the
  first Software Center: deferred because each needs a distinct provenance,
  preview, authorization, and recovery policy.
- Remove dependencies automatically with every application: rejected because
  the resulting transaction is broader than the user's explicit intent;
  dependency cleanup remains a separately previewed future operation.

## Validation

Deterministic tests must prove that the broker accepts only candidates from the
release-pinned FreeBSD catalogue, binds authorization to the exact preview,
rejects expired or changed plans and caller-controlled package text, serializes
transactions, and refuses protected or non-requested removals. Negative tests
must cover an unknown repository, altered catalogue, ABI or branch mismatch,
locked package, dependency removal, replayed plan, and missing boot-environment
preflight.

The native FreeBSD gate must use a disposable signed repository and ZFS boot
environment to prove install and remove ordering, authentic package ownership,
rollback after injected failure, and preservation of `/home`. Interactive
acceptance must show source/origin, the full dependency transaction, explicit
confirmation and authentication, success/failure state, and honest blocked
messaging. It must not mutate the host's real custom repositories or claim that
an unexecuted production-signing or rollback gate passed.
