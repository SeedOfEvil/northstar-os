# M5 corrected installer RC r81 - 2026-08-12

Draft PR 87 produced a corrected authoritative installer release candidate
after disposable Proxmox diagnostics exposed three integration defects in the
initial r80 media: the installer sidebar could consume the whole window, an
empty optical provider was reported as an unsafe disk record, and bare-X11
milestone windows used a fixed size that left uncovered display edges.

The corrected source changes were tested interactively on disposable diagnostic
media only. That diagnostic disk was explicitly excluded from release use. The
authoritative r81 package, repository, QCOW2, signed source, and raw installer
were rebuilt from immutable inputs on the disposable FreeBSD builder.

## Source and signed repository

- package source commit: `2002b9b2a9d8260d356a0b08539db7aeccf38293`;
- clean assembly/lock commit: `3367895aff689017f1d8d5c182f9a0ef26887e59`;
- package: `northstar-0.2.1-amd64.pkg`, 7,199,864 bytes;
- package SHA-256: `af4dd03581048ed46c9017b8227c43e47cd2d1c47974202bf0bbd21cfe91b83b`;
- repository revision: `81`;
- catalogue SHA-256: `fa0528ef1807b15f7b6fe17bb7ade9e48753e84e5fa15a69407ffadd82a7b918`;
- metadata SHA-256: `68fc6ec21589680d886f0e984bd54baaa7ba2e98aa098b02f07554f19360401e`;
- signing fingerprint: `894937b31ba0cd7c3dbff9df97c774dceb2c3c27b6607841a8444cb023e242a4`;
- authentic isolated `pkg` refresh: pass;
- altered signed-catalogue rejection: pass; and
- private signing key in publication or release output: no.

The optimized FreeBSD build completed all 95 remaining compile/link targets,
and the native CTest suite passed 29 of 29 tests. The refreshed offline runtime
closure contains 236 exact packages, including Northstar 0.2.1 and the pinned
compatibility compositor, with no Northstar 0.2.0 artifact.

## Authoritative artifacts

- QCOW2 size: 3,334,668,288 bytes;
- QCOW2 SHA-256: `f9d2635523da4693a614e4b8cc6b85f621e1495f1f3b681aecdc121fcc8d3bb7`;
- rootfs payload size: 1,029,825,460 bytes;
- rootfs SHA-256: `01655dfaf61f9b8ccdc5b3152b1da842e4c6ff7e69319b2d1a101ac71658ea2c`;
- raw installer virtual size: 21,474,836,480 bytes;
- raw installer SHA-256: `e49351370275a70b2108ff69d6e73322bcccc1bc5d79108a5a1579687c675bcf`;
- compressed transfer size: 2,818,799,819 bytes;
- compressed transfer SHA-256: `dc5da00f973334c5d4a3abc967fd8af8afef6c6836160ed2c4059dec9dc33683`;
- signed source manifest SHA-256: `d327d7c22ec94a5cb36dd067b0f631450504c08327679fa09842d99bd8191ccf`;
- detached source signature SHA-256: `dc1a7eb0eed1a1c90c858668822cede3406baca5bf19d4c687a3890508c4aa1e`;
- image provenance SHA-256: `d0b45f4b5593cafbdb611742cedb7321b9f4c93aab0e49b5ef0c1dee4b60c302`;
- media provenance SHA-256: `eb334a6543946f309c9c3f91ee982dfb41495a98829328c86297966be865daaa`.

Independent post-assembly validation rehashed every recorded artifact, verified
the detached RSA/SHA-256 source signature, compared the two payload copies,
confirmed exact commit and size bindings, and scanned for private key material.
`qemu-img check` reported no errors, zero fragmentation, and image end offset
3,334,668,288.

## Snapshot-only boot evidence

The authoritative QCOW2 reached all required serial milestones under UEFI,
virtio, TCG, and `snapshot=on` in approximately 208 seconds:

- `Trying to mount root from zfs:nstar_3367895aff68/ROOT/default`;
- `FreeBSD/amd64 (northstar-image)`; and
- the multi-user `login:` prompt.

Serial-log SHA-256 is
`1c4e387836540a95ecbb7e33b749415f99dae12effc6d1023efd8c0a80e9d10b`.
The source QCOW2 hash remained unchanged after the smoke.

## Remaining promotion gate

The compressed installer and public evidence are retained locally under
`.artifacts/accepted/m5-installer-rc/r81/`. PR 87 remains unmerged until this
authoritative r81 media is imported into the disposable Proxmox acceptance VM
and passes the destructive install/retry, first-administrator setup, branded
greeter/desktop, input, first-party applications, signed update, rollback, and
`/home` preservation checklist. The hot-patched diagnostic media is not an
accepted release artifact.

## Destructive-gate diagnostic follow-up

The first Proxmox destination review exposed a fourth integration defect: the
authenticated r81 media discovered the inactive 50 GiB target and prepared the
correct whole-disk plan, but the graphical controller still ended at the
earlier review-only boundary. The protected staging engine, PolicyKit actions,
guarded executor, execution marker, and signed read-only source were packaged,
but the GUI did not call them. No transaction was staged and no destination
mutation occurred.

The corrected controller now carries the discovered physical sector size into
the reviewed request, hashes the fixed signed source manifest, creates a
caller-owned mode-0600 request, and performs two bounded PolicyKit operations:
protected staging followed by guarded execution. It accepts only exact
transaction-bound reports and exposes explicit verifying, installing,
completed, failed, and recovery states. Disk discovery protocol 2 rejects
missing or unsupported sector sizes instead of assuming 512-byte media.

Native diagnostic evidence on the FreeBSD builder:

- complete project build: pass;
- CTest: 29 of 29 passed;
- installer controller stage-to-execute test: 5 of 5 passed;
- disk discovery, QML, signed source, staging engine, executor, and recovery
  shell contracts: pass; and
- real disks mutated by automated tests: none.

For the next interactive attempt, only the installer executable and discovery
helper were installed onto the existing r81 `da1` diagnostic media. A
root-owned marker explicitly labels it `diagnostic-installer-ui-hotfix`; the
patched hashes were independently reread from the exported media. This media
is suitable only for the snapshotted Proxmox install test. It does not replace
the immutable authoritative artifact, whose clean rebuild remains required
after destructive installation acceptance.

The first destructive diagnostic attempt then stopped during archive preflight
because the authentic rootfs listing was larger than the executor's original
4 MiB bound. The signed 982 MiB XZ payload produces a 7,990,918-byte listing
with 136,658 entries and a maximum path length of 194 bytes. The transaction
contained only the four staging journal events; no execution state,
`mutation-started`, partitioning, or destination change was recorded.

The corrected preflight permits at most 32 MiB of listing output, 500,000
entries, and 1,024 bytes per path. A process file-size limit applies while the
archive is listed, and absolute paths, parent traversal, empty entries, and all
existing signed-source checks remain rejected. A generated 140,000-entry
fixture reproduces a realistic listing above the former limit. All installer
contracts and 29 of 29 CTests pass with the new bound. The exact staged
transaction was archived through the protected engine as
`transaction-abandoned` with `DISK_MUTATION=none`; the corrected executor was
then installed and independently hash-verified on the diagnostic media.

The next attempt passed archive preflight and reached the first GPT operation,
where FreeBSD returned `gpart: Device busy`. The builder subsequently booted
unchanged: its original GPT and EFI/ZFS partitions remained intact and its
pool was healthy. The protected journal stopped at `mutation-started`; there
was no `partition-table-created` event. A disposable exported ZFS pool alone
did not reproduce the condition. FreeBSD's installed `geom(4)` documentation
identifies debug flag `0x10` as the bounded mechanism for replacing a Rank-1
provider, which is otherwise protected even for root.

The executor now records the existing `kern.geom.debugflags`, enables only bit
`0x10` immediately around destruction and creation of the confirmed target's
GPT, and restores the exact prior value before formatting begins. Every signal
and failure cleanup path also restores it. Tests force fake `gpart` to fail
unless the bit is active and prove restoration after both successful and
failed GPT replacement. All installer contracts and 29 of 29 CTests pass. The
interrupted transaction remains on diagnostic media for the authenticated
installer-side clean-retry flow because its target is intentionally active
while the builder system is booted.
