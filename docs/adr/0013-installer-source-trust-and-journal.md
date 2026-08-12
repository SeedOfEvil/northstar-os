# ADR 0013: Installer source trust and recoverable journal

Status: Accepted

## Context

A reviewed source-manifest digest is only useful if the privileged installer
can bind it to authenticated release media. Installer state must also survive
a process interruption without silently starting over, hiding historical
work, or allowing a later request to overwrite an unresolved transaction.

## Decision

Install a fixed read-only verifier at
`/usr/local/libexec/northstar-installer-source-verify`. In production it reads
only `/var/run/northstar-installer/source`, trusts only the root-owned public
key at `/usr/local/share/northstar/installer/source-signing.pem`, and uses the
base-system OpenSSL and SHA-256 tools. Environment path overrides are accepted
only in explicit test mode.

The source contains a bounded schema-2 ten-field manifest, a detached RSA/SHA-256
signature, and one path-safe `.txz` payload. The verifier requires root-owned,
non-group/world-writable source material; verifies the reviewed manifest
digest, detached signature, payload size, and payload digest; and reports
bounded provenance. Signing private keys remain outside the repository,
installer media, pull-request environment, and runtime image.

The protected installer engine stages state only after source verification and
a fresh target revalidation both pass. Schema 2 additionally binds the
`northstar-rootfs-v1` payload kind and installed runtime-manifest digest. Each
transaction receives a unique,
bounded identifier and a mode-0600 request, transaction record, and sequenced
journal below `/var/db/northstar/installer/transactions`. A mode-0600 active
pointer identifies the only live transaction. Missing active state with one
valid transaction is reported as interrupted. Explicit authenticated recovery
restores that pointer; explicit confirmed abandonment appends a journal event
and moves the transaction into the archive. No operation in this slice mutates
a disk or extracts the payload.

## Consequences

- A digest copied from an untrusted request cannot substitute arbitrary media.
- Tampered manifests, signatures, payloads, unsafe paths, and mutable source
  permissions fail closed before target or transaction mutation.
- Interruption between transaction publication and active-pointer publication
  leaves a discoverable orphan instead of invisible historical state.
- Recovery and abandonment are deliberate authenticated actions and remain
  auditable after the active pointer is removed.
- Release media cannot stage until the protected release process provisions
  the corresponding public key; no placeholder production key is shipped.
- Destructive installation requires the separately reviewed executor in
  [`ADR 0014`](0014-guarded-installer-execution.md), which revalidates source
  and target immediately before its first mutation.

## Alternatives considered

Trusting only a manifest digest from the UI was rejected because the UI and
request are caller-controlled. Embedding a private key was rejected because it
would let every image forge trusted media. Accepting arbitrary source paths or
public keys was rejected because it widens the privileged interface. A single
overwritable state file was rejected because interrupted and historical work
would become ambiguous. Automatically deleting orphaned state was rejected
because recovery must preserve evidence and require an explicit operator
choice.

## Validation

Unit contracts generate a disposable external RSA key, accept authentic media,
and reject reviewed-digest mismatch, payload tampering, signature tampering,
and signed path traversal. Engine tests prove failed source verification writes
no active transaction, the journal records source and target checks, duplicate
staging is blocked, interrupted state is detected and recovered, abandonment
is archived, and no disk-mutation command is present. Native FreeBSD validation
runs these contracts on DEV01; destructive evidence remains deferred to the M5
Installer Release Candidate.
