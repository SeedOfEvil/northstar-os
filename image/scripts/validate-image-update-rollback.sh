#!/bin/sh

# Reboot-spanning acceptance gate for an installed Northstar image.
# State intentionally lives on /home, outside the root boot environment.

set -eu

EX_USAGE=64
EX_DATAERR=65
EX_UNAVAILABLE=69
EX_NOPERM=77

STATE_DIR=${NORTHSTAR_IMAGE_VALIDATION_STATE_DIR:-/home/.northstar-image-validation}
IMAGE_MARKER=${NORTHSTAR_IMAGE_MARKER:-/var/db/northstar/image-build.conf}
TRANSACTION=${NORTHSTAR_IMAGE_UPDATE_TRANSACTION:-/usr/local/libexec/northstar-update-transaction}
PKG=${NORTHSTAR_IMAGE_PKG:-/usr/sbin/pkg}
BECTL=${NORTHSTAR_IMAGE_BECTL:-/sbin/bectl}
REPOSITORY=${NORTHSTAR_IMAGE_REPOSITORY:-northstar-development}
PACKAGE=${NORTHSTAR_IMAGE_PACKAGE_NAME:-northstar}
TEST_MODE=${NORTHSTAR_IMAGE_VALIDATION_TEST_MODE:-0}
STATE=$STATE_DIR/state.conf
SENTINEL=$STATE_DIR/home-sentinel.txt
FAILURE_PKG=$STATE_DIR/pkg-failure-wrapper

fail() {
    status=$1
    shift
    printf 'ERROR: %s\n' "$*" >&2
    exit "$status"
}

usage() {
    cat <<'EOF'
Usage:
  validate-image-update-rollback.sh --prepare --baseline-version V --candidate-version V
  validate-image-update-rollback.sh --inject-failure
  validate-image-update-rollback.sh --verify-failure-recovery
  validate-image-update-rollback.sh --normalize-after-failure
  validate-image-update-rollback.sh --apply-update
  validate-image-update-rollback.sh --schedule-rollback
  validate-image-update-rollback.sh --verify-rollback

Run the phases in order on a disposable installed Northstar QCOW2. Reboot after
--inject-failure and --schedule-rollback. The gate mutates ZFS boot environments
and packages; it refuses to run without the installed-image marker.
EOF
}

require_production_boundary() {
    [ "$TEST_MODE" = 1 ] && return
    [ "$(uname -s)" = FreeBSD ] || fail "$EX_UNAVAILABLE" 'this destructive gate runs only on FreeBSD'
    [ "$(id -u)" -eq 0 ] || fail "$EX_NOPERM" 'this gate requires root'
    [ -f "$IMAGE_MARKER" ] && [ ! -L "$IMAGE_MARKER" ] ||
        fail "$EX_NOPERM" 'installed Northstar image marker is missing'
    case "$STATE_DIR" in /home/*) ;; *) fail "$EX_NOPERM" 'validation state must remain beneath /home' ;; esac
    root_dataset=$(df -T / 2>/dev/null | awk 'NR == 2 { print $1 }')
    home_dataset=$(df -T /home 2>/dev/null | awk 'NR == 2 { print $1 }')
    [ -n "$root_dataset" ] && [ -n "$home_dataset" ] && [ "$root_dataset" != "$home_dataset" ] ||
        fail "$EX_NOPERM" '/home must be a separate dataset so rollback preserves evidence'
}

require_state_boundary() {
    [ -d "$STATE_DIR" ] && [ ! -L "$STATE_DIR" ] || fail "$EX_NOPERM" 'validation state directory is unsafe'
    [ -f "$STATE" ] && [ ! -L "$STATE" ] || fail "$EX_NOPERM" 'validation state file is unsafe'
    [ -f "$SENTINEL" ] && [ ! -L "$SENTINEL" ] || fail "$EX_NOPERM" 'home sentinel is unsafe'
    [ "$TEST_MODE" = 1 ] && return
    owner=$(stat -f '%u' "$STATE_DIR")
    mode=$(stat -f '%Lp' "$STATE_DIR")
    [ "$owner" = 0 ] && [ "$mode" = 700 ] || fail "$EX_NOPERM" 'validation state directory must be root-owned mode 0700'
    owner=$(stat -f '%u' "$STATE")
    mode=$(stat -f '%Lp' "$STATE")
    [ "$owner" = 0 ] && [ "$mode" = 600 ] || fail "$EX_NOPERM" 'validation state must be root-owned mode 0600'
}

require_tools() {
    for executable in "$TRANSACTION" "$PKG" "$BECTL"; do
        [ -x "$executable" ] || fail "$EX_UNAVAILABLE" "required executable unavailable: $executable"
    done
}

state_value() {
    key=$1
    [ -f "$STATE" ] || fail "$EX_DATAERR" 'validation state is missing; run --prepare first'
    value=$(awk -F= -v key="$key" '$1 == key { print substr($0, length(key) + 2) }' "$STATE")
    [ -n "$value" ] || fail "$EX_DATAERR" "validation state omits $key"
    printf '%s\n' "$value"
}

write_state() {
    stage=$1
    baseline=$2
    candidate=$3
    baseline_be=$4
    rollback_be=$5
    sentinel_sha=$6
    mkdir -p "$STATE_DIR"
    tmp=$STATE.tmp.$$
    {
        printf 'schema_version=1\n'
        printf 'stage=%s\n' "$stage"
        printf 'baseline_version=%s\n' "$baseline"
        printf 'candidate_version=%s\n' "$candidate"
        printf 'baseline_boot_environment=%s\n' "$baseline_be"
        printf 'rollback_boot_environment=%s\n' "$rollback_be"
        printf 'sentinel_sha256=%s\n' "$sentinel_sha"
    } > "$tmp"
    chmod 0600 "$tmp"
    mv "$tmp" "$STATE"
}

digest() {
    if command -v sha256 >/dev/null 2>&1; then
        sha256 -q "$1"
    else
        sha256sum "$1" | awk '{ print $1 }'
    fi
}

active_be() {
    "$BECTL" list -H | awk '$2 ~ /N/ { count++; name=$1 } END { if (count == 1) print name; else exit 2 }'
}

installed_version() {
    "$PKG" query -e "%n == $PACKAGE" '%v' 2>/dev/null || true
}

verify_sentinel() {
    expected=$(state_value sentinel_sha256)
    [ -f "$SENTINEL" ] && [ "$(digest "$SENTINEL")" = "$expected" ] ||
        fail "$EX_DATAERR" 'home sentinel was not preserved'
}

read_transaction_be() {
    transaction_state=${NORTHSTAR_UPDATE_STATE_DIR:-/var/db/northstar}/update-state.conf
    [ -f "$transaction_state" ] || fail "$EX_DATAERR" 'transaction state was not created'
    value=$(awk -F= '$1 == "boot_environment" { print $2 }' "$transaction_state")
    [ -n "$value" ] || fail "$EX_DATAERR" 'transaction state omits its boot environment'
    printf '%s\n' "$value"
}

prepare() {
    baseline=$1
    candidate=$2
    [ ! -e "$STATE_DIR" ] || fail "$EX_DATAERR" "validation state already exists: $STATE_DIR"
    [ "$(installed_version)" = "$baseline" ] || fail "$EX_DATAERR" "installed $PACKAGE is not baseline $baseline"
    available=$("$PKG" rquery -r "$REPOSITORY" -e "%n == $PACKAGE" '%v' 2>/dev/null || true)
    [ "$available" = "$candidate" ] || fail "$EX_DATAERR" "repository candidate is not $candidate"
    baseline_be=$(active_be) || fail "$EX_DATAERR" 'could not identify exactly one active boot environment'
    mkdir -p "$STATE_DIR"
    chmod 0700 "$STATE_DIR"
    printf 'Northstar image update rollback sentinel\n' > "$SENTINEL"
    chmod 0600 "$SENTINEL"
    sentinel_sha=$(digest "$SENTINEL")
    write_state prepared "$baseline" "$candidate" "$baseline_be" none "$sentinel_sha"
    printf 'PREPARED=yes\nBASELINE=%s\nCANDIDATE=%s\nBOOT_ENVIRONMENT=%s\n' \
        "$baseline" "$candidate" "$baseline_be"
}

inject_failure() {
    [ "$(state_value stage)" = prepared ] || fail "$EX_DATAERR" 'failure injection requires prepared state'
    baseline=$(state_value baseline_version)
    candidate=$(state_value candidate_version)
    baseline_be=$(state_value baseline_boot_environment)
    sentinel_sha=$(state_value sentinel_sha256)
    cat > "$FAILURE_PKG" <<EOF
#!/bin/sh
case "\${1-}" in
upgrade) exit 86 ;;
*) exec "$PKG" "\$@" ;;
esac
EOF
    chmod 0700 "$FAILURE_PKG"
    if NORTHSTAR_UPDATE_PKG="$FAILURE_PKG" "$TRANSACTION" --apply-update --confirm; then
        fail "$EX_DATAERR" 'injected package failure unexpectedly succeeded'
    fi
    rollback_be=$(read_transaction_be)
    write_state failure-rollback-scheduled "$baseline" "$candidate" "$baseline_be" "$rollback_be" "$sentinel_sha"
    printf 'FAILURE_ROLLBACK_SCHEDULED=%s\nREBOOT_REQUIRED=yes\n' "$rollback_be"
}

verify_failure_recovery() {
    [ "$(state_value stage)" = failure-rollback-scheduled ] || fail "$EX_DATAERR" 'failure recovery is not scheduled'
    baseline=$(state_value baseline_version)
    candidate=$(state_value candidate_version)
    baseline_be=$(state_value baseline_boot_environment)
    rollback_be=$(state_value rollback_boot_environment)
    sentinel_sha=$(state_value sentinel_sha256)
    [ "$(active_be)" = "$rollback_be" ] || fail "$EX_DATAERR" 'failure rollback boot environment is not active'
    [ "$(installed_version)" = "$baseline" ] || fail "$EX_DATAERR" 'baseline package version was not recovered'
    verify_sentinel
    write_state failure-recovered "$baseline" "$candidate" "$baseline_be" "$rollback_be" "$sentinel_sha"
    printf 'FAILURE_RECOVERY_VERIFIED=yes\n'
}

normalize_after_failure() {
    [ "$(state_value stage)" = failure-recovered ] || fail "$EX_DATAERR" 'normalization requires verified failure recovery'
    baseline=$(state_value baseline_version)
    candidate=$(state_value candidate_version)
    baseline_be=$(state_value baseline_boot_environment)
    rollback_be=$(state_value rollback_boot_environment)
    sentinel_sha=$(state_value sentinel_sha256)
    [ "$(active_be)" = "$rollback_be" ] || fail "$EX_DATAERR" 'refusing to normalize a non-active rollback environment'
    [ "$baseline_be" != "$rollback_be" ] || fail "$EX_DATAERR" 'boot-environment names unexpectedly match'
    "$BECTL" destroy -F "$baseline_be"
    "$BECTL" rename "$rollback_be" "$baseline_be"
    [ "$(active_be)" = "$baseline_be" ] || fail "$EX_DATAERR" 'boot-environment normalization failed'
    write_state normalized "$baseline" "$candidate" "$baseline_be" none "$sentinel_sha"
    printf 'NORMALIZED=yes\nBOOT_ENVIRONMENT=%s\n' "$baseline_be"
}

apply_update() {
    [ "$(state_value stage)" = normalized ] || fail "$EX_DATAERR" 'successful update requires normalized state'
    baseline=$(state_value baseline_version)
    candidate=$(state_value candidate_version)
    baseline_be=$(state_value baseline_boot_environment)
    sentinel_sha=$(state_value sentinel_sha256)
    "$TRANSACTION" --apply-update --confirm
    [ "$(installed_version)" = "$candidate" ] || fail "$EX_DATAERR" 'candidate package version was not installed'
    rollback_be=$(read_transaction_be)
    verify_sentinel
    write_state updated "$baseline" "$candidate" "$baseline_be" "$rollback_be" "$sentinel_sha"
    printf 'UPDATE_VERIFIED=yes\nROLLBACK_BOOT_ENVIRONMENT=%s\n' "$rollback_be"
}

schedule_rollback() {
    [ "$(state_value stage)" = updated ] || fail "$EX_DATAERR" 'explicit rollback requires a verified update'
    baseline=$(state_value baseline_version)
    candidate=$(state_value candidate_version)
    baseline_be=$(state_value baseline_boot_environment)
    rollback_be=$(state_value rollback_boot_environment)
    sentinel_sha=$(state_value sentinel_sha256)
    "$TRANSACTION" --rollback --confirm
    write_state explicit-rollback-scheduled "$baseline" "$candidate" "$baseline_be" "$rollback_be" "$sentinel_sha"
    printf 'EXPLICIT_ROLLBACK_SCHEDULED=%s\nREBOOT_REQUIRED=yes\n' "$rollback_be"
}

verify_rollback() {
    [ "$(state_value stage)" = explicit-rollback-scheduled ] || fail "$EX_DATAERR" 'explicit rollback is not scheduled'
    baseline=$(state_value baseline_version)
    candidate=$(state_value candidate_version)
    baseline_be=$(state_value baseline_boot_environment)
    rollback_be=$(state_value rollback_boot_environment)
    sentinel_sha=$(state_value sentinel_sha256)
    [ "$(active_be)" = "$rollback_be" ] || fail "$EX_DATAERR" 'explicit rollback boot environment is not active'
    [ "$(installed_version)" = "$baseline" ] || fail "$EX_DATAERR" 'explicit rollback did not restore the baseline package'
    verify_sentinel
    write_state passed "$baseline" "$candidate" "$baseline_be" "$rollback_be" "$sentinel_sha"
    printf 'IMAGE_UPDATE_ROLLBACK_GATE=PASS\nBASELINE=%s\nCANDIDATE=%s\nHOME_PRESERVED=yes\n' "$baseline" "$candidate"
}

case "${1-}" in
--help|-h)
    usage
    exit 0
    ;;
esac

require_production_boundary
require_tools
case "${1-}" in
--prepare)
    [ "$#" -eq 5 ] && [ "$2" = --baseline-version ] && [ "$4" = --candidate-version ] || fail "$EX_USAGE" 'invalid --prepare arguments'
    prepare "$3" "$5"
    ;;
--inject-failure)
    [ "$#" -eq 1 ] || fail "$EX_USAGE" 'unexpected arguments'
    require_state_boundary
    inject_failure
    ;;
--verify-failure-recovery)
    [ "$#" -eq 1 ] || fail "$EX_USAGE" 'unexpected arguments'
    require_state_boundary
    verify_failure_recovery
    ;;
--normalize-after-failure)
    [ "$#" -eq 1 ] || fail "$EX_USAGE" 'unexpected arguments'
    require_state_boundary
    normalize_after_failure
    ;;
--apply-update)
    [ "$#" -eq 1 ] || fail "$EX_USAGE" 'unexpected arguments'
    require_state_boundary
    apply_update
    ;;
--schedule-rollback)
    [ "$#" -eq 1 ] || fail "$EX_USAGE" 'unexpected arguments'
    require_state_boundary
    schedule_rollback
    ;;
--verify-rollback)
    [ "$#" -eq 1 ] || fail "$EX_USAGE" 'unexpected arguments'
    require_state_boundary
    verify_rollback
    ;;
*)
    usage >&2
    exit "$EX_USAGE"
    ;;
esac
