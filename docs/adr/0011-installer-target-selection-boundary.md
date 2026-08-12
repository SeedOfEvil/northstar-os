# ADR 0011: Installer target selection before mutation

Status: Accepted

## Context

Installing Northstar will eventually erase and repartition a disk. A graphical
selection error, stale device list, malformed discovery output, or accidental
choice of the running system disk must not cross directly into privileged disk
mutation.

## Decision

Disk discovery is a separate, unprivileged, read-only helper with a versioned
bounded output protocol. It enumerates FreeBSD whole-disk devices, reports
capacity and description, and marks any disk referenced by mounted filesystems,
ZFS pools, or swap as ineligible. Disks below 16 GiB are also ineligible.

The GUI rejects malformed, duplicate, oversized, or contradictory records. It
never permits selection of a system or ineligible disk. Before preparing a
plan, the operator must select an eligible target, type its exact device name,
and affirm that all data will be permanently erased.

This slice only creates a human-readable review plan. It contains no partition,
filesystem, pool-creation, or copy operation. A later privileged installer
engine must rediscover and revalidate immutable device identity immediately
before mutation; GUI state will never be sufficient authorization.

## Consequences

- Routine UI and safety work can be validated on DEV01 without touching disks.
- A disk becoming active after discovery cannot be made safe by this model
  alone; the future engine must perform a final independent preflight.
- Device names are confirmation affordances, not stable hardware identities.
  The execution contract must bind stronger identity data where FreeBSD exposes
  it.
- Complete installation remains deferred to the M5 Installer Release Candidate.

## Alternatives considered

Running the complete GUI as root unnecessarily expands privilege. Allowing an
Install button immediately after a single click is too easy to misuse. Hiding
the running disk without displaying why it is unavailable makes operator review
weaker. Reusing UI-provided discovery records inside a privileged helper would
trust stale and attacker-controlled state.

## Validation

Fixture tests prove active and undersized disks are excluded and the helper has
no mutation commands. Controller tests cover malformed records, ineligible
selection, exact-name confirmation, erasure acknowledgement, and review-only
planning. QML loading and 1280x800 review are routine gates; actual disk
mutation is outside this PR.
