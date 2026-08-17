#!/bin/sh

# Collect passive M6 networking, audio, input, and power capabilities and join
# them with explicit operator observations. The collector never transmits
# traffic, plays audio, suspends the host, or changes device state.

set -eu

PROG=${0##*/}
OUTPUT=${TMPDIR:-/tmp}/northstar-platform-evidence.conf
OBSERVATIONS=
TEMPLATE=
REQUIRE_PASS=0
TEST_MODE=${NORTHSTAR_PLATFORM_TEST_MODE:-0}

OBSERVATION_KEYS='network_connectivity audio_playback volume_control keyboard_input pointer_input suspend_resume'

usage() {
    cat <<EOF
Usage: $PROG [options]

Options:
  --output FILE          Write the bounded platform record to FILE.
  --observations FILE    Validate fixed operator observations from FILE.
  --write-template FILE  Write a mode-0600 observation template.
  --require-pass         Exit unsuccessfully unless a physical lane passes.

Collection is passive. It does not send network traffic, play audio, alter
volume, inject input, suspend, resume, reboot, or shut down the machine.
EOF
}

fail() { printf 'ERROR: %s\n' "$1" >&2; exit "${2:-2}"; }

while [ "$#" -gt 0 ]; do
    case "$1" in
        --output) [ "$#" -ge 2 ] || fail '--output requires a value'; OUTPUT=$2; shift 2 ;;
        --observations) [ "$#" -ge 2 ] || fail '--observations requires a value'; OBSERVATIONS=$2; shift 2 ;;
        --write-template) [ "$#" -ge 2 ] || fail '--write-template requires a value'; TEMPLATE=$2; shift 2 ;;
        --require-pass) REQUIRE_PASS=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) fail "unknown option: $1" ;;
    esac
done

[ -n "$OUTPUT" ] || fail 'output path is empty'
[ ! -L "$OUTPUT" ] || fail 'output path must not be a symbolic link'
case "$TEST_MODE" in 0|1) ;; *) fail 'test mode must be 0 or 1' ;; esac

field() {
    key=$1
    file=$2
    awk -F= -v wanted="$key" '$1 == wanted { count++; value=substr($0, index($0, "=") + 1) } END { if (count == 1) print value }' "$file"
}

safe_count() {
    value=$1
    case "$value" in
        ''|*[!0-9]*) printf '0\n' ;;
        *) [ "$value" -le 999 ] 2>/dev/null && printf '%s\n' "$value" || printf '999\n' ;;
    esac
}

yes_no() { case "$1" in yes|no) printf '%s\n' "$1" ;; *) printf 'no\n' ;; esac; }

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

write_template() {
    target=$1
    [ -n "$target" ] || fail 'template path is empty'
    [ ! -L "$target" ] || fail 'template path must not be a symbolic link'
    mkdir -p "$(dirname "$target")"
    temporary=$target.tmp.$$
    trap 'rm -f "$temporary"' EXIT HUP INT TERM
    umask 077
    {
        printf 'schema_version=1\n'
        for key in $OBSERVATION_KEYS; do printf '%s=pending\n' "$key"; done
    } > "$temporary"
    chmod 0600 "$temporary"
    mv "$temporary" "$target"
    trap - EXIT HUP INT TERM
}

[ -z "$TEMPLATE" ] || write_template "$TEMPLATE"

if [ "$TEST_MODE" = 1 ]; then
    platform_class=${NORTHSTAR_PLATFORM_TEST_CLASS:-virtual-machine}
    wired_device_count=${NORTHSTAR_PLATFORM_TEST_WIRED_DEVICES:-1}
    wired_active_count=${NORTHSTAR_PLATFORM_TEST_WIRED_ACTIVE:-1}
    wifi_device_count=${NORTHSTAR_PLATFORM_TEST_WIFI_DEVICES:-0}
    wifi_active_count=${NORTHSTAR_PLATFORM_TEST_WIFI_ACTIVE:-0}
    default_route=${NORTHSTAR_PLATFORM_TEST_DEFAULT_ROUTE:-yes}
    dns_configured=${NORTHSTAR_PLATFORM_TEST_DNS:-yes}
    audio_device_count=${NORTHSTAR_PLATFORM_TEST_AUDIO_DEVICES:-1}
    mixer_available=${NORTHSTAR_PLATFORM_TEST_MIXER_AVAILABLE:-yes}
    mixer_readable=${NORTHSTAR_PLATFORM_TEST_MIXER_READABLE:-yes}
    input_device_count=${NORTHSTAR_PLATFORM_TEST_INPUT_DEVICES:-2}
    keyboard_available=${NORTHSTAR_PLATFORM_TEST_KEYBOARD:-yes}
    pointer_available=${NORTHSTAR_PLATFORM_TEST_POINTER:-yes}
    acpi_available=${NORTHSTAR_PLATFORM_TEST_ACPI:-yes}
    suspend_command_available=${NORTHSTAR_PLATFORM_TEST_SUSPEND_COMMAND:-yes}
else
    platform_class=physical
    if command -v sysctl >/dev/null 2>&1; then
        guest_hint=$(sysctl -n kern.vm_guest 2>/dev/null || printf unknown)
        case "$guest_hint" in none|unknown|'') ;; *) platform_class=virtual-machine ;; esac
    fi

    wired_device_count=0
    wired_active_count=0
    wifi_device_count=0
    wifi_active_count=0
    if command -v ifconfig >/dev/null 2>&1; then
        for interface in $(ifconfig -l 2>/dev/null || true); do
            case "$interface" in
                lo*|pflog*|pfsync*|enc*|tun*|tap*|bridge*|epair*) continue ;;
                wlan*)
                    wifi_device_count=$((wifi_device_count + 1))
                    if ifconfig "$interface" 2>/dev/null | grep -q 'status: active'; then
                        wifi_active_count=$((wifi_active_count + 1))
                    fi
                    ;;
                *)
                    if ifconfig "$interface" 2>/dev/null | grep -q 'media: Ethernet'; then
                        wired_device_count=$((wired_device_count + 1))
                        if ifconfig "$interface" 2>/dev/null | grep -q 'status: active'; then
                            wired_active_count=$((wired_active_count + 1))
                        fi
                    fi
                    ;;
            esac
        done
    fi
    if command -v sysctl >/dev/null 2>&1; then
        wifi_devices=$(sysctl -n net.wlan.devices 2>/dev/null || true)
        set -- $wifi_devices
        [ "$#" -le "$wifi_device_count" ] || wifi_device_count=$#
    fi

    default_route=no
    if command -v route >/dev/null 2>&1 && route -n get default >/dev/null 2>&1; then default_route=yes; fi
    dns_configured=no
    if [ -r /etc/resolv.conf ] && awk '$1 == "nameserver" && $2 !~ /^#/ { found=1 } END { exit !found }' /etc/resolv.conf; then
        dns_configured=yes
    fi

    audio_device_count=$(count_devices /dev 'dsp*')
    mixer_available=no
    mixer_readable=no
    if command -v mixer >/dev/null 2>&1; then
        mixer_available=yes
        if mixer -s >/dev/null 2>&1 || mixer >/dev/null 2>&1; then mixer_readable=yes; fi
    fi

    input_events=$(count_devices /dev/input 'event*')
    pointer_devices=$(count_devices /dev 'ums*')
    keyboard_devices=$(count_devices /dev 'kbd*')
    input_device_count=$((input_events + pointer_devices + keyboard_devices))
    keyboard_available=no
    pointer_available=no
    if [ "$input_events" -gt 0 ]; then keyboard_available=yes; pointer_available=yes; fi
    [ "$keyboard_devices" -eq 0 ] || keyboard_available=yes
    [ "$pointer_devices" -eq 0 ] || pointer_available=yes

    acpi_available=no
    [ -c /dev/acpi ] && acpi_available=yes
    suspend_command_available=no
    if command -v acpiconf >/dev/null 2>&1; then suspend_command_available=yes; fi
fi

case "$platform_class" in physical|virtual-machine) ;; *) platform_class=unknown ;; esac
wired_device_count=$(safe_count "$wired_device_count")
wired_active_count=$(safe_count "$wired_active_count")
wifi_device_count=$(safe_count "$wifi_device_count")
wifi_active_count=$(safe_count "$wifi_active_count")
audio_device_count=$(safe_count "$audio_device_count")
input_device_count=$(safe_count "$input_device_count")
default_route=$(yes_no "$default_route")
dns_configured=$(yes_no "$dns_configured")
mixer_available=$(yes_no "$mixer_available")
mixer_readable=$(yes_no "$mixer_readable")
keyboard_available=$(yes_no "$keyboard_available")
pointer_available=$(yes_no "$pointer_available")
acpi_available=$(yes_no "$acpi_available")
suspend_command_available=$(yes_no "$suspend_command_available")

capability_blockers=
add_blocker() {
    if [ -n "$capability_blockers" ]; then capability_blockers=$capability_blockers,$1; else capability_blockers=$1; fi
}
if [ "$wired_active_count" -eq 0 ] && [ "$wifi_active_count" -eq 0 ]; then add_blocker active_network; fi
[ "$default_route" = yes ] || add_blocker default_route
[ "$dns_configured" = yes ] || add_blocker dns
[ "$audio_device_count" -gt 0 ] || add_blocker audio_device
[ "$mixer_available" = yes ] || add_blocker mixer
[ "$mixer_readable" = yes ] || add_blocker mixer_access
[ "$input_device_count" -gt 0 ] || add_blocker input_device
[ "$keyboard_available" = yes ] || add_blocker keyboard
[ "$pointer_available" = yes ] || add_blocker pointer
if [ "$platform_class" = physical ]; then
    [ "$acpi_available" = yes ] || add_blocker acpi
    [ "$suspend_command_available" = yes ] || add_blocker suspend_command
fi
[ -n "$capability_blockers" ] || capability_blockers=none

capability_status=ready
[ "$capability_blockers" = none ] || capability_status=blocked
if [ "$platform_class" = virtual-machine ]; then capability_status=supplemental; fi

observations_state=absent
manual_pass_count=0
manual_fail_count=0
manual_pending_count=0
manual_deferred_count=0
if [ -n "$OBSERVATIONS" ]; then
    [ -f "$OBSERVATIONS" ] || fail 'observations file is unavailable'
    [ ! -L "$OBSERVATIONS" ] || fail 'observations file must not be a symbolic link'
    [ "$(field schema_version "$OBSERVATIONS")" = 1 ] || fail 'observations schema must be exactly 1'
    expected_lines=1
    for key in $OBSERVATION_KEYS; do
        expected_lines=$((expected_lines + 1))
        value=$(field "$key" "$OBSERVATIONS")
        case "$value" in
            pass) manual_pass_count=$((manual_pass_count + 1)) ;;
            fail) manual_fail_count=$((manual_fail_count + 1)) ;;
            pending) manual_pending_count=$((manual_pending_count + 1)) ;;
            deferred) manual_deferred_count=$((manual_deferred_count + 1)) ;;
            '') fail "observation is missing or duplicated: $key" ;;
            *) fail "invalid observation value for $key" ;;
        esac
    done
    actual_lines=$(awk 'NF { count++ } END { print count + 0 }' "$OBSERVATIONS")
    [ "$actual_lines" -eq "$expected_lines" ] || fail 'observations contain unknown or blank records'
    observations_state=validated
fi

platform_status=inventory-only
if [ "$observations_state" = validated ]; then
    if [ "$manual_fail_count" -gt 0 ]; then platform_status=fail
    elif [ "$manual_pending_count" -gt 0 ]; then platform_status=pending
    elif [ "$platform_class" = virtual-machine ]; then platform_status=supplemental
    elif [ "$capability_status" = blocked ]; then platform_status=blocked
    elif [ "$manual_deferred_count" -gt 0 ]; then platform_status=partial
    else platform_status=pass
    fi
fi

mkdir -p "$(dirname "$OUTPUT")"
temporary=$OUTPUT.tmp.$$
trap 'rm -f "$temporary"' EXIT HUP INT TERM
umask 077
{
    printf 'schema_version=1\n'
    printf 'captured_at_utc=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    printf 'platform_class=%s\n' "$platform_class"
    printf 'wired_device_count=%s\n' "$wired_device_count"
    printf 'wired_active_count=%s\n' "$wired_active_count"
    printf 'wifi_device_count=%s\n' "$wifi_device_count"
    printf 'wifi_active_count=%s\n' "$wifi_active_count"
    printf 'default_route=%s\n' "$default_route"
    printf 'dns_configured=%s\n' "$dns_configured"
    printf 'audio_device_count=%s\n' "$audio_device_count"
    printf 'mixer_available=%s\n' "$mixer_available"
    printf 'mixer_readable=%s\n' "$mixer_readable"
    printf 'input_device_count=%s\n' "$input_device_count"
    printf 'keyboard_available=%s\n' "$keyboard_available"
    printf 'pointer_available=%s\n' "$pointer_available"
    printf 'acpi_available=%s\n' "$acpi_available"
    printf 'suspend_command_available=%s\n' "$suspend_command_available"
    printf 'capability_status=%s\n' "$capability_status"
    printf 'capability_blockers=%s\n' "$capability_blockers"
    printf 'observations=%s\n' "$observations_state"
    printf 'manual_pass_count=%s\n' "$manual_pass_count"
    printf 'manual_fail_count=%s\n' "$manual_fail_count"
    printf 'manual_pending_count=%s\n' "$manual_pending_count"
    printf 'manual_deferred_count=%s\n' "$manual_deferred_count"
    printf 'platform_status=%s\n' "$platform_status"
} > "$temporary"
chmod 0600 "$temporary"
mv "$temporary" "$OUTPUT"
trap - EXIT HUP INT TERM

printf 'PLATFORM_STATUS=%s\nCAPABILITY_STATUS=%s\nBLOCKERS=%s\nOUTPUT=%s\n' \
    "$platform_status" "$capability_status" "$capability_blockers" "$OUTPUT"

if [ "$REQUIRE_PASS" -eq 1 ] && [ "$platform_status" != pass ]; then exit 1; fi
