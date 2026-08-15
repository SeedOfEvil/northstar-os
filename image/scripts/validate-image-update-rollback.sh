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

BASELINE_VERSION=
CANDIDATE_VERSION=
IMAGE_COMMIT=
REPOSITORY_REVISION=
CANDIDATE_SOURCE_REVISION=
CATALOGUE_SHA256=
SIGNATURE_FINGERPRINT=
BASELINE_BE=
ROLLBACK_BE=
SENTINEL_SHA256=

fail() {
    status=$1
    shift
    printf 'ERROR: %s\n' "$*" >&2
    exit "$status"
}

usage() {
    cat <<'EOF'
Usage:
  validate-image-update-rollback.sh --prepare \
    --baseline-version V --candidate-version V --image-commit COMMIT \
    --repository-revision N --candidate-source COMMIT \
    --catalogue-sha256 SHA256 --signature-fingerprint SHA256
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
    var_dataset=$(df -T /var 2>/dev/null | awk 'NR == 2 { print $1 }')
    [ -n "$root_dataset" ] && [ -n "$home_dataset" ] && [ "$root_dataset" != "$home_dataset" ] ||
        fail "$EX_NOPERM" '/home must be a separate dataset so rollback preserves evidence'
    [ -n "$var_dataset" ] && [ "$var_dataset" = "$root_dataset" ] ||
        fail "$EX_NOPERM" '/var must belong to the root boot environment so package state rolls back'
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
    mkdir -p "$STATE_DIR"
    tmp=$STATE.tmp.$$
    {
        printf 'schema_version=2\n'
        printf 'stage=%s\n' "$stage"
        printf 'baseline_version=%s\n' "$BASELINE_VERSION"
        printf 'candidate_version=%s\n' "$CANDIDATE_VERSION"
        printf 'image_commit=%s\n' "$IMAGE_COMMIT"
        printf 'repository_revision=%s\n' "$REPOSITORY_REVISION"
        printf 'candidate_source_revision=%s\n' "$CANDIDATE_SOURCE_REVISION"
        printf 'catalogue_sha256=%s\n' "$CATALOGUE_SHA256"
        printf 'signature_fingerprint=%s\n' "$SIGNATURE_FINGERPRINT"
        printf 'baseline_boot_environment=%s\n' "$BASELINE_BE"
        printf 'rollback_boot_environment=%s\n' "$ROLLBACK_BE"
        printf 'sentinel_sha256=%s\n' "$SENTINEL_SHA256"
    } > "$tmp"
    chmod 0600 "$tmp"
    mv "$tmp" "$STATE"
}

load_state() {
    [ "$(state_value schema_version)" = 2 ] || fail "$EX_DATAERR" 'validation state schema is unsupported'
    BASELINE_VERSION=$(state_value baseline_version)
    CANDIDATE_VERSION=$(state_value candidate_version)
    IMAGE_COMMIT=$(state_value image_commit)
    REPOSITORY_REVISION=$(state_value repository_revision)
    CANDIDATE_SOURCE_REVISION=$(state_value candidate_source_revision)
    CATALOGUE_SHA256=$(state_value catalogue_sha256)
    SIGNATURE_FINGERPRINT=$(state_value signature_fingerprint)
    BASELINE_BE=$(state_value baseline_boot_environment)
    ROLLBACK_BE=$(state_value rollback_boot_environment)
    SENTINEL_SHA256=$(state_value sentinel_sha256)
}

matches() {
    printf '%s\n' "$1" | grep -Eq "$2"
}

marker_value() {
    key=$1
    [ -f "$IMAGE_MARKER" ] && [ ! -L "$IMAGE_MARKER" ] || fail "$EX_NOPERM" 'installed Northstar image marker is missing'
    value=$(awk -F= -v key="$key" '$1 == key { if (found++) exit 2; print substr($0, length(key) + 2) }' "$IMAGE_MARKER") \
        || fail "$EX_DATAERR" "image marker repeats $key"
    [ -n "$value" ] || fail "$EX_DATAERR" "image marker omits $key"
    printf '%s\n' "$value"
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
    [ -f "$SENTINEL" ] && [ "$(digest "$SENTINEL")" = "$SENTINEL_SHA256" ] ||
        fail "$EX_DATAERR" 'home sentinel was not preserved'
}

transaction_value() {
    key=$1
    transaction_state=${NORTHSTAR_UPDATE_STATE_DIR:-/var/db/northstar}/update-state.conf
    [ -f "$transaction_state" ] || fail "$EX_DATAERR" 'transaction state was not created'
    value=$(awk -F= -v key="$key" '$1 == key { if (found++) exit 2; print substr($0, length(key) + 2) }' "$transaction_state") \
        || fail "$EX_DATAERR" "transaction state repeats $key"
    [ -n "$value" ] || fail "$EX_DATAERR" "transaction state omits $key"
    printf '%s\n' "$value"
}

verify_transaction_binding() {
    [ "$(transaction_value repository_revision)" = "$REPOSITORY_REVISION" ] || fail "$EX_DATAERR" 'transaction repository revision changed'
    [ "$(transaction_value source_revision)" = "$CANDIDATE_SOURCE_REVISION" ] || fail "$EX_DATAERR" 'transaction source revision changed'
    [ "$(transaction_value catalogue_sha256)" = "$CATALOGUE_SHA256" ] || fail "$EX_DATAERR" 'transaction catalogue digest changed'
    [ "$(transaction_value signature_fingerprint)" = "$SIGNATURE_FINGERPRINT" ] || fail "$EX_DATAERR" 'transaction signature fingerprint changed'
}

read_transaction_be() {
    verify_transaction_binding
    transaction_value boot_environment
}

prepare() {
    baseline=$1
    candidate=$2
    image_commit=$3
    repository_revision=$4
    candidate_source=$5
    catalogue_sha256=$6
    signature_fingerprint=$7
    [ ! -e "$STATE_DIR" ] || fail "$EX_DATAERR" "validation state already exists: $STATE_DIR"
    matches "$baseline" '^[A-Za-z0-9][A-Za-z0-9_.+~,:-]{0,63}$' || fail "$EX_DATAERR" 'baseline version is unsafe'
    matches "$candidate" '^[A-Za-z0-9][A-Za-z0-9_.+~,:-]{0,63}$' || fail "$EX_DATAERR" 'candidate version is unsafe'
    matches "$image_commit" '^[0-9A-Fa-f]{40,64}$' || fail "$EX_DATAERR" 'image commit is not resolved'
    case "$repository_revision" in ''|*[!0-9]*) fail "$EX_DATAERR" 'repository revision is not numeric' ;; esac
    [ "${#repository_revision}" -le 9 ] || fail "$EX_DATAERR" 'repository revision is too long'
    matches "$candidate_source" '^[0-9A-Fa-f]{40,64}$' || fail "$EX_DATAERR" 'candidate source revision is not resolved'
    matches "$catalogue_sha256" '^[0-9A-Fa-f]{64}$' || fail "$EX_DATAERR" 'catalogue digest is not SHA-256'
    matches "$signature_fingerprint" '^[0-9A-Fa-f]{64}$' || fail "$EX_DATAERR" 'signature fingerprint is not SHA-256'
    [ "$(marker_value project_commit)" = "$image_commit" ] || fail "$EX_DATAERR" 'installed image commit does not match the accepted baseline'
    [ "$(installed_version)" = "$baseline" ] || fail "$EX_DATAERR" "installed $PACKAGE is not baseline $baseline"
    available=$("$PKG" rquery -r "$REPOSITORY" -e "%n == $PACKAGE" '%v' 2>/dev/null || true)
    [ "$available" = "$candidate" ] || fail "$EX_DATAERR" "repository candidate is not $candidate"
    baseline_be=$(active_be) || fail "$EX_DATAERR" 'could not identify exactly one active boot environment'
    BASELINE_VERSION=$baseline
    CANDIDATE_VERSION=$candidate
    IMAGE_COMMIT=$image_commit
    REPOSITORY_REVISION=$repository_revision
    CANDIDATE_SOURCE_REVISION=$candidate_source
    CATALOGUE_SHA256=$(printf '%s' "$catalogue_sha256" | tr '[:upper:]' '[:lower:]')
    SIGNATURE_FINGERPRINT=$(printf '%s' "$signature_fingerprint" | tr '[:upper:]' '[:lower:]')
    BASELINE_BE=$baseline_be
    ROLLBACK_BE=none
    mkdir -p "$STATE_DIR"
    chmod 0700 "$STATE_DIR"
    printf 'Northstar image update rollback sentinel\n' > "$SENTINEL"
    chmod 0600 "$SENTINEL"
    SENTINEL_SHA256=$(digest "$SENTINEL")
    write_state prepared
    printf 'PREPARED=yes\nBASELINE=%s\nCANDIDATE=%s\nBOOT_ENVIRONMENT=%s\n' \
        "$baseline" "$candidate" "$baseline_be"
}

inject_failure() {
    [ "$(state_value stage)" = prepared ] || fail "$EX_DATAERR" 'failure injection requires prepared state'
    load_state
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
    ROLLBACK_BE=$(read_transaction_be)
    write_state failure-rollback-scheduled
    printf 'FAILURE_ROLLBACK_SCHEDULED=%s\nREBOOT_REQUIRED=yes\n' "$ROLLBACK_BE"
}

verify_failure_recovery() {
    [ "$(state_value stage)" = failure-rollback-scheduled ] || fail "$EX_DATAERR" 'failure recovery is not scheduled'
    load_state
    [ "$(active_be)" = "$ROLLBACK_BE" ] || fail "$EX_DATAERR" 'failure rollback boot environment is not active'
    [ "$(installed_version)" = "$BASELINE_VERSION" ] || fail "$EX_DATAERR" 'baseline package version was not recovered'
    verify_sentinel
    write_state failure-recovered
    printf 'FAILURE_RECOVERY_VERIFIED=yes\n'
}

normalize_after_failure() {
    [ "$(state_value stage)" = failure-recovered ] || fail "$EX_DATAERR" 'normalization requires verified failure recovery'
    load_state
    [ "$(active_be)" = "$ROLLBACK_BE" ] || fail "$EX_DATAERR" 'refusing to normalize a non-active rollback environment'
    [ "$BASELINE_BE" != "$ROLLBACK_BE" ] || fail "$EX_DATAERR" 'boot-environment names unexpectedly match'
    "$BECTL" destroy -F "$BASELINE_BE"
    "$BECTL" rename "$ROLLBACK_BE" "$BASELINE_BE"
    [ "$(active_be)" = "$BASELINE_BE" ] || fail "$EX_DATAERR" 'boot-environment normalization failed'
    ROLLBACK_BE=none
    write_state normalized
    printf 'NORMALIZED=yes\nBOOT_ENVIRONMENT=%s\n' "$BASELINE_BE"
}

apply_update() {
    [ "$(state_value stage)" = normalized ] || fail "$EX_DATAERR" 'successful update requires normalized state'
    load_state
    "$TRANSACTION" --apply-update --confirm
    [ "$(installed_version)" = "$CANDIDATE_VERSION" ] || fail "$EX_DATAERR" 'candidate package version was not installed'
    ROLLBACK_BE=$(read_transaction_be)
    verify_sentinel
    write_state updated
    printf 'UPDATE_VERIFIED=yes\nROLLBACK_BOOT_ENVIRONMENT=%s\n' "$ROLLBACK_BE"
}

schedule_rollback() {
    [ "$(state_value stage)" = updated ] || fail "$EX_DATAERR" 'explicit rollback requires a verified update'
    load_state
    "$TRANSACTION" --rollback --confirm
    write_state explicit-rollback-scheduled
    printf 'EXPLICIT_ROLLBACK_SCHEDULED=%s\nREBOOT_REQUIRED=yes\n' "$ROLLBACK_BE"
}

verify_rollback() {
    [ "$(state_value stage)" = explicit-rollback-scheduled ] || fail "$EX_DATAERR" 'explicit rollback is not scheduled'
    load_state
    [ "$(active_be)" = "$ROLLBACK_BE" ] || fail "$EX_DATAERR" 'explicit rollback boot environment is not active'
    [ "$(installed_version)" = "$BASELINE_VERSION" ] || fail "$EX_DATAERR" 'explicit rollback did not restore the baseline package'
    verify_sentinel
    write_state passed
    printf 'IMAGE_UPDATE_ROLLBACK_GATE=PASS\nBASELINE=%s\nCANDIDATE=%s\nHOME_PRESERVED=yes\n' "$BASELINE_VERSION" "$CANDIDATE_VERSION"
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
    [ "$#" -eq 15 ] \
        && [ "$2" = --baseline-version ] && [ "$4" = --candidate-version ] \
        && [ "$6" = --image-commit ] && [ "$8" = --repository-revision ] \
        && [ "${10}" = --candidate-source ] && [ "${12}" = --catalogue-sha256 ] \
        && [ "${14}" = --signature-fingerprint ] \
        || fail "$EX_USAGE" 'invalid --prepare arguments'
    prepare "$3" "$5" "$7" "$9" "${11}" "${13}" "${15}"
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
