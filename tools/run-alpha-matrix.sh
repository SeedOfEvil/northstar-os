#!/bin/sh

# Assemble one privacy-bounded M6 matrix record from automatic preflight and
# explicit operator observations. This runner is read-only: it never launches,
# crashes, updates, reboots, or shuts down the machine it evaluates.

set -eu

PROG=${0##*/}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
OUTPUT=${TMPDIR:-/tmp}/northstar-alpha-matrix.conf
LANE=
OBSERVATIONS=
TEMPLATE=
REQUIRE_PASS=0
TEST_MODE=${NORTHSTAR_MATRIX_TEST_MODE:-0}

usage() {
    cat <<EOF
Usage: $PROG --lane vm|intel|amd [options]

Options:
  --output FILE          Write the bounded matrix record to FILE.
  --observations FILE    Validate fixed manual observations from FILE.
  --write-template FILE  Write a mode-0600 observation template.
  --require-pass         Exit unsuccessfully unless a physical lane passes.

The runner performs read-only preflight. It never launches applications,
forces a shell crash, mutates packages, reboots, or shuts down the machine.
EOF
}

fail() { printf 'ERROR: %s\n' "$1" >&2; exit "${2:-2}"; }

while [ "$#" -gt 0 ]; do
    case "$1" in
        --lane) [ "$#" -ge 2 ] || fail '--lane requires a value'; LANE=$2; shift 2 ;;
        --output) [ "$#" -ge 2 ] || fail '--output requires a value'; OUTPUT=$2; shift 2 ;;
        --observations) [ "$#" -ge 2 ] || fail '--observations requires a value'; OBSERVATIONS=$2; shift 2 ;;
        --write-template) [ "$#" -ge 2 ] || fail '--write-template requires a value'; TEMPLATE=$2; shift 2 ;;
        --require-pass) REQUIRE_PASS=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) fail "unknown option: $1" ;;
    esac
done

case "$LANE" in vm|intel|amd) ;; *) fail '--lane must be vm, intel, or amd' ;; esac
case "$TEST_MODE" in 0|1) ;; *) fail 'test mode must be 0 or 1' ;; esac
[ -n "$OUTPUT" ] || fail 'output path is empty'
[ ! -L "$OUTPUT" ] || fail 'output path must not be a symbolic link'

OBSERVATION_KEYS='graphical_login direct_compositor display_output native_qt xwayland firefox files settings networking audio input shell_recovery update_rollback clean_shutdown'

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

workspace=$(mktemp -d "${TMPDIR:-/tmp}/northstar-alpha-matrix.XXXXXX")
cleanup() { rm -rf "$workspace"; }
trap cleanup EXIT HUP INT TERM
readiness=$workspace/readiness.conf
sh "$SCRIPT_DIR/collect-alpha-readiness.sh" --output "$readiness" >/dev/null

field() {
    key=$1
    file=$2
    awk -F= -v wanted="$key" '$1 == wanted { count++; value=substr($0, index($0, "=") + 1) } END { if (count == 1) print value }' "$file"
}

hardware_status=$(field alpha_status "$readiness")
hardware_claim=$(field matrix_claim "$readiness")
hardware_graphics_lane=$(field graphics_lane "$readiness")
[ -n "$hardware_status" ] && [ -n "$hardware_claim" ] && [ -n "$hardware_graphics_lane" ] || fail 'readiness record is incomplete'

preflight_blockers=
add_blocker() {
    if [ -n "$preflight_blockers" ]; then
        preflight_blockers=$preflight_blockers,$1
    else
        preflight_blockers=$1
    fi
}

case "$LANE:$hardware_status:$hardware_claim" in
    vm:supplemental:vm) ;;
    intel:ready:intel) ;;
    amd:ready:amd) ;;
    *) add_blocker lane_mismatch ;;
esac

available_executable() {
    name=$1
    command -v "$name" >/dev/null 2>&1 && return 0
    [ -x "/usr/local/bin/$name" ] && return 0
    [ -x "${HOME:-/nonexistent}/.local/bin/$name" ] && return 0
    return 1
}

available_file() {
    relative=$1
    [ -f "/usr/local/$relative" ] && return 0
    [ -f "${HOME:-/nonexistent}/.local/$relative" ] && return 0
    return 1
}

if [ "$TEST_MODE" = 1 ]; then
    shell_available=${NORTHSTAR_MATRIX_TEST_SHELL:-yes}
    session_available=${NORTHSTAR_MATRIX_TEST_SESSION:-yes}
    desktop_entry_available=${NORTHSTAR_MATRIX_TEST_DESKTOP_ENTRY:-yes}
    firefox_available=${NORTHSTAR_MATRIX_TEST_FIREFOX:-yes}
    terminal_available=${NORTHSTAR_MATRIX_TEST_TERMINAL:-yes}
    diagnostics_available=${NORTHSTAR_MATRIX_TEST_DIAGNOSTICS:-yes}
    display_manager_available=${NORTHSTAR_MATRIX_TEST_DISPLAY_MANAGER:-yes}
else
    shell_available=no; available_executable northstar-shell && shell_available=yes
    session_available=no; available_executable northstar-session && session_available=yes
    desktop_entry_available=no; available_file share/wayland-sessions/northstar.desktop && desktop_entry_available=yes
    firefox_available=no; available_executable firefox && firefox_available=yes
    terminal_available=no
    if available_executable qterminal || available_executable xterm; then terminal_available=yes; fi
    diagnostics_available=no
    if [ -x "$SCRIPT_DIR/collect-diagnostics.sh" ] || [ -f "$SCRIPT_DIR/collect-diagnostics.sh" ]; then diagnostics_available=yes; fi
    display_manager_available=no
    if command -v service >/dev/null 2>&1 && service sddm status >/dev/null 2>&1; then display_manager_available=yes; fi
fi

normalize_presence() {
    case "$1" in yes|no) printf '%s\n' "$1" ;; *) printf 'no\n' ;; esac
}
shell_available=$(normalize_presence "$shell_available")
session_available=$(normalize_presence "$session_available")
desktop_entry_available=$(normalize_presence "$desktop_entry_available")
firefox_available=$(normalize_presence "$firefox_available")
terminal_available=$(normalize_presence "$terminal_available")
diagnostics_available=$(normalize_presence "$diagnostics_available")
display_manager_available=$(normalize_presence "$display_manager_available")

for item in shell session desktop_entry firefox terminal diagnostics display_manager; do
    eval value=\$${item}_available
    case "$value" in yes) ;; *) add_blocker "$item" ;; esac
done

wayland_session=absent
x11_session=absent
[ -n "${WAYLAND_DISPLAY:-}" ] && wayland_session=present
[ -n "${DISPLAY:-}" ] && x11_session=present

observations_state=absent
manual_pass_count=0
manual_fail_count=0
manual_pending_count=0
manual_deferred_count=0

if [ -n "$OBSERVATIONS" ]; then
    [ -f "$OBSERVATIONS" ] || fail 'observations file is unavailable'
    [ ! -L "$OBSERVATIONS" ] || fail 'observations file must not be a symbolic link'
    schema=$(field schema_version "$OBSERVATIONS")
    [ "$schema" = 1 ] || fail 'observations schema must be exactly 1'
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

[ -n "$preflight_blockers" ] || preflight_blockers=none
preflight_status=pass
[ "$preflight_blockers" = none ] || preflight_status=blocked

matrix_status=inventory-only
if [ "$preflight_status" = blocked ]; then
    matrix_status=blocked
elif [ "$observations_state" = validated ]; then
    if [ "$manual_fail_count" -gt 0 ]; then
        matrix_status=fail
    elif [ "$manual_pending_count" -gt 0 ]; then
        matrix_status=pending
    elif [ "$LANE" = vm ]; then
        matrix_status=supplemental
    elif [ "$manual_deferred_count" -gt 0 ]; then
        matrix_status=partial
    else
        matrix_status=pass
    fi
fi

mkdir -p "$(dirname "$OUTPUT")"
temporary=$OUTPUT.tmp.$$
trap 'rm -f "$temporary"; cleanup' EXIT HUP INT TERM
umask 077
{
    printf 'schema_version=1\n'
    printf 'captured_at_utc=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    printf 'expected_lane=%s\n' "$LANE"
    printf 'hardware_status=%s\n' "$hardware_status"
    printf 'hardware_claim=%s\n' "$hardware_claim"
    printf 'hardware_graphics_lane=%s\n' "$hardware_graphics_lane"
    printf 'shell_available=%s\n' "$shell_available"
    printf 'session_available=%s\n' "$session_available"
    printf 'desktop_entry_available=%s\n' "$desktop_entry_available"
    printf 'firefox_available=%s\n' "$firefox_available"
    printf 'terminal_available=%s\n' "$terminal_available"
    printf 'diagnostics_available=%s\n' "$diagnostics_available"
    printf 'display_manager_available=%s\n' "$display_manager_available"
    printf 'wayland_session=%s\n' "$wayland_session"
    printf 'x11_session=%s\n' "$x11_session"
    printf 'preflight_status=%s\n' "$preflight_status"
    printf 'preflight_blockers=%s\n' "$preflight_blockers"
    printf 'observations=%s\n' "$observations_state"
    printf 'manual_pass_count=%s\n' "$manual_pass_count"
    printf 'manual_fail_count=%s\n' "$manual_fail_count"
    printf 'manual_pending_count=%s\n' "$manual_pending_count"
    printf 'manual_deferred_count=%s\n' "$manual_deferred_count"
    printf 'matrix_status=%s\n' "$matrix_status"
} > "$temporary"
chmod 0600 "$temporary"
mv "$temporary" "$OUTPUT"
trap cleanup EXIT HUP INT TERM

printf 'MATRIX_STATUS=%s\nEXPECTED_LANE=%s\nPREFLIGHT=%s\nBLOCKERS=%s\nOUTPUT=%s\n' \
    "$matrix_status" "$LANE" "$preflight_status" "$preflight_blockers" "$OUTPUT"

if [ "$REQUIRE_PASS" -eq 1 ] && [ "$matrix_status" != pass ]; then exit 1; fi
