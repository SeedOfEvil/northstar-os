#!/bin/sh

set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
ENGINE=$ROOT/apps/installer/northstar-installer-engine
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-installer-engine.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM
BIN=$TMP_DIR/bin
FAKE_ROOT=$TMP_DIR/root
mkdir -p "$BIN" "$FAKE_ROOT"
fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }

cat > "$BIN/geom" <<'EOF'
#!/bin/sh
[ "$3" = da1 ] || exit 1
printf '%s\n' 'Geom name: da1' '  Mediasize: 68719476736 (64G)' \
  '  Sectorsize: 512' '  descr: Northstar Test Disk'
EOF
cat > "$BIN/mount" <<'EOF'
#!/bin/sh
[ "${NORTHSTAR_TEST_ACTIVE:-0}" != 1 ] || printf '%s\n' '/dev/da1p2 /mnt ufs rw 1 1'
EOF
for tool in zpool swapinfo; do
cat > "$BIN/$tool" <<'EOF'
#!/bin/sh
exit 0
EOF
done
cat > "$BIN/sha256" <<'EOF'
#!/bin/sh
[ "${1:-}" = -q ] && shift
if command -v sha256sum >/dev/null 2>&1; then
    if [ "$#" -eq 1 ]; then sha256sum "$1"; else sha256sum; fi | awk '{ print $1 }'
else
    exec /sbin/sha256 -q "$@"
fi
EOF
cat > "$BIN/source-verify" <<'EOF'
#!/bin/sh
[ "$1" = --verify ] && [ "$#" -eq 2 ] || exit 64
[ "${NORTHSTAR_TEST_SOURCE_FAIL:-0}" != 1 ] || exit 77
printf '%s\n' 'SOURCE_VERIFICATION=PASS' "MANIFEST_SHA256=$2" \
  'PAYLOAD_KIND=northstar-rootfs-v1' 'PAYLOAD_NAME=northstar-runtime.txz' 'PAYLOAD_SIZE=4096' \
  'PAYLOAD_SHA256=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef' \
  'PROJECT_COMMIT=0123456789abcdef0123456789abcdef01234567' \
  'RUNTIME_MANIFEST_SHA256=abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789' \
  'SOURCE_MUTATION=none'
EOF
cat > "$BIN/install" <<'EOF'
#!/bin/sh
last=
for argument in "$@"; do last=$argument; done
mkdir -p "$last"
EOF
cat > "$BIN/chown" <<'EOF'
#!/bin/sh
exit 0
EOF
for tool in chmod mv rm; do
cat > "$BIN/$tool" <<EOF
#!/bin/sh
exec /bin/$tool "\$@"
EOF
done
chmod +x "$BIN"/*

DESCRIPTION_DIGEST=$(printf '%s' 'Northstar Test Disk' | "$BIN/sha256" -q)
REQUEST=$TMP_DIR/install.request
cat > "$REQUEST" <<EOF
protocol=1
operation=install
target_device=da1
target_mediasize=68719476736
target_sectorsize=512
target_description_sha256=$DESCRIPTION_DIGEST
layout=gpt-uefi-zfs
pool_name=nstar_0123456789ab
source_manifest_sha256=abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789
confirmation=erase-target
plan_status=verified
EOF
chmod 600 "$REQUEST"

run_engine() {
    env NORTHSTAR_INSTALLER_ENGINE_TEST_MODE=1 \
        NORTHSTAR_INSTALLER_ENGINE_ROOT="$FAKE_ROOT" \
        NORTHSTAR_INSTALLER_ENGINE_GEOM="$BIN/geom" \
        NORTHSTAR_INSTALLER_ENGINE_MOUNT="$BIN/mount" \
        NORTHSTAR_INSTALLER_ENGINE_ZPOOL="$BIN/zpool" \
        NORTHSTAR_INSTALLER_ENGINE_SWAPINFO="$BIN/swapinfo" \
        NORTHSTAR_INSTALLER_ENGINE_SHA256="$BIN/sha256" \
        NORTHSTAR_INSTALLER_ENGINE_INSTALL="$BIN/install" \
        NORTHSTAR_INSTALLER_ENGINE_CHOWN="$BIN/chown" \
        NORTHSTAR_INSTALLER_ENGINE_CHMOD="$BIN/chmod" \
        NORTHSTAR_INSTALLER_ENGINE_MV="$BIN/mv" \
        NORTHSTAR_INSTALLER_ENGINE_RM="$BIN/rm" \
        NORTHSTAR_INSTALLER_ENGINE_CP=/bin/cp \
        NORTHSTAR_INSTALLER_ENGINE_SOURCE_VERIFY="$BIN/source-verify" \
        NORTHSTAR_TEST_ACTIVE="${NORTHSTAR_TEST_ACTIVE:-0}" \
        NORTHSTAR_TEST_SOURCE_FAIL="${NORTHSTAR_TEST_SOURCE_FAIL:-0}" \
        sh "$ENGINE" "$@"
}

run_engine --capabilities | grep -Fx 'disk_mutation=not-implemented' >/dev/null \
    || fail 'capabilities do not state the no-mutation boundary'
run_engine --validate "$REQUEST" | grep -Fx 'INSTALLER_REQUEST=VALID' >/dev/null \
    || fail 'valid request was rejected'

cp "$REQUEST" "$TMP_DIR/partition.request"
sed -i.bak 's/target_device=da1/target_device=da1p2/' "$TMP_DIR/partition.request"
if run_engine --validate "$TMP_DIR/partition.request" >/dev/null 2>&1; then
    fail 'partition target was accepted as a whole disk'
fi

cp "$REQUEST" "$TMP_DIR/drift.request"
sed -i.bak 's/target_mediasize=68719476736/target_mediasize=68719476737/' "$TMP_DIR/drift.request"
if run_engine --stage "$TMP_DIR/drift.request" >/dev/null 2>&1; then
    fail 'target identity drift was accepted'
fi
if NORTHSTAR_TEST_ACTIVE=1 run_engine --stage "$REQUEST" >/dev/null 2>&1; then
    fail 'active target was staged'
fi
if NORTHSTAR_TEST_SOURCE_FAIL=1 run_engine --stage "$REQUEST" >/dev/null 2>&1; then
    fail 'failed installer source verification was accepted'
fi
[ ! -e "$FAKE_ROOT/var/db/northstar/installer/active.conf" ] \
    || fail 'failed source verification wrote active transaction state'

run_engine --stage "$REQUEST" > "$TMP_DIR/output"
grep -Fx 'INSTALLER_PREFLIGHT=PASS' "$TMP_DIR/output" >/dev/null \
    || fail 'verified target did not pass independent preflight'
grep -Fx 'SOURCE_VERIFICATION=PASS' "$TMP_DIR/output" >/dev/null \
    || fail 'staging did not report trusted source verification'
ACTIVE=$FAKE_ROOT/var/db/northstar/installer/active.conf
[ -f "$ACTIVE" ] || fail 'active transaction pointer was not staged'
TRANSACTION_ID=$(awk -F= '$1 == "transaction_id" { print $2 }' "$ACTIVE")
[ -n "$TRANSACTION_ID" ] || fail 'active transaction identity is missing'
STATE=$FAKE_ROOT/var/db/northstar/installer/transactions/$TRANSACTION_ID/transaction.conf
JOURNAL=$FAKE_ROOT/var/db/northstar/installer/transactions/$TRANSACTION_ID/journal.log
[ -f "$STATE" ] && [ -f "$JOURNAL" ] || fail 'root-owned transaction and journal were not staged'
grep -Fx 'status=staged' "$STATE" >/dev/null || fail 'staged status is wrong'
grep -Fx 'execution=guarded-executor-only' "$STATE" >/dev/null \
    || fail 'staged state is not restricted to the guarded executor'
grep -Fx 'target_device=da1' "$STATE" >/dev/null || fail 'staged target is wrong'
grep -Fx 'payload_name=northstar-runtime.txz' "$STATE" >/dev/null \
    || fail 'staged state does not bind the verified payload'
[ "$(wc -l < "$JOURNAL" | tr -d ' ')" -eq 4 ] || fail 'initial journal event sequence is incomplete'
grep -Fx '0002|source-verified' "$JOURNAL" >/dev/null || fail 'source verification was not journaled'

STAGED_REQUEST=$FAKE_ROOT/var/db/northstar/installer/transactions/$TRANSACTION_ID/request.conf
cp "$STAGED_REQUEST" "$TMP_DIR/request.backup"
printf '%s\n' 'tampered=1' >> "$STAGED_REQUEST"
if run_engine --status >/dev/null 2>&1; then
    fail 'altered staged request was accepted by status validation'
fi
mv "$TMP_DIR/request.backup" "$STAGED_REQUEST"
cp "$JOURNAL" "$TMP_DIR/journal.backup"
printf '%s\n' '0005|unknown-event' >> "$JOURNAL"
if run_engine --status >/dev/null 2>&1; then
    fail 'invalid transaction journal transition was accepted'
fi
mv "$TMP_DIR/journal.backup" "$JOURNAL"

if run_engine --stage "$REQUEST" >/dev/null 2>&1; then
    fail 'existing unresolved installer state was overwritten'
fi

run_engine --status | grep -Fx 'INSTALLER_STATUS=staged' >/dev/null \
    || fail 'active staged transaction was not reported'
rm -f "$ACTIVE"
run_engine --status | grep -Fx 'INSTALLER_STATUS=interrupted' >/dev/null \
    || fail 'interrupted transaction was not detected'
run_engine --recover "$TRANSACTION_ID" > "$TMP_DIR/recover.out"
grep -Fx 'INSTALLER_RECOVERY=PASS' "$TMP_DIR/recover.out" >/dev/null \
    || fail 'interrupted transaction could not be recovered'
grep -Fx '0005|transaction-recovered' "$JOURNAL" >/dev/null \
    || fail 'transaction recovery was not journaled'
run_engine --abandon "$TRANSACTION_ID" --confirm > "$TMP_DIR/abandon.out"
grep -Fx 'INSTALLER_ABANDON=PASS' "$TMP_DIR/abandon.out" >/dev/null \
    || fail 'staged transaction could not be explicitly abandoned'
[ ! -e "$ACTIVE" ] || fail 'abandoned transaction remained active'
ARCHIVE=$FAKE_ROOT/var/db/northstar/installer/archive/$TRANSACTION_ID
[ -d "$ARCHIVE" ] || fail 'abandoned transaction was not archived'
grep -Fx '0006|transaction-abandoned' "$ARCHIVE/journal.log" >/dev/null \
    || fail 'transaction abandonment was not journaled'
run_engine --status | grep -Fx 'INSTALLER_STATUS=idle' >/dev/null \
    || fail 'installer did not return to idle after abandonment'

if grep -Eq '(^|[[:space:]])(gpart|newfs|dd)([[:space:]]|$)|zpool[[:space:]]+(create|destroy)' "$ENGINE"; then
    fail 'engine foundation contains a disk mutation command'
fi
printf '%s\n' 'PASS: installer engine verifies source, revalidates target, and journals recoverable no-mutation state'
