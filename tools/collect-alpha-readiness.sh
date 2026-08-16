#!/bin/sh

# Produce one bounded, machine-readable M6 hardware/session capability record.
# The probe is read-only and intentionally omits serials, MAC addresses, raw
# PCI listings, command lines, environment values, and user-controlled paths.

set -eu

PROG=${0##*/}
OUTPUT=${TMPDIR:-/tmp}/northstar-alpha-readiness.conf
REQUIRE_READY=0
TEST_MODE=${NORTHSTAR_ALPHA_TEST_MODE:-0}

usage() {
    cat <<EOF
Usage: $PROG [--output FILE] [--require-ready]

Collect a privacy-bounded M6 alpha hardware and session readiness record.
Collection always distinguishes supplemental VM graphics from direct DRM/KMS.
--require-ready exits unsuccessfully unless the complete physical alpha lane
is ready; ordinary diagnostics collection does not require it.
EOF
}

fail() {
    printf 'ERROR: %s\n' "$1" >&2
    exit "${2:-2}"
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --output)
            [ "$#" -ge 2 ] || fail '--output requires a value'
            OUTPUT=$2
            shift 2
            ;;
        --require-ready)
            REQUIRE_READY=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            fail "unknown option: $1"
            ;;
    esac
done

case "$OUTPUT" in '') fail 'output path is empty' ;; esac
[ ! -L "$OUTPUT" ] || fail 'output path must not be a symbolic link'
case "$TEST_MODE" in 0|1) ;; *) fail 'test mode must be 0 or 1' ;; esac

safe_token() {
    value=$1
    fallback=$2
    if printf '%s\n' "$value" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9_.:+-]{0,95}$'; then
        printf '%s\n' "$value"
    else
        printf '%s\n' "$fallback"
    fi
}

safe_count() {
    value=$1
    case "$value" in
        ''|*[!0-9]*) printf '0\n' ;;
        *)
            case "$value" in
                ?|??|???) [ "$value" -le 999 ] 2>/dev/null && printf '%s\n' "$value" || printf '999\n' ;;
                *) printf '999\n' ;;
            esac
            ;;
    esac
}

count_devices() {
    directory=$1
    pattern=$2
    if [ ! -d "$directory" ] || ! command -v find >/dev/null 2>&1; then
        printf '0\n'
        return
    fi
    find "$directory" -maxdepth 1 -type c -name "$pattern" -print 2>/dev/null |
        awk 'END { print NR + 0 }'
}

service_running() {
    service_name=$1
    if command -v service >/dev/null 2>&1 && service "$service_name" status >/dev/null 2>&1; then
        printf 'yes\n'
    else
        printf 'no\n'
    fi
}

if [ "$TEST_MODE" = 1 ]; then
    release=${NORTHSTAR_ALPHA_TEST_RELEASE:-15.1-RELEASE}
    architecture=${NORTHSTAR_ALPHA_TEST_ARCH:-amd64}
    boot_method=${NORTHSTAR_ALPHA_TEST_BOOT:-UEFI}
    root_filesystem=${NORTHSTAR_ALPHA_TEST_ROOTFS:-zfs}
    platform_class=${NORTHSTAR_ALPHA_TEST_PLATFORM:-unknown}
    cpu_vendor=${NORTHSTAR_ALPHA_TEST_CPU_VENDOR:-unknown}
    drm_card_count=${NORTHSTAR_ALPHA_TEST_DRM_CARDS:-0}
    drm_render_count=${NORTHSTAR_ALPHA_TEST_DRM_RENDER:-0}
    drm_driver=${NORTHSTAR_ALPHA_TEST_DRM_DRIVER:-none}
    wired_interface_count=${NORTHSTAR_ALPHA_TEST_WIRED:-0}
    wifi_device_count=${NORTHSTAR_ALPHA_TEST_WIFI:-0}
    audio_device_count=${NORTHSTAR_ALPHA_TEST_AUDIO:-0}
    input_device_count=${NORTHSTAR_ALPHA_TEST_INPUT:-0}
    qemu_guest_agent=${NORTHSTAR_ALPHA_TEST_AGENT:-no}
else
    release=unavailable
    architecture=unavailable
    boot_method=unavailable
    root_filesystem=unavailable
    platform_class=unknown
    cpu_vendor=unknown
    drm_driver=none
    wired_interface_count=0
    wifi_device_count=0

    command -v freebsd-version >/dev/null 2>&1 && release=$(freebsd-version -u 2>/dev/null || printf unavailable)
    command -v uname >/dev/null 2>&1 && architecture=$(uname -m 2>/dev/null || printf unavailable)
    command -v sysctl >/dev/null 2>&1 && boot_method=$(sysctl -n machdep.bootmethod 2>/dev/null || printf unavailable)
    if command -v mount >/dev/null 2>&1; then
        root_filesystem=$(mount -p 2>/dev/null | awk '$2 == "/" { print $3; exit }')
        [ -n "$root_filesystem" ] || root_filesystem=unavailable
    fi

    guest_hint=unknown
    if command -v sysctl >/dev/null 2>&1; then
        guest_hint=$(sysctl -n kern.vm_guest 2>/dev/null || printf unknown)
        vendor=$(sysctl -n machdep.cpu_vendor 2>/dev/null || printf unknown)
        case "$vendor" in
            GenuineIntel) cpu_vendor=intel ;;
            AuthenticAMD) cpu_vendor=amd ;;
            *) cpu_vendor=other ;;
        esac
    fi
    case "$guest_hint" in
        none|unknown|'') platform_class=physical ;;
        *) platform_class=virtual-machine ;;
    esac

    drm_card_count=$(count_devices /dev/dri 'card*')
    drm_render_count=$(count_devices /dev/dri 'renderD*')
    if command -v kldstat >/dev/null 2>&1; then
        if kldstat -q -m i915kms >/dev/null 2>&1 || kldstat -n i915kms.ko >/dev/null 2>&1; then
            drm_driver=intel
        elif kldstat -q -m amdgpu >/dev/null 2>&1 || kldstat -n amdgpu.ko >/dev/null 2>&1; then
            drm_driver=amd
        elif [ "$drm_card_count" -gt 0 ] || [ "$drm_render_count" -gt 0 ]; then
            drm_driver=other
        fi
    elif [ "$drm_card_count" -gt 0 ] || [ "$drm_render_count" -gt 0 ]; then
        drm_driver=other
    fi

    if command -v ifconfig >/dev/null 2>&1; then
        for interface in $(ifconfig -l 2>/dev/null || true); do
            case "$interface" in lo*|pflog*|pfsync*|enc*|tun*|tap*|bridge*|epair*|wlan*) continue ;; esac
            if ifconfig "$interface" 2>/dev/null | grep -q 'media: Ethernet'; then
                wired_interface_count=$((wired_interface_count + 1))
            fi
        done
    fi
    if command -v sysctl >/dev/null 2>&1; then
        wifi_devices=$(sysctl -n net.wlan.devices 2>/dev/null || true)
        set -- $wifi_devices
        wifi_device_count=$#
    fi
    audio_device_count=$(count_devices /dev 'dsp*')
    input_events=$(count_devices /dev/input 'event*')
    usb_pointer_count=$(count_devices /dev 'ums*')
    input_device_count=$((input_events + usb_pointer_count))
    qemu_guest_agent=$(service_running qemu_guest_agent)
fi

release=$(safe_token "$release" unavailable)
architecture=$(safe_token "$architecture" unavailable)
boot_method=$(safe_token "$boot_method" unavailable)
root_filesystem=$(safe_token "$root_filesystem" unavailable)
platform_class=$(safe_token "$platform_class" unknown)
cpu_vendor=$(safe_token "$cpu_vendor" unknown)
drm_driver=$(safe_token "$drm_driver" none)
drm_card_count=$(safe_count "$drm_card_count")
drm_render_count=$(safe_count "$drm_render_count")
wired_interface_count=$(safe_count "$wired_interface_count")
wifi_device_count=$(safe_count "$wifi_device_count")
audio_device_count=$(safe_count "$audio_device_count")
input_device_count=$(safe_count "$input_device_count")
case "$qemu_guest_agent" in yes|no) ;; *) qemu_guest_agent=no ;; esac

direct_drm_kms=no
if [ "$drm_card_count" -gt 0 ] && [ "$drm_render_count" -gt 0 ]; then
    direct_drm_kms=yes
fi

case "$direct_drm_kms:$drm_driver:$platform_class" in
    yes:intel:*) graphics_lane=intel-drm ;;
    yes:amd:*) graphics_lane=amd-drm ;;
    yes:*:*) graphics_lane=other-drm ;;
    no:*:virtual-machine) graphics_lane=vm-supplemental ;;
    *) graphics_lane=unavailable ;;
esac

wayland_session=absent
x11_session=absent
[ -n "${WAYLAND_DISPLAY:-}" ] && wayland_session=present
[ -n "${DISPLAY:-}" ] && x11_session=present

blockers=
add_blocker() {
    if [ -n "$blockers" ]; then blockers=$blockers,$1; else blockers=$1; fi
}
case "$release" in 15.1-RELEASE|15.1-RELEASE-p*) ;; *) add_blocker freebsd_release ;; esac
[ "$architecture" = amd64 ] || add_blocker architecture
[ "$boot_method" = UEFI ] || add_blocker uefi
[ "$root_filesystem" = zfs ] || add_blocker zfs_root
[ "$direct_drm_kms" = yes ] || add_blocker direct_drm_kms
[ "$graphics_lane" != other-drm ] || add_blocker supported_graphics
[ "$wired_interface_count" -gt 0 ] || add_blocker wired_network
[ "$audio_device_count" -gt 0 ] || add_blocker audio
[ "$input_device_count" -gt 0 ] || add_blocker input
[ -n "$blockers" ] || blockers=none

alpha_status=blocked
matrix_claim=none
base_ready=yes
case "$release" in 15.1-RELEASE|15.1-RELEASE-p*) ;; *) base_ready=no ;; esac
[ "$architecture" = amd64 ] || base_ready=no
[ "$boot_method" = UEFI ] || base_ready=no
[ "$root_filesystem" = zfs ] || base_ready=no
if [ "$blockers" = none ]; then
    case "$graphics_lane" in
        intel-drm) alpha_status=ready; matrix_claim=intel ;;
        amd-drm) alpha_status=ready; matrix_claim=amd ;;
        *) alpha_status=blocked ;;
    esac
elif [ "$platform_class" = virtual-machine ] && [ "$base_ready" = yes ]; then
    alpha_status=supplemental
    matrix_claim=vm
fi

output_parent=$(dirname "$OUTPUT")
mkdir -p "$output_parent"
tmp=$OUTPUT.tmp.$$
trap 'rm -f "$tmp"' EXIT HUP INT TERM
umask 077
{
    printf 'schema_version=1\n'
    printf 'captured_at_utc=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    printf 'release=%s\n' "$release"
    printf 'architecture=%s\n' "$architecture"
    printf 'boot_method=%s\n' "$boot_method"
    printf 'root_filesystem=%s\n' "$root_filesystem"
    printf 'platform_class=%s\n' "$platform_class"
    printf 'cpu_vendor=%s\n' "$cpu_vendor"
    printf 'drm_card_count=%s\n' "$drm_card_count"
    printf 'drm_render_count=%s\n' "$drm_render_count"
    printf 'drm_driver=%s\n' "$drm_driver"
    printf 'direct_drm_kms=%s\n' "$direct_drm_kms"
    printf 'graphics_lane=%s\n' "$graphics_lane"
    printf 'wired_interface_count=%s\n' "$wired_interface_count"
    printf 'wifi_device_count=%s\n' "$wifi_device_count"
    printf 'audio_device_count=%s\n' "$audio_device_count"
    printf 'input_device_count=%s\n' "$input_device_count"
    printf 'qemu_guest_agent=%s\n' "$qemu_guest_agent"
    printf 'wayland_session=%s\n' "$wayland_session"
    printf 'x11_session=%s\n' "$x11_session"
    printf 'matrix_claim=%s\n' "$matrix_claim"
    printf 'alpha_status=%s\n' "$alpha_status"
    printf 'blockers=%s\n' "$blockers"
} > "$tmp"
chmod 0600 "$tmp"
mv "$tmp" "$OUTPUT"
trap - EXIT HUP INT TERM

printf 'ALPHA_READINESS=%s\nGRAPHICS_LANE=%s\nMATRIX_CLAIM=%s\nBLOCKERS=%s\nOUTPUT=%s\n' \
    "$alpha_status" "$graphics_lane" "$matrix_claim" "$blockers" "$OUTPUT"

if [ "$REQUIRE_READY" -eq 1 ] && [ "$alpha_status" != ready ]; then
    exit 1
fi
