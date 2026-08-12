# ADR 0014: Guarded installer execution

Status: Accepted

## Context

Northstar can authenticate installer media and stage a recoverable transaction,
but no component is yet allowed to consume that transaction and perform a disk
installation. Moving from read-only validation to destructive execution must
not turn the graphical installer into a root process, accept arbitrary paths or
commands, run from an ordinary installed system, trust stale device identity,
or lose the point of interruption after the first disk mutation.

## Decision

Install a second fixed PolicyKit executable,
`/usr/local/libexec/northstar-installer-executor`. It accepts only a protected
transaction identifier and, for execution, an exact `--confirm-device` value.
It never accepts a source path, public key, pool name, layout, payload path, or
shell command from its caller.

Production execution requires all of the following:

- FreeBSD root reached through the dedicated administrator-authenticated
  PolicyKit action;
- a root-owned mode-0400/0600 marker at
  `/etc/northstar/installer-execution.conf` declaring installer-media boot,
  confirmed-whole-disk scope, and the exact source-manifest digest;
- the fixed installer source root mounted as a distinct read-only filesystem,
  not merely a protected directory on writable system storage;
- the one root-owned active transaction and its unchanged request digest;
- `execution=guarded-executor-only`, exact whole-disk typed confirmation, and
  the fixed GPT/UEFI/ZFS layout;
- another detached-signature, payload, and provenance verification through the
  fixed source verifier;
- another GEOM identity and mount/ZFS/swap exclusion immediately before the
  mutation marker; and
- a bounded `northstar-rootfs-v1` archive with no absolute or parent-traversal
  entries.

After the mutation marker, the executor creates GPT EFI and ZFS partitions,
formats the EFI system partition, creates the Northstar ZFS root/home/tmp
datasets, extracts the signed root filesystem, verifies the installed runtime
manifest and required boot/session files, installs the UEFI fallback loader,
sets the ZFS boot filesystem/cache, exports the pool, and archives completed
state. Every completed phase is appended atomically to the root-owned journal.

Cleanup traps unmount the EFI filesystem and export the pool. A post-mutation
failure writes `status=interrupted`, the last completed phase,
`mutation_started=yes`, and `cleanup-and-restart-required`. PR83 deliberately
does not attempt in-place resume because destructive commands are not all
idempotent. The affected target must be inspected and restarted from its
beginning by a later recovery workflow. A completed transaction whose final
archive publication was interrupted can be finalized without disk mutation.

Production environment overrides are ignored, and root cannot enter test mode.
No installer-media execution marker is installed on a normal Northstar system
or development VM. The release-media build is responsible for provisioning
that marker and the public verification key.

## Consequences

- Compromise of the unprivileged UI cannot select commands or widen trusted
  file paths.
- Installing the helper on DEV01 or an ordinary Northstar installation does
  not authorize disk execution.
- Mutation can begin only after source and target checks have passed twice and
  the exact device name has been confirmed again.
- Interrupted destructive work is visible and cannot be abandoned through the
  preflight-only state path.
- A signed root-filesystem payload now uses source-manifest schema 2 and binds
  `payload_kind=northstar-rootfs-v1` plus the installed runtime-manifest digest.
- Actual physical-disk correctness and reboot recovery remain release-candidate
  evidence, not a routine development-VM claim.

## Alternatives considered

Running the QML installer as root was rejected as an unnecessarily broad
privilege boundary. Adding execution flags to the staging engine was rejected
because read-only planning and destructive execution need separate reviewable
surfaces. Trusting the initial source or disk check was rejected because both
can change before mutation. Automatically resuming after any failure was
rejected because partition, formatting, pool, and extraction phases have
different idempotency and recovery properties. Installing an enabled marker on
all systems was rejected because it would remove the installer-media boundary.

## Validation

Routine contracts use an unprivileged temporary root and fixed fake tools. They
prove that marker, source, target, archive, and confirmation failures invoke no
mutation command; that GPT, EFI, ZFS, extraction, verification, bootloader, and
export phases occur in order; that success archives the full journal; and that
an injected failure after dataset creation exports the pool, skips extraction,
and remains visibly interrupted. Native FreeBSD runs the same contracts and a
clean build. Real file-backed and physical target execution is deferred to a
disposable protected builder and the M5 Installer Release Candidate.
