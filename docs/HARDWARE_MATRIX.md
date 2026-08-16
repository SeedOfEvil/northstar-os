# Hardware matrix

Northstar starts with a narrow, testable hardware lane. Hardware outside this matrix may work, but it is not an alpha support promise until it has repeatable evidence.

## Alpha targets

| Component | Supported target | Status |
| --- | --- | --- |
| CPU | x86-64 | Required |
| Firmware | UEFI | Required |
| Disk | SATA, NVMe, virtio | Targeted |
| Filesystem | ZFS | Required |
| Virtual platform | QEMU/Proxmox | First validation lane |
| Physical graphics | One tested Intel generation and one tested AMD generation | To be selected and recorded |
| Network | Wired Ethernet | First physical lane |
| Wi-Fi | Specific tested Intel adapters | To be selected and recorded |
| NVIDIA | Unsupported or experimental | Not an alpha requirement |
| ARM64 | Out of scope | Not supported |
| Apple hardware | Out of scope | Not supported |

## Test systems

The initial development plan uses:

| System | Purpose | Recommended allocation |
| --- | --- | --- |
| `NSTAR-DEV01` | Interactive development and desktop testing | 8 vCPU, 16 GB RAM, 100 GB ZFS disk |
| `NSTAR-BLD01` | Poudriere packages and image building | 16 vCPU, 32 GB RAM, 300 GB ZFS disk |
| `NSTAR-TEST01` | Disposable release validation | 4-8 vCPU, 8-16 GB RAM, 64 GB disk |

`DEV01` and `BLD01` may share a VM while the project is small. Separate them before package/image builds become destructive or materially time-consuming.

## Matrix entry requirements

Each physical entry must record the exact FreeBSD release, kernel, CPU, GPU, firmware mode, display connector, storage, network adapter, driver/module, compositor result, suspend/resume result, and known limitations. Unsupported NVIDIA Wayland behavior must not be hidden by a generic "Linux desktop" claim.

## Alpha-readiness inventory

Run the bounded, read-only inventory before opening or updating a matrix entry:

```sh
make alpha-readiness ALPHA_OUTPUT=/tmp/northstar-alpha-readiness.conf
```

The schema-1 record contains only normalized capability classes and counts. It
does not include MAC addresses, serial numbers, raw PCI listings, command
lines, environment values, interface names, or user paths.

- `alpha_status=ready` is limited to a FreeBSD 15.1 amd64 UEFI/ZFS physical
  system with direct card and render nodes, an identified Intel or AMD DRM
  driver, wired networking, audio, and input.
- `alpha_status=supplemental` records a valid virtual base lane without
  claiming direct DRM/KMS. It may support development and noVNC testing, but
  cannot close the Intel or AMD matrix gate.
- `alpha_status=blocked` lists the missing or unsupported capability classes.
- `matrix_claim` is `intel`, `amd`, `vm`, or `none`; a report is only an
  inventory input. A physical claim still requires the interactive evidence
  listed above.

Use `sh tools/collect-alpha-readiness.sh --require-ready` only on a candidate
physical alpha machine. That option exits unsuccessfully for supplemental and
blocked systems.
