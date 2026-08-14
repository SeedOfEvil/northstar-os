#!/bin/sh

# Exercise the installer's ZFS-label reset algorithm against real FreeBSD GEOM,
# ZFS, and gpart using only a newly created file-backed md device.
set -eu

fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
[ "$(id -u)" -eq 0 ] || fail 'run this smoke test as root'
[ "$(uname -s)" = FreeBSD ] || fail 'FreeBSD is required'

work=$(mktemp -d /var/tmp/northstar-installer-zfs-reset.XXXXXX)
disk=$work/target.img
log=$work/smoke.log
md=
pool=nstar_reset_$$
original_flags=$(sysctl -n kern.geom.debugflags)
flags_changed=0

cleanup() {
    status=$?
    set +e
    if zpool list -H -o name "$pool" >/dev/null 2>&1; then
        zpool export "$pool" >> "$log" 2>&1 \
            || zpool destroy -f "$pool" >> "$log" 2>&1
    fi
    if [ "$flags_changed" -eq 1 ]; then
        sysctl "kern.geom.debugflags=$original_flags" >> "$log" 2>&1
    fi
    case "$md" in
        md[0-9]*)
            gpart destroy -F "$md" >> "$log" 2>&1 || true
            mdconfig -d -u "${md#md}" >> "$log" 2>&1 || true
            ;;
    esac
    rm -f "$disk"
    if [ "$status" -ne 0 ]; then cat "$log" >&2; fi
    rm -rf "$work"
    trap - EXIT HUP INT TERM
    exit "$status"
}
trap cleanup EXIT HUP INT TERM

exec 3>&1
exec > "$log" 2>&1
truncate -s 2G "$disk"
md=$(mdconfig -a -t vnode -f "$disk")
case "$md" in md[0-9]*) ;; *) fail "unsafe md provider: $md" ;; esac
mdconfig -lv | grep -F "$md" | grep -F "$disk" >/dev/null \
    || fail 'md provider is not backed by the test image'

gpart create -s gpt "$md"
gpart add -a 1m -t freebsd-zfs "$md"
mkdir "$work/root"
zpool create -f -o altroot="$work/root" -o cachefile=none \
    -O mountpoint=none "$pool" "/dev/${md}p1"
zfs create -o mountpoint=/probe "$pool/probe"
printf '%s\n' northstar > "$work/root/probe/evidence.txt"
zpool export "$pool"

required_flags=$((original_flags | 16))
sysctl "kern.geom.debugflags=$required_flags"
flags_changed=1
[ "$(sysctl -n kern.geom.debugflags)" = "$required_flags" ] \
    || fail 'GEOM write authorization was not enabled'

zpool labelclear -f "/dev/${md}p1" \
    || fail 'partition ZFS label clearing failed'
# A partition-backed pool normally has no whole-disk label. The production
# executor therefore treats this additional cleanup as best effort.
zpool labelclear -f "/dev/$md" >/dev/null 2>&1 || true
gpart destroy -F "$md"
gpart create -s gpt "$md"

sysctl "kern.geom.debugflags=$original_flags"
flags_changed=0
[ "$(sysctl -n kern.geom.debugflags)" = "$original_flags" ] \
    || fail 'GEOM write authorization was not restored'
gpart show "$md" | grep -F 'GPT' >/dev/null \
    || fail 'replacement GPT was not created'

printf 'PASS: real FreeBSD ZFS target reset and GPT replacement succeeded on %s\n' "$md" >&3
