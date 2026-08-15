#!/bin/sh

# Reproduce the rejected r84 install against an explicitly confirmed,
# disposable VirtIO disk. This test is destructive only to that exact disk.
set -eu

fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
usage() {
    printf 'usage: %s --device DEVICE --expected-bytes BYTES --confirm-device DEVICE\n' "${0##*/}" >&2
    exit 64
}

device=
expected_bytes=
confirmation=
while [ "$#" -gt 0 ]; do
    case "$1" in
        --device) [ "$#" -ge 2 ] || usage; device=$2; shift 2 ;;
        --expected-bytes) [ "$#" -ge 2 ] || usage; expected_bytes=$2; shift 2 ;;
        --confirm-device) [ "$#" -ge 2 ] || usage; confirmation=$2; shift 2 ;;
        *) usage ;;
    esac
done

[ "$(id -u)" -eq 0 ] || fail 'run this VirtIO regression as root'
[ "$(uname -s)" = FreeBSD ] || fail 'FreeBSD is required'
case "$device" in da[0-9]*) ;; *) fail 'only an explicitly named daN provider is accepted' ;; esac
case "$expected_bytes" in ''|*[!0-9]*) fail 'expected byte size is invalid' ;; esac
[ "$confirmation" = "$device" ] || fail 'typed device confirmation does not match'
[ -c "/dev/$device" ] && [ ! -L "/dev/$device" ] || fail 'target is not a direct character device'

info=$(geom disk list "$device") || fail 'could not inspect target disk'
actual_bytes=$(printf '%s\n' "$info" | awk -F: '/^[[:space:]]*Mediasize:/ { sub(/^[[:space:]]*/, "", $2); split($2, a, /[[:space:]]/); print a[1]; exit }')
description=$(printf '%s\n' "$info" | awk -F: '/^[[:space:]]*descr:/ { sub(/^[[:space:]]*/, "", $2); print $2; exit }')
mode=$(printf '%s\n' "$info" | awk -F: '/^[[:space:]]*Mode:/ { gsub(/[[:space:]]/, "", $2); print $2; exit }')
[ "$actual_bytes" = "$expected_bytes" ] || fail 'target size does not match the explicit safety binding'
[ "$description" = 'QEMU QEMU HARDDISK' ] || fail 'target is not the expected QEMU disk'
[ "$mode" = r0w0e0 ] || fail 'target already has open GEOM consumers'
[ ! -e /dev/msdosfs/NSTAR_EFI ] || fail 'NSTAR_EFI alias already exists before fixture creation'
if gpart show "$device" >/dev/null 2>&1; then fail 'target already contains a partition table'; fi
if mount -p | grep -Eq "(^|[[:space:]])(/dev/)?${device}(p[0-9]+)?([[:space:]]|$)"; then
    fail 'target is mounted'
fi
if zpool status -P 2>/dev/null | grep -Eq "(^|[^[:alnum:]_])(/dev/)?${device}(p[0-9]+)?([^[:alnum:]_]|$)"; then
    fail 'target belongs to an imported pool'
fi
if swapinfo -k 2>/dev/null | grep -Eq "(^|[^[:alnum:]_])(/dev/)?${device}(p[0-9]+)?([^[:alnum:]_]|$)"; then
    fail 'target is active swap'
fi

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
work=$(mktemp -d /var/tmp/northstar-installer-virtio-retry.XXXXXX)
mountpoint=$work/efi
log=$work/smoke.log
pool=nstar_virtio_retry_$$
original_flags=$(sysctl -n kern.geom.debugflags)
flags_changed=0
efi_mounted=0

cleanup() {
    status=$?
    trap - EXIT HUP INT TERM
    set +e
    if [ "$efi_mounted" -eq 1 ]; then umount "$mountpoint" >> "$log" 2>&1; fi
    if zpool list -H -o name "$pool" >/dev/null 2>&1; then
        zpool export "$pool" >> "$log" 2>&1 || zpool destroy -f "$pool" >> "$log" 2>&1
    fi
    if [ "$flags_changed" -eq 1 ]; then
        sysctl "kern.geom.debugflags=$original_flags" >> "$log" 2>&1
    fi
    gpart destroy -F "$device" >> "$log" 2>&1 || true
    if [ "$status" -ne 0 ]; then cat "$log" >&2; fi
    rm -rf "$work"
    exit "$status"
}
trap cleanup EXIT HUP INT TERM

exec 3>&1
exec > "$log" 2>&1
mkdir "$mountpoint"
gpart create -s gpt "$device"
gpart add -a 1m -s 64m -t efi -l NSTAR_TEST_EFI "$device"
gpart add -a 1m -t freebsd-zfs -l NSTAR_TEST_ZFS "$device"
newfs_msdos -F 32 -c 1 -L NSTAR_EFI "/dev/${device}p1"
zpool create -f -o cachefile=none -O mountpoint=none "$pool" "/dev/${device}p2"
zpool export "$pool"

geom label status | awk -v component="${device}p1" \
    '$1 == "msdosfs/NSTAR_EFI" && $3 == component { found=1 } END { exit !found }' \
    || fail 'fixture EFI label did not resolve to the VirtIO target'
mount_msdosfs /dev/msdosfs/NSTAR_EFI "$mountpoint"
efi_mounted=1
"$ROOT/apps/installer/northstar-installer-disks" > "$work/disks.out"
awk -F '\t' -v device="$device" \
    '$1 == device && $6 == "yes" && $7 == "no" { found=1 } END { exit !found }' \
    "$work/disks.out" || fail 'label-mounted VirtIO target was not rejected by discovery'

required_flags=$((original_flags | 16))
sysctl "kern.geom.debugflags=$required_flags"
flags_changed=1
zdb -l "/dev/${device}p2" >/dev/null 2>&1 || fail 'fixture ZFS label is unavailable'
zpool labelclear -f "/dev/${device}p2"
if zdb -l "/dev/${device}p2" >/dev/null 2>&1; then fail 'fixture ZFS label remains after clearing'; fi
if gpart destroy -F "$device" > "$work/expected-busy.out" 2>&1; then
    fail 'mounted EFI alias did not block GPT replacement'
fi
grep -F 'Device busy' "$work/expected-busy.out" >/dev/null \
    || fail 'GPT replacement failed for an unexpected reason'

umount "$mountpoint"
efi_mounted=0
if zdb -l "/dev/${device}p2" >/dev/null 2>&1; then
    fail 'interrupted retry fixture unexpectedly regained a ZFS label'
fi
gpart destroy -F "$device"
gpart create -s gpt "$device"
sysctl "kern.geom.debugflags=$original_flags"
flags_changed=0
[ "$(sysctl -n kern.geom.debugflags)" = "$original_flags" ] \
    || fail 'GEOM protection was not restored'
gpart show "$device" | grep -F GPT >/dev/null || fail 'clean retry did not create replacement GPT'

printf 'PASS: real VirtIO alias-mounted failure and already-cleared clean retry succeeded on %s\n' "$device" >&3
