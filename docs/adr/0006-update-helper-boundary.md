# ADR 0006: Narrow update-helper request boundary

Status: Accepted

## Context

The Software Center can now verify a publication and prepare a bounded boot
environment name, but the desktop must not pass arbitrary commands through
`sudo`, D-Bus, or a root-owned script. A future update service needs a stable
boundary that can be audited independently from the shell and that preserves
the distinction between a verified plan and an authorized mutation.

## Decision

Define the first helper protocol as a strict, line-oriented request consumed
by `src/update/northstar-update-helper`:

- protocol version `1`;
- operation `create-before` or `rollback` only;
- development/stable channel, bounded repository revision, source revision,
  catalogue digest, and trusted signature fingerprint;
- a boot-environment name derived exactly from those plan fields;
- `plan_status=verified`; and
- `authorization=interactive-confirmation`.

The read-only `--dry-run` path validates and prints the one `bectl` operation.
The mutation `--apply` path requires root and a non-symlink request owned by
root with mode `0600`, then invokes only `bectl create` or `bectl activate`.
Package installation and upgrade are not part of this helper. A future
privileged broker must create the request after rechecking the signed plan and
collecting user confirmation; no `sudoers` rule or desktop connection is
added by this slice.

## Consequences

The helper can be tested with deterministic request fixtures and gives the
future broker a small, explicit authorization surface. It does not yet create
boot environments on the development VM, mutate package state, or implement
rollback selection end to end. The production deployment must use a fixed
root-owned helper path and must not expose the test-only
`NORTHSTAR_UPDATE_BECTL_PATH` override through its authorization policy.

## Alternatives considered

- Passing `pkg`, `bectl`, or shell text from QML: rejected because it widens
  the privileged command surface and makes authorization difficult to audit.
- Letting the shell call `bectl` directly: rejected because the shell is an
  unprivileged session component and must not own system mutation.
- Combining package mutation and boot-environment operations immediately:
  deferred until the broker can verify the repository transaction and prove
  N-1/rollback behavior in a disposable ZFS environment.

## Validation

`tests/unit/test-update-helper.sh` covers capabilities, valid dry-run plans,
bounded names, verified-plan requirements, duplicate fields, and root-only
apply behavior. Native FreeBSD validation must additionally use stubs or a
disposable root-owned request; no real VM boot environment is changed by this
slice.
