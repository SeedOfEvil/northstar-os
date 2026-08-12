# ADR 0015: Installer clean retry and sanitized diagnostics

Status: Accepted

## Context

The guarded executor records and cleans up a failed destructive installation,
but PR83 intentionally leaves the transaction active and forbids generic
resume. Operators need enough evidence to understand the failure and a safe
way to begin again without deleting that evidence, leaking private state, or
turning recovery into an alternate disk-mutation interface.

## Decision

Install a fixed PolicyKit-facing helper,
`/usr/local/libexec/northstar-installer-recovery`, with only three operations:
`--status`, `--diagnostics TRANSACTION_ID`, and `--prepare-retry TRANSACTION_ID
--confirm-device DEVICE`. Production dispatches status to the fixed engine and
the other exact arguments to the fixed guarded executor. Arbitrary executables,
paths, commands, transaction formats, and device formats are rejected before
dispatch.

Diagnostics require the exact active stopped execution and validate the
protected transaction, execution state, and complete ordered journal. Output
is a bounded key/value schema containing only transaction/device identity,
sizes, layout, pool name, trusted digests and commit, last phase, mutation and
recovery state, and journal count/final event. Raw requests, journal contents,
logs, mount paths, home paths, usernames, environment, credentials, keys, and
payload contents are excluded. The unprivileged Qt controller rejects duplicate
or unknown fields and atomically writes the normalized report under the user's
Documents directory.

Clean retry is not resume. It requires the installer-media marker bound to the
same manifest, the exact active interrupted transaction, a journal whose phases
match the fixed execution order and end in `execution-interrupted`, the same
GEOM identity, a target absent from mounts, active ZFS pools, and swap, and an
exact typed device confirmation. It appends `retry-prepared`, removes only the
active-state pointer, and moves the complete failed transaction to the archive.
It invokes no partition, filesystem, pool, mount, extraction, or bootloader
command. The UI must then rediscover destinations and stage a new reviewed
transaction from its beginning.

Failures after execution authorization but before the first disk mutation are
also recorded as interrupted with `mutation_started=no`; this keeps every
attempt visible and recoverable through the same bounded workflow.

## Consequences

- The failed attempt remains durable evidence instead of being overwritten.
- Recovery cannot replay a partially completed destructive phase.
- Diagnostic export is useful for support without copying root-owned state or
  private logs into the user's account.
- A retry still needs administrator authentication, installer media, the same
  target identity, target quiescence, and another exact device confirmation.
- The routine test lane proves state transitions without touching a real disk;
  reboot and actual interrupted-media behavior remain release-candidate gates.

## Alternatives considered

Automatic phase resume was rejected because GPT, formatting, extraction, and
bootloader operations do not share one safe idempotency contract. Deleting the
failed transaction was rejected because it destroys support evidence. Exporting
the raw transaction directory was rejected because it widens privacy and file
ownership risks. Reusing the destructive execution PolicyKit prompt was
rejected because diagnostics and retry preparation perform no disk mutation
and need an accurate authorization message.

## Validation

The shell harness injects failure after dataset creation, verifies pool cleanup
and strict ordered interruption state, rejects wrong confirmation, target
drift, and journal tampering without invoking any disk tool, exports only the
allowlisted diagnostic schema, archives the failed attempt, and returns the
engine to idle. A separate wrapper test proves unsafe arguments never reach the
executor. Qt tests validate idle/interrupted UI state, exact confirmation,
atomic export, private-data exclusion, unknown-field rejection, and the
retry-ready transition. Native FreeBSD clean build and tests remain required.
