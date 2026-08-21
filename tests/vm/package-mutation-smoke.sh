#!/bin/sh

# Root-isolated install/remove contract. Fake pkg and bectl tools prove that an
# opaque preview-bound plan is independently recovered, authenticated, ordered,
# verified, serialized, and rolled back without touching the host package DB.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TRANSACTION=$ROOT/src/update/northstar-package-transaction
TMP_DIR=$(mktemp -d /tmp/northstar-package-mutation.XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

[ "$(id -u)" -eq 0 ] || { echo 'FAIL: run package mutation smoke as root' >&2; exit 1; }
mkdir -p "$TMP_DIR/state" "$TMP_DIR/run"
printf '%s\n' 'home-data-survives' > "$TMP_DIR/home-sentinel"
printf '%s\n' absent > "$TMP_DIR/cowsay-state"
printf '%s\n' present > "$TMP_DIR/qterminal-state"

cat > "$TMP_DIR/policy" <<'POLICY'
protocol=1
repository=FreeBSD-ports
abi=FreeBSD:15:amd64
ports_branch=2026Q3
url=pkg+https://pkg.FreeBSD.org/FreeBSD:15:amd64/quarterly
mirror_type=SRV
signature_type=FINGERPRINTS
fingerprints=/usr/share/keys/pkg
POLICY
chmod 0600 "$TMP_DIR/policy"

cat > "$TMP_DIR/pkg" <<'PKG'
#!/bin/sh
if [ "$1" = -o ] && [ "$2" = REPO_AUTOUPDATE=false ]; then
    shift 2
fi
case "$1" in
    -vv)
        cat <<'CONFIG'
ABI = "FreeBSD:15:amd64";
Repositories:
  FreeBSD-ports: {
    url             : "pkg+https://pkg.FreeBSD.org/FreeBSD:15:amd64/quarterly",
    enabled         : yes,
    priority        : 0,
    mirror_type     : "SRV",
    signature_type  : "FINGERPRINTS",
    fingerprints    : "/usr/share/keys/pkg"
  }
CONFIG
        ;;
    update)
        printf '%s\n' update >> "$NORTHSTAR_TEST_EVENTS"
        ;;
    rquery)
        if [ "${NORTHSTAR_TEST_ALTER_CATALOGUE:-0}" -eq 1 ]; then
            printf '%s\n' 'cowsay|3.04_4|games/cowsay' 'qterminal|2.4.0|x11/qterminal' 'xterm|410|x11/xterm'
        else
            printf '%s\n' 'cowsay|3.04_3|games/cowsay' 'qterminal|2.4.0|x11/qterminal' 'xterm|410|x11/xterm'
        fi
        ;;
    query)
        if [ "$2" = -a ]; then
            [ "$(cat "$NORTHSTAR_TEST_COWSAY_STATE")" = present ] \
                && printf 'cowsay|3.04_3|games/cowsay|0|FreeBSD-ports|0\n'
            [ "$(cat "$NORTHSTAR_TEST_QTERMINAL_STATE")" = present ] \
                && printf 'qterminal|2.4.0|x11/qterminal|0|FreeBSD-ports|%s\n' "${NORTHSTAR_TEST_LOCKED:-0}"
        elif [ "$2" = -e ]; then
            case "$3" in
                *cowsay*) state=$(cat "$NORTHSTAR_TEST_COWSAY_STATE"); value='3.04_3|games/cowsay|FreeBSD-ports' ;;
                *qterminal*) state=$(cat "$NORTHSTAR_TEST_QTERMINAL_STATE"); value='2.4.0|x11/qterminal|FreeBSD-ports' ;;
                *) state=absent; value= ;;
            esac
            [ "$state" = present ] || exit 1
            [ "$#" -lt 4 ] || printf '%s\n' "$value"
        else
            exit 2
        fi
        ;;
    install)
        if [ "$2" = -n ] && [ "$3" = -y ]; then
            printf '%s\n' 'INSTALL PREVIEW: cowsay 3.04_3 from FreeBSD-ports'
        else
            printf '%s\n' install >> "$NORTHSTAR_TEST_EVENTS"
            [ "${NORTHSTAR_TEST_FAIL:-0}" -eq 0 ] || exit 1
            printf '%s\n' present > "$NORTHSTAR_TEST_COWSAY_STATE"
        fi
        ;;
    delete)
        if [ "$2" = -n ] && [ "$3" = -y ]; then
            printf '%s\n' 'REMOVE PREVIEW: qterminal 2.4.0 from FreeBSD-ports'
        else
            printf '%s\n' remove >> "$NORTHSTAR_TEST_EVENTS"
            [ "${NORTHSTAR_TEST_FAIL:-0}" -eq 0 ] || exit 1
            printf '%s\n' absent > "$NORTHSTAR_TEST_QTERMINAL_STATE"
        fi
        ;;
    *) exit 2 ;;
esac
PKG

cat > "$TMP_DIR/bectl" <<'BECTL'
#!/bin/sh
printf '%s\n' "$1" >> "$NORTHSTAR_TEST_EVENTS"
BECTL

cat > "$TMP_DIR/zfs" <<'ZFS'
#!/bin/sh
exit 0
ZFS
chmod 0700 "$TMP_DIR/pkg" "$TMP_DIR/bectl" "$TMP_DIR/zfs"

canonical_catalogue() {
    printf '%s\n' 'cowsay|3.04_3|games/cowsay' 'qterminal|2.4.0|x11/qterminal' 'xterm|410|x11/xterm'
}

make_plan() {
    operation=$1
    index=$2
    name=$3
    version=$4
    origin=$5
    preview=$6
    timestamp=$(date +%s)
    catalogue_sha256=$(canonical_catalogue | sort -u | sha256 -q)
    record_hash=$({
        printf 'protocol=1\ntimestamp=%s\noperation=%s\nrepository=FreeBSD-ports\n' "$timestamp" "$operation"
        printf 'catalogue_sha256=%s\nindex=%s\nname=%s\nversion=%s\norigin=%s\n' \
            "$catalogue_sha256" "$index" "$name" "$version" "$origin"
    } | sha256 -q)
    preview_hash=$(printf '%s' "$preview" | sha256 -q)
    printf '%010d-%08d-%s-%s\n' "$timestamp" "$index" "$record_hash" "$preview_hash"
}

run_transaction() {
    env NORTHSTAR_PACKAGE_PKG="$TMP_DIR/pkg" \
        NORTHSTAR_PACKAGE_TEST_MODE=1 \
        NORTHSTAR_PACKAGE_BECTL="$TMP_DIR/bectl" \
        NORTHSTAR_PACKAGE_ZFS="$TMP_DIR/zfs" \
        NORTHSTAR_PACKAGE_SHA256=/sbin/sha256 \
        NORTHSTAR_PACKAGE_POLICY="$TMP_DIR/policy" \
        NORTHSTAR_PACKAGE_STATE_DIR="$TMP_DIR/state" \
        NORTHSTAR_PACKAGE_RUN_DIR="$TMP_DIR/run" \
        NORTHSTAR_TEST_EVENTS="$TMP_DIR/events" \
        NORTHSTAR_TEST_COWSAY_STATE="$TMP_DIR/cowsay-state" \
        NORTHSTAR_TEST_QTERMINAL_STATE="$TMP_DIR/qterminal-state" \
        NORTHSTAR_TEST_FAIL="${NORTHSTAR_TEST_FAIL:-0}" \
        NORTHSTAR_TEST_LOCKED="${NORTHSTAR_TEST_LOCKED:-0}" \
        NORTHSTAR_TEST_ALTER_CATALOGUE="${NORTHSTAR_TEST_ALTER_CATALOGUE:-0}" \
        sh "$TRANSACTION" --apply-plan "$1" --confirm
}

: > "$TMP_DIR/events"
install_plan=$(make_plan install 0 cowsay 3.04_3 games/cowsay \
    'INSTALL PREVIEW: cowsay 3.04_3 from FreeBSD-ports')
run_transaction "$install_plan"
[ "$(paste -sd, "$TMP_DIR/events")" = 'update,create,install' ] \
    || { echo 'FAIL: install did not create a boot environment before pkg' >&2; exit 1; }
grep -Fx 'status=completed' "$TMP_DIR/state/package-state.conf" >/dev/null
[ "$(cat "$TMP_DIR/home-sentinel")" = home-data-survives ]

if run_transaction "$install_plan" >/dev/null 2>&1; then
    echo 'FAIL: replayed install plan unexpectedly succeeded' >&2
    exit 1
fi

: > "$TMP_DIR/events"
remove_plan=$(make_plan remove 1 qterminal 2.4.0 x11/qterminal \
    'REMOVE PREVIEW: qterminal 2.4.0 from FreeBSD-ports')
run_transaction "$remove_plan"
[ "$(paste -sd, "$TMP_DIR/events")" = 'update,create,remove' ] \
    || { echo 'FAIL: removal did not create a boot environment before pkg' >&2; exit 1; }
[ "$(cat "$TMP_DIR/qterminal-state")" = absent ]

printf '%s\n' present > "$TMP_DIR/qterminal-state"
NORTHSTAR_TEST_LOCKED=1
export NORTHSTAR_TEST_LOCKED
locked_plan=$(make_plan remove 1 qterminal 2.4.0 x11/qterminal \
    'REMOVE PREVIEW: qterminal 2.4.0 from FreeBSD-ports')
if run_transaction "$locked_plan" >/dev/null 2>&1; then
    echo 'FAIL: locked removal unexpectedly succeeded' >&2
    exit 1
fi
NORTHSTAR_TEST_LOCKED=0
export NORTHSTAR_TEST_LOCKED

printf '%s\n' absent > "$TMP_DIR/cowsay-state"
NORTHSTAR_TEST_ALTER_CATALOGUE=1
export NORTHSTAR_TEST_ALTER_CATALOGUE
altered_plan=$(make_plan install 0 cowsay 3.04_3 games/cowsay \
    'INSTALL PREVIEW: cowsay 3.04_3 from FreeBSD-ports')
if run_transaction "$altered_plan" >/dev/null 2>&1; then
    echo 'FAIL: altered catalogue unexpectedly matched the original plan' >&2
    exit 1
fi
NORTHSTAR_TEST_ALTER_CATALOGUE=0
export NORTHSTAR_TEST_ALTER_CATALOGUE

: > "$TMP_DIR/events"
NORTHSTAR_TEST_FAIL=1
export NORTHSTAR_TEST_FAIL
failure_plan=$(make_plan install 0 cowsay 3.04_3 games/cowsay \
    'INSTALL PREVIEW: cowsay 3.04_3 from FreeBSD-ports')
if run_transaction "$failure_plan" >/dev/null 2>&1; then
    echo 'FAIL: injected package failure unexpectedly succeeded' >&2
    exit 1
fi
[ "$(paste -sd, "$TMP_DIR/events")" = 'update,create,install,activate' ] \
    || { echo 'FAIL: failed package action did not schedule boot-environment rollback' >&2; exit 1; }
grep -Fx 'status=rollback-scheduled' "$TMP_DIR/state/package-state.conf" >/dev/null
[ "$(cat "$TMP_DIR/home-sentinel")" = home-data-survives ]

printf '%s\n' 'PASS: opaque install/remove plans, ordering, verification, rejection, rollback, and home preservation'
