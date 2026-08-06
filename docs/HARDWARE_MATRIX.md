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
