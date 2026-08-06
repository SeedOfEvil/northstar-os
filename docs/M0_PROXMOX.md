# M0 Proxmox validation

This runbook installs stock FreeBSD 15.1 amd64 in a Proxmox VM and then runs
the Northstar M0 bootstrap. It does not build a Northstar image, configure a
GitHub remote, or copy project files into the FreeBSD base system.

## Download and verify the installer

Use the official `disc1` installer ISO. It is the appropriate M0 input because
the bootstrap obtains the desktop packages from the configured FreeBSD
repository after installation. The official media directory and checksums are
recorded in [`image/manifests/freebsd-15.1-amd64-installer.lock`](../image/manifests/freebsd-15.1-amd64-installer.lock).

From PowerShell in the repository root:

```powershell
$MediaDir = Join-Path (Get-Location) '.artifacts\freebsd\15.1'
$IsoName = 'FreeBSD-15.1-RELEASE-amd64-disc1.iso'
$Iso = Join-Path $MediaDir $IsoName
$BaseUrl = 'https://download.freebsd.org/releases/amd64/amd64/ISO-IMAGES/15.1'
$Sha256File = Join-Path $MediaDir 'CHECKSUM.SHA256-FreeBSD-15.1-RELEASE-amd64'
$Sha512File = Join-Path $MediaDir 'CHECKSUM.SHA512-FreeBSD-15.1-RELEASE-amd64'

New-Item -ItemType Directory -Force -Path $MediaDir | Out-Null
Invoke-WebRequest "$BaseUrl/$IsoName" -OutFile $Iso
Invoke-WebRequest "$BaseUrl/CHECKSUM.SHA256-FreeBSD-15.1-RELEASE-amd64" -OutFile $Sha256File
Invoke-WebRequest "$BaseUrl/CHECKSUM.SHA512-FreeBSD-15.1-RELEASE-amd64" -OutFile $Sha512File

$PublishedSha256 = (((Select-String -LiteralPath $Sha256File -Pattern ([regex]::Escape($IsoName))).Line -split '=', 2)[1]).Trim().ToLowerInvariant()
$ExpectedSha256 = 'fa27646f05a1440fd26ffbb85e06a50bc86e128242a4e9cb7bb3ea76e1aa5fd9'
$ActualSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $Iso).Hash.ToLowerInvariant()
if ($PublishedSha256 -ne $ExpectedSha256 -or $ActualSha256 -ne $ExpectedSha256) {
    throw "SHA256 mismatch for $IsoName: published $PublishedSha256 actual $ActualSha256"
}

$Sha512Line = (Select-String -LiteralPath $Sha512File -Pattern ([regex]::Escape($IsoName))).Line
$ExpectedSha512 = (($Sha512Line -split '=', 2)[1]).Trim().ToLowerInvariant()
$ActualSha512 = (Get-FileHash -Algorithm SHA512 -LiteralPath $Iso).Hash.ToLowerInvariant()
if ([string]::IsNullOrWhiteSpace($ExpectedSha512) -or $ActualSha512 -ne $ExpectedSha512) {
    throw "SHA512 mismatch for $IsoName: $ActualSha512"
}

"Verified $IsoName"
"SHA256 $ActualSha256"
"SHA512 $ActualSha512"
```

After the command succeeds, copy the ISO to Proxmox through the web UI's
storage **ISO Images** upload action, or with `scp` to the node's ISO storage:

```powershell
scp $Iso root@<proxmox-node>:/var/lib/vz/template/iso/
```

If the node uses different storage, use that storage's ISO upload path. Verify
the checksum again on Proxmox before attaching the media to the VM.

## Create the Proxmox VM

Create `NSTAR-DEV01` with these defaults:

| Setting | Value |
| --- | --- |
| Firmware | OVMF/UEFI |
| Machine | Q35 |
| CPU | 8 vCPUs |
| Memory | 16 GB |
| Disk | 100 GB, SCSI or VirtIO-SCSI |
| Network | VirtIO on the existing LAN bridge, normally `vmbr0` |
| Installer | The verified `disc1.iso` |
| Secure Boot | Disabled for the initial validation VM |

Attach the ISO, boot from it, and install FreeBSD 15.1 amd64 using the
installer's guided ZFS/root-on-ZFS layout. Create the development user during
installation. Configure working network access so `pkg update` can reach the
official repository.

Do not install `drm-kmod` as part of M0. GPU modules remain hardware-specific.

## Transfer the local repository without a remote

Once the VM has network access and SSH is available, create a Git bundle on
the Windows checkout:

```powershell
$Bundle = Join-Path (Get-Location) '.artifacts\northstar-main.bundle'
git bundle create $Bundle main
scp $Bundle <development-user>@<freebsd-ip>:/tmp/northstar-main.bundle
```

On FreeBSD, install Git if the base system does not provide it, then clone the
bundle locally:

```sh
sudo pkg install git
git clone /tmp/northstar-main.bundle "$HOME/src/northstar"
cd "$HOME/src/northstar"
git remote remove origin
git remote -v
```

The final command should produce no output. The bundle preserves the local
commit history without creating a GitHub or other network remote.

## Run M0 validation

From the FreeBSD checkout, with the selected account in place:

```sh
make check-host
sudo make bootstrap NORTHSTAR_USER=<development-user>
sudo make bootstrap NORTHSTAR_USER=<development-user>
```

Log out and back in to refresh the `video` group membership. As the
unprivileged development user, start Wayfire through D-Bus:

```sh
dbus-run-session -- wayfire
```

Inside the Wayfire session, run:

```sh
make vm-smoke
sh tests/vm/m0-smoke.sh --launch
QT_QPA_PLATFORM=wayland qterminal
MOZ_ENABLE_WAYLAND=1 firefox
xterm
make diagnostics OUTPUT=/tmp/northstar-m0-diagnostics
```

Inspect the bootstrap capture and diagnostics. Confirm that the package
versions are resolved, `dbus` and `seatd` are enabled/running, and no
Northstar files were placed in `/usr/bin` or `/usr/lib`.

## Acceptance checklist

- [ ] Official ISO SHA256 and SHA512 checks pass before upload.
- [ ] Proxmox VM boots in UEFI mode.
- [ ] FreeBSD 15.1 amd64 is installed on root-on-ZFS.
- [ ] `make check-host` passes.
- [ ] The first bootstrap succeeds.
- [ ] The second bootstrap is idempotent.
- [ ] Wayfire starts as the unprivileged user through D-Bus.
- [ ] QTerminal launches through Wayland.
- [ ] Firefox launches with Wayland enabled.
- [ ] `xterm` launches through Xwayland.
- [ ] Captures contain package versions and service state.
- [ ] Diagnostics contain no credentials, tokens, keys, or sensitive environment values.
- [ ] No Northstar files are present in `/usr/bin` or `/usr/lib`.
- [ ] The Windows checkout still has no Git remote configured.

## Recovery

If the ISO hash fails, delete the incomplete local artifact and download it
again from the official directory. Do not upload an unverified image.

If the VM does not boot, confirm OVMF/UEFI, the attached ISO, and the VM boot
order before changing the FreeBSD installation. If the package preflight
fails, repair repository connectivity or package availability and rerun the
bootstrap; it stops before package, service, or group mutations when
preflight fails.
