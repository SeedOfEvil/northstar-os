#!/bin/sh

# Idempotent M0 development-environment bootstrap for FreeBSD.
#
# This script deliberately changes only package state, two rc.conf variables,
# the selected user's video-group membership, and the two requested services.

set -eu

PROG=${0##*/}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
DEFAULT_MANIFEST=$REPO_ROOT/packaging/manifests/bootstrap-packages.txt
DEFAULT_CAPTURE=/var/log/northstar/m0-bootstrap.txt

DEV_USER=
MANIFEST=$DEFAULT_MANIFEST
CAPTURE=$DEFAULT_CAPTURE
DRY_RUN=0
TMP_DIR=

usage() {
    cat <<USAGE
Usage: $PROG --user USER [options]

Install the Northstar M0 development packages and enable the required
Wayland session services. The command must run as root.

Options:
  --user USER       Existing non-root development account (required)
  --manifest FILE   Package manifest (default: $DEFAULT_MANIFEST)
  --capture FILE    Package/host capture (default: $DEFAULT_CAPTURE)
  --dry-run         Validate and print mutating operations without applying them
  --help            Show this help
USAGE
}

die() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

usage_error() {
    printf 'ERROR: %s\n' "$1" >&2
    usage >&2
    exit 2
}

cleanup() {
    if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
        case "$TMP_DIR" in
            "${TMPDIR:-/tmp}"/northstar-bootstrap.*) rm -rf "$TMP_DIR" ;;
        esac
    fi
}

trap cleanup EXIT HUP INT TERM

while [ "$#" -gt 0 ]; do
    case "$1" in
        --user)
            [ "$#" -ge 2 ] || usage_error '--user requires a value'
            DEV_USER=$2
            shift 2
            ;;
        --manifest)
            [ "$#" -ge 2 ] || usage_error '--manifest requires a value'
            MANIFEST=$2
            shift 2
            ;;
        --capture)
            [ "$#" -ge 2 ] || usage_error '--capture requires a value'
            CAPTURE=$2
            shift 2
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            usage_error "unknown option: $1"
            ;;
    esac
done

[ -n "$DEV_USER" ] || usage_error '--user is required'
[ "$DEV_USER" != root ] || die 'the development account must not be root'

if [ "$(id -u)" -ne 0 ]; then
    die 'bootstrap-dev.sh must run as root; use sudo or doas on the FreeBSD host'
fi

for command_name in \
    id \
    sh \
    pkg \
    sysrc \
    service \
    pw \
    freebsd-version \
    uname \
    sysctl \
    mount \
    awk \
    sed \
    sort \
    uniq \
    mktemp \
    mkdir \
    mv \
    rm \
    chmod \
    dirname \
    date \
    xargs
do
    command -v "$command_name" >/dev/null 2>&1 || die "required command is unavailable: $command_name"
done

if ! id "$DEV_USER" >/dev/null 2>&1; then
    die "development account does not exist: $DEV_USER"
fi

if ! sh "$SCRIPT_DIR/check-host.sh"; then
    die 'host validation failed; no package or service changes were made'
fi

[ -r "$MANIFEST" ] || die "package manifest is not readable: $MANIFEST"

TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-bootstrap.XXXXXX")
VALIDATED_MANIFEST=$TMP_DIR/packages.txt
TO_INSTALL=$TMP_DIR/to-install.txt

while IFS= read -r line || [ -n "$line" ]; do
    line=$(printf '%s\n' "$line" | sed 's/[[:space:]]*$//')
    case "$line" in
        ''|\#*) continue ;;
        *[!A-Za-z0-9+_.-]*) die "invalid package name in manifest: $line" ;;
    esac
    printf '%s\n' "$line" >> "$VALIDATED_MANIFEST"
done < "$MANIFEST"

[ -s "$VALIDATED_MANIFEST" ] || die "package manifest contains no packages: $MANIFEST"

duplicates=$(sort "$VALIDATED_MANIFEST" | uniq -d)
[ -z "$duplicates" ] || die "duplicate package names in manifest: $duplicates"

run_mutation() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf 'DRY-RUN:'
        printf ' %s' "$@"
        printf '\n'
    else
        "$@"
    fi
}

ensure_rc_setting() {
    setting=$1
    if [ "$DRY_RUN" -eq 1 ]; then
        printf 'DRY-RUN: ensure sysrc %s\n' "$setting"
    elif sysrc -c "$setting" >/dev/null 2>&1; then
        printf 'PASS: sysrc already has %s\n' "$setting"
    else
        sysrc "$setting"
    fi
}

ensure_service_running() {
    service_name=$1
    if [ "$DRY_RUN" -eq 1 ]; then
        printf 'DRY-RUN: ensure service %s is running\n' "$service_name"
    elif service "$service_name" status >/dev/null 2>&1; then
        printf 'PASS: service already running: %s\n' "$service_name"
    else
        service "$service_name" start
    fi
}

printf 'Northstar M0 bootstrap for user %s\n' "$DEV_USER"
printf 'Manifest: %s\n' "$MANIFEST"

if [ "$DRY_RUN" -eq 1 ]; then
    printf 'DRY-RUN: pkg update\n'
else
    pkg update
fi

missing=0
if [ "$DRY_RUN" -eq 1 ]; then
    while IFS= read -r package_name; do
        printf 'DRY-RUN: verify package catalogue entry: %s\n' "$package_name"
    done < "$VALIDATED_MANIFEST"
else
    while IFS= read -r package_name; do
        if pkg search -U -e "$package_name" >/dev/null 2>&1; then
            printf 'PASS: package is available: %s\n' "$package_name"
        else
            printf 'FAIL: package is unavailable: %s\n' "$package_name" >&2
            missing=1
        fi
    done < "$VALIDATED_MANIFEST"
fi

[ "$missing" -eq 0 ] || die 'package preflight failed; no packages or services were changed'

: > "$TO_INSTALL"
if [ "$DRY_RUN" -eq 1 ]; then
    printf 'DRY-RUN: pkg install -y'
    while IFS= read -r package_name; do
        printf ' %s' "$package_name"
    done < "$VALIDATED_MANIFEST"
    printf '\n'
else
    while IFS= read -r package_name; do
        if pkg info -e "$package_name" >/dev/null 2>&1; then
            printf 'PASS: package already installed: %s\n' "$package_name"
        else
            printf '%s\n' "$package_name" >> "$TO_INSTALL"
        fi
    done < "$VALIDATED_MANIFEST"

    if [ -s "$TO_INSTALL" ]; then
        xargs pkg install -y < "$TO_INSTALL"
    else
        printf 'PASS: all manifest packages are already installed\n'
    fi
fi

ensure_rc_setting 'dbus_enable=YES'
ensure_rc_setting 'seatd_enable=YES'

if ! pw groupshow video >/dev/null 2>&1; then
    die 'the video group does not exist; create or repair the FreeBSD graphics baseline before bootstrapping'
fi

user_groups=$(id -Gn "$DEV_USER")
case " $user_groups " in
    *' video '*)
        printf 'PASS: %s is already a member of video\n' "$DEV_USER"
        ;;
    *)
        run_mutation pw groupmod video -m "$DEV_USER"
        ;;
esac

ensure_service_running dbus
ensure_service_running seatd

if [ "$DRY_RUN" -eq 1 ]; then
    printf 'DRY-RUN: would write capture to %s\n' "$CAPTURE"
    printf 'M0 bootstrap dry-run complete.\n'
    exit 0
fi

installed_packages=$TMP_DIR/installed-packages.txt
raw_installed_packages=$TMP_DIR/raw-installed-packages.txt
if ! pkg info -a -q > "$raw_installed_packages"; then
    die 'could not capture installed package versions'
fi
sort "$raw_installed_packages" > "$installed_packages"

capture_dir=$(dirname "$CAPTURE")
mkdir -p "$capture_dir"
capture_tmp=$(mktemp "${CAPTURE}.XXXXXX")

project_commit=unknown
if command -v git >/dev/null 2>&1; then
    project_commit=$(git -C "$REPO_ROOT" rev-parse --verify HEAD 2>/dev/null || printf 'unknown')
fi

boot_method=$(sysctl -n machdep.bootmethod 2>/dev/null || printf 'unknown')
root_source=$(mount -p 2>/dev/null | awk '$2 == "/" { print $1; exit }')
dbus_enabled=$(sysrc -n dbus_enable 2>/dev/null || printf 'unknown')
seatd_enabled=$(sysrc -n seatd_enable 2>/dev/null || printf 'unknown')

{
    printf '# Northstar M0 bootstrap capture\n'
    printf 'captured_at_utc=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    printf 'project_commit=%s\n' "$project_commit"
    printf 'freebsd_userland=%s\n' "$(freebsd-version -u 2>/dev/null || printf 'unknown')"
    printf 'freebsd_kernel=%s\n' "$(freebsd-version -k 2>/dev/null || printf 'unknown')"
    printf 'architecture=%s\n' "$(uname -m 2>/dev/null || printf 'unknown')"
    printf 'boot_method=%s\n' "$boot_method"
    printf 'root_source=%s\n' "$root_source"
    printf 'dbus_enable=%s\n' "$dbus_enabled"
    printf 'seatd_enable=%s\n' "$seatd_enabled"
    printf '\n[requested_packages]\n'
    cat "$VALIDATED_MANIFEST"
    printf '\n[installed_packages]\n'
    cat "$installed_packages"
} > "$capture_tmp"

chmod 0644 "$capture_tmp"
mv -f "$capture_tmp" "$CAPTURE"

printf 'Capture written to %s\n' "$CAPTURE"
printf 'M0 bootstrap complete. Log out and back in before starting a Wayland session.\n'
