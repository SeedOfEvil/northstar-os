# M6 alpha readiness

PR90 establishes the inventory boundary used before a machine enters the
Northstar alpha hardware matrix. It deliberately does not publish a new image,
change the installed system, or claim that a successful inventory alone proves
alpha support.

## Interface

```sh
make alpha-readiness ALPHA_OUTPUT=/tmp/northstar-alpha-readiness.conf
sh tools/collect-alpha-readiness.sh --require-ready
make alpha-readiness-test
```

The first command always produces a bounded schema-1 record when collection is
possible. The second is a physical-candidate gate and exits nonzero unless the
complete Intel or AMD capability lane is present. The third exercises all
classifications without inspecting host hardware.

## Classification

| Status | Meaning |
| --- | --- |
| `ready` | Exact supported base plus direct Intel/AMD DRM card and render nodes, wired networking, audio, and input |
| `supplemental` | Exact supported virtual base; useful for VM development evidence but not a direct DRM/KMS claim |
| `blocked` | Base, graphics, network, audio, input, or supported-driver requirements are incomplete |

`matrix_claim` is intentionally narrower than detection. It is `intel` or
`amd` only for a complete physical lane, `vm` only for a valid supplemental
virtual lane, and `none` otherwise. Unsupported DRM hardware remains blocked.

## Privacy and safety

The probe is read-only. Its output excludes serials, MAC addresses, raw PCI
listings, interface names, command lines, environment values, and user paths.
It records session variables only as present or absent. Output is written
atomically with mode 0600, and an existing symbolic-link target is rejected.

## Remaining evidence

NSTAR-DEV01 is expected to remain supplemental because Proxmox basic VGA uses
scfb/pixman and exposes no guest DRM render device. M6 still requires selection
and interactive validation of one physical Intel DRM machine and one physical
AMD DRM machine, followed by the complete application, recovery, update,
rollback, and shutdown matrix.

PR91 adds the next boundary: `tools/run-alpha-matrix.sh` combines this automatic
inventory with a strict fixed-field operator observation record. The runner is
read-only and does not launch applications, inject crashes, mutate packages,
reboot, or shut down a system. See
[`M6_HARDWARE_MATRIX_RUNNER.md`](M6_HARDWARE_MATRIX_RUNNER.md).
