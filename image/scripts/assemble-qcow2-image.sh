#!/bin/sh

# Assemble the first Northstar UEFI/root-on-ZFS QCOW2 image from verified,
# offline inputs. Production mode is FreeBSD/root-only and requires an explicit
# disposable-builder marker. The script never accepts an existing disk device.

set -eu

RESOLVED=
ARTIFACTS=
RUNTIME=
OUTPUT=
PROJECT_ROOT=
PROJECT_COMMIT=
BUILDER_MARKER=/etc/northstar/disposable-image-builder.conf
DISK_SIZE_GB=16
PREFLIGHT=0
DEVELOPMENT_AUTOLOGIN=0
STAGING=
MD_DEVICE=
POOL=
MOUNT_ROOT=
EFI_MOUNTED=0
DEVFS_MOUNTED=0
PACKAGE_MOUNTED=0
POOL_CREATED=0
SUCCESS=0

usage() {
    cat <<'USAGE'
Usage: assemble-qcow2-image.sh --resolved-inputs DIR --artifacts DIR \
  --runtime-bundle DIR --output NEW_DIRECTORY --project-root CLEAN_CHECKOUT \
  --project-commit FULL_COMMIT [--disk-size-gb 16] \
  [--builder-marker FILE] [--development-autologin] [--preflight]

Preflight validates every immutable input without root or disk mutation.
Production mode requires FreeBSD root, a protected disposable-builder marker,
and qemu-img. It creates only a new file-backed md device owned by this run.
USAGE
}

die() { printf 'ERROR: %s\n' "$1" >&2; exit 1; }

cleanup() {
    set +e
    if [ "$PACKAGE_MOUNTED" -eq 1 ] && [ -n "$MOUNT_ROOT" ]; then
        umount "$MOUNT_ROOT/.northstar-packages" >/dev/null 2>&1
    fi
    if [ "$DEVFS_MOUNTED" -eq 1 ] && [ -n "$MOUNT_ROOT" ]; then
        umount "$MOUNT_ROOT/dev" >/dev/null 2>&1
    fi
    if [ "$EFI_MOUNTED" -eq 1 ] && [ -n "$MOUNT_ROOT" ]; then
        umount "$MOUNT_ROOT/boot/efi" >/dev/null 2>&1
    fi
    if [ "$POOL_CREATED" -eq 1 ] && [ -n "$POOL" ]; then
        zpool export "$POOL" >/dev/null 2>&1
    fi
    if [ -n "$MD_DEVICE" ]; then
        mdconfig -d -u "${MD_DEVICE#md}" >/dev/null 2>&1
    fi
    if [ "$SUCCESS" -ne 1 ] && [ -n "$STAGING" ] && [ -d "$STAGING" ]; then
        rm -rf "$STAGING"
    fi
}
trap cleanup EXIT HUP INT TERM

while [ "$#" -gt 0 ]; do
    case "$1" in
        --resolved-inputs) RESOLVED=${2-}; shift 2 ;;
        --artifacts) ARTIFACTS=${2-}; shift 2 ;;
        --runtime-bundle) RUNTIME=${2-}; shift 2 ;;
        --output) OUTPUT=${2-}; shift 2 ;;
        --project-root) PROJECT_ROOT=${2-}; shift 2 ;;
        --project-commit) PROJECT_COMMIT=${2-}; shift 2 ;;
        --builder-marker) BUILDER_MARKER=${2-}; shift 2 ;;
        --disk-size-gb) DISK_SIZE_GB=${2-}; shift 2 ;;
        --development-autologin) DEVELOPMENT_AUTOLOGIN=1; shift ;;
        --preflight) PREFLIGHT=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

for command_name in awk basename cp cut dirname git grep mkdir mktemp mv rm sort stat tr uname wc; do
    command -v "$command_name" >/dev/null 2>&1 \
        || die "required preflight command is unavailable: $command_name"
done
if command -v sha256 >/dev/null 2>&1; then
    SHA256_COMMAND=sha256
elif command -v sha256sum >/dev/null 2>&1; then
    SHA256_COMMAND=sha256sum
else
    die 'sha256 or sha256sum is required'
fi

file_sha256() {
    if [ "$SHA256_COMMAND" = sha256 ]; then sha256 -q "$1"; else sha256sum "$1" | awk '{ print $1 }'; fi
}
file_size() {
    if [ "$(uname -s)" = FreeBSD ]; then stat -f '%z' "$1"; else wc -c < "$1" | tr -d ' '; fi
}
config_count() { awk -F= -v key="$2" '$1 == key { count++ } END { print count + 0 }' "$1"; }
config_value() { awk -F= -v key="$2" '$1 == key { sub(/^[^=]*=/, ""); print; exit }' "$1"; }
required_value() {
    [ "$(config_count "$1" "$2")" -eq 1 ] || die "configuration key must appear once: $2"
    value=$(config_value "$1" "$2")
    [ -n "$value" ] || die "configuration key is empty: $2"
    printf '%s' "$value"
}

[ -d "$RESOLVED" ] && [ ! -L "$RESOLVED" ] || die 'resolved inputs must be a real directory'
[ -d "$ARTIFACTS" ] && [ ! -L "$ARTIFACTS" ] || die 'artifacts must be a real directory'
[ -d "$RUNTIME" ] && [ ! -L "$RUNTIME" ] || die 'runtime bundle must be a real directory'
[ -d "$PROJECT_ROOT/.git" ] && [ ! -L "$PROJECT_ROOT" ] || die 'project root must be a real Git checkout'
[ -n "$OUTPUT" ] && [ ! -e "$OUTPUT" ] && [ ! -L "$OUTPUT" ] || die 'output must not exist'
printf '%s\n' "$PROJECT_COMMIT" | grep -Eq '^[0-9a-f]{40}$' || die 'project commit must be a full lowercase Git revision'
case "$DISK_SIZE_GB" in ''|*[!0-9]*) die 'disk size must be an integer GiB value' ;; esac
[ "$DISK_SIZE_GB" -ge 12 ] && [ "$DISK_SIZE_GB" -le 64 ] || die 'disk size must be between 12 and 64 GiB'
PROJECT_ROOT=$(CDPATH= cd -- "$PROJECT_ROOT" && pwd)
[ "$(git -C "$PROJECT_ROOT" rev-parse HEAD)" = "$PROJECT_COMMIT" ] || die 'project checkout HEAD does not match project commit'
[ -z "$(git -C "$PROJECT_ROOT" status --porcelain)" ] || die 'project checkout is dirty'

resolved_conf=$RESOLVED/resolved-image-inputs.conf
input_lock=$RESOLVED/input.lock
artifact_records=$RESOLVED/artifact-records
for path in "$resolved_conf" "$input_lock" "$artifact_records"; do
    [ -f "$path" ] && [ ! -L "$path" ] || die "resolved input file is missing or unsafe: $(basename "$path")"
done
[ "$(required_value "$resolved_conf" schema_version)" = 1 ] || die 'unsupported resolved-input schema'
[ "$(required_value "$resolved_conf" target_format)" = qcow2 ] || die 'resolved target is not qcow2'
[ "$(required_value "$resolved_conf" project_commit)" = "$PROJECT_COMMIT" ] || die 'resolved inputs target a different project commit'
[ "$(file_sha256 "$input_lock")" = "$(required_value "$resolved_conf" input_lock_sha256)" ] || die 'resolved input lock digest mismatch'
[ "$(file_sha256 "$artifact_records")" = "$(required_value "$resolved_conf" artifact_records_sha256)" ] || die 'artifact-record digest mismatch'
[ "$(required_value "$resolved_conf" freebsd_release)" = 15.1-RELEASE ] || die 'unsupported FreeBSD release'
[ "$(required_value "$resolved_conf" freebsd_arch)" = amd64 ] || die 'unsupported FreeBSD architecture'

artifact_count=0
while IFS='|' read -r filename digest size extra; do
    [ -z "$extra" ] || die 'artifact record has extra fields'
    printf '%s\n' "$filename" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9._+-]*$' || die 'artifact filename is unsafe'
    printf '%s\n' "$digest" | grep -Eq '^[0-9a-f]{64}$' || die 'artifact digest is unsafe'
    printf '%s\n' "$size" | grep -Eq '^[0-9]+$' || die 'artifact size is unsafe'
    path=$ARTIFACTS/$filename
    [ -f "$path" ] && [ ! -L "$path" ] || die "artifact is missing or unsafe: $filename"
    [ "$(file_sha256 "$path")" = "$digest" ] || die "artifact digest mismatch: $filename"
    actual_size=$(file_size "$path")
    [ "$actual_size" = "$size" ] \
        || die "artifact size mismatch: $filename expected=$size actual=$actual_size"
    artifact_count=$((artifact_count + 1))
done < "$artifact_records"
[ "$artifact_count" -eq 3 ] || die 'resolved image must contain exactly three primary artifacts'

runtime_conf=$RUNTIME/runtime-bundle.conf
runtime_records=$RUNTIME/runtime-package-records
runtime_packages=$RUNTIME/packages
[ -f "$runtime_conf" ] && [ ! -L "$runtime_conf" ] || die 'runtime bundle configuration is missing or unsafe'
[ -f "$runtime_records" ] && [ ! -L "$runtime_records" ] || die 'runtime package records are missing or unsafe'
[ -d "$runtime_packages" ] && [ ! -L "$runtime_packages" ] || die 'runtime package directory is missing or unsafe'
[ "$(required_value "$runtime_conf" schema_version)" = 1 ] || die 'unsupported runtime bundle schema'
[ "$(required_value "$runtime_conf" freebsd_abi)" = 'FreeBSD:15:amd64' ] || die 'runtime bundle ABI is unsupported'
[ "$(required_value "$runtime_conf" source_date_epoch)" = "$(required_value "$resolved_conf" source_date_epoch)" ] || die 'runtime and image epochs differ'
[ "$(file_sha256 "$runtime_records")" = "$(required_value "$runtime_conf" runtime_package_records_sha256)" ] || die 'runtime package-record digest mismatch'
expected_package_count=$(required_value "$runtime_conf" package_count)
printf '%s\n' "$expected_package_count" | grep -Eq '^[0-9]+$' || die 'runtime package count is unsafe'

package_count=0
northstar_found=0
compat_found=0
while IFS='|' read -r filename digest size name version origin extra; do
    [ -z "$extra" ] || die 'runtime package record has extra fields'
    printf '%s\n' "$filename" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9._+~,-]*\.pkg$' || die 'runtime package filename is unsafe'
    printf '%s\n' "$digest" | grep -Eq '^[0-9a-f]{64}$' || die 'runtime package digest is unsafe'
    printf '%s\n' "$size" | grep -Eq '^[0-9]+$' || die 'runtime package size is unsafe'
    printf '%s\n' "$name" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9+_.-]*$' || die 'runtime package name is unsafe'
    printf '%s\n' "$version" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9+_.~,:-]*$' || die 'runtime package version is unsafe'
    printf '%s\n' "$origin" | grep -Eq '^[A-Za-z0-9_.+~-]+/[A-Za-z0-9_.+~-]+$' || die 'runtime package origin is unsafe'
    path=$runtime_packages/$filename
    [ -f "$path" ] && [ ! -L "$path" ] || die "runtime package is missing or unsafe: $filename"
    [ "$(file_sha256 "$path")" = "$digest" ] || die "runtime package digest mismatch: $filename"
    actual_size=$(file_size "$path")
    [ "$actual_size" = "$size" ] \
        || die "runtime package size mismatch: $filename expected=$size actual=$actual_size"
    [ "$name" != northstar ] || northstar_found=1
    [ "$name" != northstar-wayfire-nested ] || compat_found=1
    package_count=$((package_count + 1))
done < "$runtime_records"
[ "$package_count" -eq "$expected_package_count" ] || die 'runtime package count does not match its manifest'
[ "$northstar_found" -eq 1 ] || die 'runtime bundle omits Northstar'
[ "$compat_found" -eq 1 ] || die 'runtime bundle omits the scfb compatibility compositor'
actual_package_files=0
for package_path in "$runtime_packages"/*.pkg; do
    [ -f "$package_path" ] || continue
    actual_package_files=$((actual_package_files + 1))
done
[ "$actual_package_files" -eq "$package_count" ] || die 'runtime package directory contains unrecorded or duplicate artifacts'

if [ "$PREFLIGHT" -eq 1 ]; then
    printf 'PASS: verified PR76 QCOW2 assembly inputs (%s packages, %s GiB)\n' "$package_count" "$DISK_SIZE_GB"
    exit 0
fi

[ "$(uname -s)" = FreeBSD ] || die 'QCOW2 assembly requires FreeBSD'
[ "$(id -u)" -eq 0 ] || die 'QCOW2 assembly must run as root on the disposable builder'
for command_name in cat chmod chown chroot devfs env gpart ln mdconfig mount mount_msdosfs mount_nullfs newfs_msdos pkg pkg-static pw qemu-img sysrc tar truncate umount zfs zpool; do
    command -v "$command_name" >/dev/null 2>&1 || die "required builder command is unavailable: $command_name"
done
[ -f "$BUILDER_MARKER" ] && [ ! -L "$BUILDER_MARKER" ] || die 'disposable-builder marker is missing or unsafe'
[ "$(stat -f '%u' "$BUILDER_MARKER")" -eq 0 ] || die 'disposable-builder marker must be root-owned'
marker_mode=$(stat -f '%Lp' "$BUILDER_MARKER")
case "$marker_mode" in 400|600) : ;; *) die 'disposable-builder marker mode must be 0400 or 0600' ;; esac
[ "$(required_value "$BUILDER_MARKER" SCHEMA_VERSION)" = 1 ] || die 'unsupported builder marker schema'
[ "$(required_value "$BUILDER_MARKER" PURPOSE)" = northstar-disposable-image-builder ] || die 'builder marker purpose is invalid'
[ "$(required_value "$BUILDER_MARKER" ALLOW_DISK_IMAGE_ASSEMBLY)" = YES ] || die 'builder marker does not authorize image assembly'
builder_id=$(required_value "$BUILDER_MARKER" BUILDER_ID)
printf '%s\n' "$builder_id" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9._-]{2,63}$' || die 'builder ID is unsafe'

output_parent=$(dirname "$OUTPUT")
mkdir -p "$output_parent"
output_parent=$(CDPATH= cd -- "$output_parent" && pwd)
OUTPUT=$output_parent/$(basename "$OUTPUT")
STAGING=$(mktemp -d "$output_parent/.northstar-qcow2.XXXXXX")
MOUNT_ROOT=$STAGING/root
mkdir "$MOUNT_ROOT" "$STAGING/empty-repos"
raw=$STAGING/northstar-15.1-amd64.raw
qcow2=$STAGING/northstar-15.1-amd64.qcow2
truncate -s "${DISK_SIZE_GB}G" "$raw"
MD_DEVICE=$(mdconfig -a -t vnode -f "$raw")
printf '%s\n' "$MD_DEVICE" | grep -Eq '^md[0-9]+$' || die 'mdconfig returned an unsafe device identity'
gpart create -s gpt "$MD_DEVICE"
gpart add -a 1m -s 260m -t efi "$MD_DEVICE"
gpart add -a 1m -t freebsd-zfs "$MD_DEVICE"
gpart show "$MD_DEVICE" > "$STAGING/partition-layout.txt"

# FreeBSD's automatic geometry can choose 16 KiB clusters for this small EFI
# partition, leaving fewer than FAT32's required 65,525 clusters. Pin one
# sector per cluster so the 260 MiB ESP is valid across supported builders.
newfs_msdos -F 32 -c 1 -L NSTAR_EFI "/dev/${MD_DEVICE}p1" >/dev/null
POOL=nstar_$(printf '%s' "$PROJECT_COMMIT" | cut -c1-12)
zpool create -f -o altroot="$MOUNT_ROOT" -o cachefile=none -o ashift=12 \
    -O mountpoint=none -O compression=lz4 -O atime=off -O canmount=off \
    "$POOL" "/dev/${MD_DEVICE}p2"
POOL_CREATED=1
zfs create -o mountpoint=none "$POOL/ROOT"
zfs create -o mountpoint=/ -o canmount=noauto "$POOL/ROOT/default"
zfs mount "$POOL/ROOT/default"
zfs create -o mountpoint=/home "$POOL/home"
zfs create -o mountpoint=/var "$POOL/var"
zfs create -o mountpoint=/tmp -o setuid=off "$POOL/tmp"
zpool set bootfs="$POOL/ROOT/default" "$POOL"
zfs list -r -o name,used,avail,refer,mountpoint,canmount "$POOL" > "$STAGING/zfs-layout.txt"

base_name=$(awk -F'|' '$1 == "base.txz" { print $1 }' "$artifact_records")
kernel_name=$(awk -F'|' '$1 == "kernel.txz" { print $1 }' "$artifact_records")
[ "$base_name" = base.txz ] && [ "$kernel_name" = kernel.txz ] || die 'resolved release sets are incomplete'
tar -xpf "$ARTIFACTS/$base_name" -C "$MOUNT_ROOT"
tar -xpf "$ARTIFACTS/$kernel_name" -C "$MOUNT_ROOT"
mkdir -p "$MOUNT_ROOT/boot/efi" "$MOUNT_ROOT/boot/zfs" "$MOUNT_ROOT/dev"
mount_msdosfs "/dev/${MD_DEVICE}p1" "$MOUNT_ROOT/boot/efi"
EFI_MOUNTED=1
mkdir -p "$MOUNT_ROOT/boot/efi/EFI/BOOT"
cp "$MOUNT_ROOT/boot/loader.efi" "$MOUNT_ROOT/boot/efi/EFI/BOOT/BOOTX64.EFI"
zpool set cachefile="$MOUNT_ROOT/boot/zfs/zpool.cache" "$POOL"
devfs -m "$MOUNT_ROOT/dev" rule applyset 0 >/dev/null 2>&1 || true
mount -t devfs devfs "$MOUNT_ROOT/dev"
DEVFS_MOUNTED=1

package_mount=$MOUNT_ROOT/.northstar-packages
package_bootstrap=$MOUNT_ROOT/tmp/northstar-pkg-static
mkdir -p "$package_mount" "$MOUNT_ROOT/tmp/empty-repos"
mount_nullfs -o ro "$runtime_packages" "$package_mount"
PACKAGE_MOUNTED=1
cp "$(command -v pkg-static)" "$package_bootstrap"
chmod 0555 "$package_bootstrap"

set --
while IFS='|' read -r filename _rest; do
    set -- "$@" "/.northstar-packages/$filename"
done < "$runtime_records"
ASSUME_ALWAYS_YES=yes chroot "$MOUNT_ROOT" /tmp/northstar-pkg-static \
    -o REPOS_DIR=/tmp/empty-repos add -M "$@"

installed_package_count=$(chroot "$MOUNT_ROOT" /tmp/northstar-pkg-static \
    query '%n|%v' | sort -u | wc -l | tr -d ' ')
[ "$installed_package_count" -eq "$package_count" ] \
    || die "installed runtime package count mismatch: expected=$package_count actual=$installed_package_count"
while IFS='|' read -r _filename _digest _size name version _origin _extra; do
    chroot "$MOUNT_ROOT" /tmp/northstar-pkg-static info -e "$name-$version" >/dev/null \
        || die "runtime package was not installed exactly: $name-$version"
done < "$runtime_records"

umount "$package_mount"
PACKAGE_MOUNTED=0
rm -rf "$package_mount" "$package_bootstrap" "$MOUNT_ROOT/tmp/empty-repos"

pw -R "$MOUNT_ROOT" useradd northstar -u 1001 -c 'Northstar User' \
    -d /home/northstar -m -s /bin/sh -G wheel,video -w no
chown -R 1001:1001 "$MOUNT_ROOT/home/northstar"
mkdir -p "$MOUNT_ROOT/home/northstar/.config"
cp "$PROJECT_ROOT/config/wayfire-nested.ini" "$MOUNT_ROOT/home/northstar/.config/wayfire.ini"
chown -R 1001:1001 "$MOUNT_ROOT/home/northstar/.config"

cat > "$MOUNT_ROOT/etc/rc.conf" <<'EOF'
hostname="northstar-image"
ifconfig_DEFAULT="DHCP"
ifconfig_vtnet0="DHCP"
dbus_enable="YES"
seatd_enable="YES"
sddm_enable="YES"
sshd_enable="YES"
zfs_enable="YES"
EOF
cat > "$MOUNT_ROOT/boot/loader.conf" <<'EOF'
zfs_load="YES"
kern.geom.label.disk_ident.enable="0"
EOF
cat > "$MOUNT_ROOT/etc/fstab" <<'EOF'
/dev/msdosfs/NSTAR_EFI /boot/efi msdosfs rw,noatime 0 0
EOF
mkdir -p "$MOUNT_ROOT/usr/local/etc/sddm.conf.d"
cp "$PROJECT_ROOT/config/sddm/northstar-proxmox.conf" \
    "$MOUNT_ROOT/usr/local/etc/sddm.conf.d/20-northstar-proxmox.conf"
if [ ! -e "$MOUNT_ROOT/usr/local/bin/sddm-greeter" ] && \
    [ -x "$MOUNT_ROOT/usr/local/bin/sddm-greeter-qt6" ]; then
    ln -s sddm-greeter-qt6 "$MOUNT_ROOT/usr/local/bin/sddm-greeter"
fi
if [ "$DEVELOPMENT_AUTOLOGIN" -eq 1 ]; then
    cat > "$MOUNT_ROOT/usr/local/etc/sddm.conf.d/30-northstar-development-autologin.conf" <<'EOF'
[Autologin]
User=northstar
Session=northstar-proxmox.desktop
Relogin=false
EOF
fi
mkdir -p "$MOUNT_ROOT/var/db/northstar"
printf '%s\n' \
    'schema_version=1' \
    'image_channel=development' \
    "project_commit=$PROJECT_COMMIT" \
    "development_autologin=$DEVELOPMENT_AUTOLOGIN" \
    > "$MOUNT_ROOT/var/db/northstar/image-build.conf"

umount "$MOUNT_ROOT/dev"
DEVFS_MOUNTED=0
umount "$MOUNT_ROOT/boot/efi"
EFI_MOUNTED=0
zpool export "$POOL"
POOL_CREATED=0
mdconfig -d -u "${MD_DEVICE#md}"
MD_DEVICE=

qemu-img convert -f raw -O qcow2 -o compat=1.1,lazy_refcounts=off,cluster_size=65536 "$raw" "$qcow2"
qemu-img check -f qcow2 "$qcow2" > "$STAGING/qcow2-check.txt"
qemu-img info -f qcow2 "$qcow2" > "$STAGING/qcow2-info.txt"
qcow2_sha256=$(file_sha256 "$qcow2")
qcow2_size=$(file_size "$qcow2")
builder_marker_sha256=$(file_sha256 "$BUILDER_MARKER")
runtime_records_sha256=$(file_sha256 "$runtime_records")
resolved_sha256=$(file_sha256 "$resolved_conf")
printf '%s\n' \
    'schema_version=1' \
    'artifact=northstar-15.1-amd64.qcow2' \
    "artifact_sha256=$qcow2_sha256" \
    "artifact_size=$qcow2_size" \
    "virtual_size_gib=$DISK_SIZE_GB" \
    'firmware=UEFI' \
    'partition_table=GPT' \
    'root_filesystem=ZFS' \
    "zpool=$POOL" \
    "project_commit=$PROJECT_COMMIT" \
    "builder_id=$builder_id" \
    "builder_marker_sha256=$builder_marker_sha256" \
    "resolved_inputs_sha256=$resolved_sha256" \
    "runtime_package_records_sha256=$runtime_records_sha256" \
    "runtime_package_count=$package_count" \
    "development_autologin=$DEVELOPMENT_AUTOLOGIN" \
    > "$STAGING/image-provenance.conf"
cp "$resolved_conf" "$STAGING/resolved-image-inputs.conf"
cp "$runtime_records" "$STAGING/runtime-package-records"
rm -rf "$MOUNT_ROOT" "$STAGING/empty-repos" "$raw"
chmod 0444 "$qcow2" "$STAGING/image-provenance.conf" \
    "$STAGING/partition-layout.txt" "$STAGING/zfs-layout.txt" \
    "$STAGING/qcow2-check.txt" "$STAGING/qcow2-info.txt" \
    "$STAGING/resolved-image-inputs.conf" "$STAGING/runtime-package-records"
mv "$STAGING" "$OUTPUT"
STAGING=
SUCCESS=1
printf 'PASS: assembled verified Northstar QCOW2 image at %s\n' "$OUTPUT"
