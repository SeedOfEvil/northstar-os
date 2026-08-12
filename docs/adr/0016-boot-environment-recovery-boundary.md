# ADR 0016: Boot-environment recovery boundary

Status: Accepted

## Context

Northstar's transactional updater can create and activate a verified pre-update
ZFS boot environment, but recovery must also remain understandable after the
update dialog is gone. Passing an arbitrary environment name or `bectl`
subcommand from a desktop process would turn that convenience into a broad
root command boundary. Automatically rebooting, deleting, renaming, mounting,
or creating environments would also make a recovery tool destructive.

## Decision

Install `northstar-boot-environment` at a fixed root-owned path and expose it
through the Northstar Recovery application. Read-only inventory invokes only
`/sbin/bectl list -H`, validates a bounded maximum of 64 records, and emits a
versioned record set. The UI rejects malformed, oversized, unsafe, duplicate,
and contradictory records before displaying them.

Activation accepts exactly `--activate NAME --confirm NAME`. Both values must
match, the target must exist, and it must use the update broker's derived
`northstar-before-<channel>-r<revision>-<source>` namespace. PolicyKit requires
administrator authentication. The helper invokes only `bectl activate NAME`,
then rereads inventory and reports success only when the target is marked for
the next boot. Production runs ignore command-path overrides; root cannot use
test mode.

The helper cannot create, destroy, rename, mount, unmount, clone, or export a
boot environment. It never runs package commands and never reboots. The
desktop provides an explicit reboot-required state and a sanitized user-owned
inventory export that excludes logs, credentials, and arbitrary command
output.

## Consequences

Users can see the current and next boot environment and select a verified
Northstar pre-update recovery point without learning `bectl`. Generic FreeBSD
or operator-created environments remain visible but cannot cross the GUI
activation boundary. Returning from a recovery point to an arbitrary
environment remains an operator action until a future signed-forward-target
contract exists.

Actual activation followed by reboot is not performed on the persistent
development VM. It remains part of the disposable M5 Installer Release
Candidate acceptance cycle.
