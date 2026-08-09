# ADR 0007: Independent verified-plan update broker

Status: Accepted

## Context

The shell and the helper request contract are intentionally unprivileged. A
root-owned request must not be created solely because an unprivileged QML
controller claims that a publication was verified. The future broker needs to
recheck the publication policy, fingerprint store, catalogue digest, RSA
signature, and package preview in a separate process before staging a request.

## Decision

Build `northstar-update-broker` from the same tested trust and update-plan core
used by Software Center. Its first operation is
`--stage-create-before`: it requires root, explicit `--confirm`, root-owned
non-group-writable policy/metadata/snapshot inputs, a root-owned non-group-
writable request directory, and a pending verified package change. It writes
only the strict mode-0600 request consumed by
`northstar-update-helper --apply`.

The broker does not call `pkg` or `bectl` in this slice. It checks the presence
of `bectl` and `zfs` as part of the existing preflight, but boot-environment
creation, package mutation, rollback activation, and the authorization policy
that can invoke the broker remain separate gates.

## Consequences

The privilege boundary now has an independently verified staging step instead
of trusting a shell-provided status. The broker can be exercised with a
root-owned fixture and fake tools without changing a real boot environment.
The installed user-local broker is not a production authorization policy;
deployment still requires a fixed root-owned path and an explicitly reviewed
privilege mechanism.

## Alternatives considered

- Let the shell write the helper request: rejected because unprivileged state
  cannot establish authorization.
- Have the broker trust only a request signature: deferred until the release
  key/custody and broker policy are established; the broker instead reuses the
  verified publication inputs directly in this slice.
- Invoke `pkg upgrade` while staging: rejected because boot-environment
  creation and package mutation need separate recovery and rollback evidence.

## Validation

`tests/vm/update-broker-smoke.sh` creates a temporary signed publication,
root-owned policy and snapshot, and fake `bectl`/`zfs` executables. It verifies
that the broker writes a root-owned mode-0600 request with the derived
boot-environment name and that no real `pkg` or `bectl` command runs.
