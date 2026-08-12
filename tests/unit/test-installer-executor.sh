#!/bin/sh

set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
EXECUTOR=$ROOT/apps/installer/northstar-installer-executor
ENGINE=$ROOT/apps/installer/northstar-installer-engine
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-installer-executor.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM
BIN=$TMP_DIR/bin
SOURCE=$TMP_DIR/source
LOG=$TMP_DIR/commands.log
RUNTIME_MANIFEST=$TMP_DIR/runtime-manifest.conf
mkdir -p "$BIN" "$SOURCE"
: > "$LOG"
fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }

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
  'PAYLOAD_KIND=northstar-rootfs-v1' 'PAYLOAD_NAME=northstar-rootfs.txz' \
  "PAYLOAD_SIZE=$NORTHSTAR_TEST_PAYLOAD_SIZE" "PAYLOAD_SHA256=$NORTHSTAR_TEST_PAYLOAD_SHA" \
  'PROJECT_COMMIT=0123456789abcdef0123456789abcdef01234567' \
  "RUNTIME_MANIFEST_SHA256=$NORTHSTAR_TEST_RUNTIME_SHA" 'SOURCE_MUTATION=none'
EOF
cat > "$BIN/geom" <<'EOF'
#!/bin/sh
[ "$3" = "${NORTHSTAR_TEST_TARGET:-md42}" ] || exit 1
size=68719476736
[ "${NORTHSTAR_TEST_TARGET_DRIFT:-0}" != 1 ] || size=68719476737
printf '%s\n' "Geom name: $3" "  Mediasize: $size (64G)" \
  '  Sectorsize: 512' '  descr: Northstar Disposable Disk'
EOF
cat > "$BIN/mount" <<'EOF'
#!/bin/sh
exit 0
EOF
cat > "$BIN/swapinfo" <<'EOF'
#!/bin/sh
exit 0
EOF
cat > "$BIN/gpart" <<'EOF'
#!/bin/sh
printf 'gpart %s\n' "$*" >> "$NORTHSTAR_TEST_LOG"
[ "$1" != show ]
EOF
cat > "$BIN/zpool" <<'EOF'
#!/bin/sh
if [ "$1" = status ]; then exit 0; fi
printf 'zpool %s\n' "$*" >> "$NORTHSTAR_TEST_LOG"
exit 0
EOF
for tool in zfs newfs_msdos mount_msdosfs umount; do
cat > "$BIN/$tool" <<'EOF'
#!/bin/sh
printf '%s %s\n' "${0##*/}" "$*" >> "$NORTHSTAR_TEST_LOG"
exit 0
EOF
done
cat > "$BIN/tar" <<'EOF'
#!/bin/sh
printf 'tar %s\n' "$*" >> "$NORTHSTAR_TEST_LOG"
case "$1" in
  -tf)
    printf '%s\n' boot/loader.efi boot/loader.conf etc/fstab etc/rc.conf \
      var/db/northstar/runtime-manifest.conf \
      usr/local/bin/northstar-session usr/local/bin/northstar-shell \
      usr/local/share/xsessions/northstar.desktop ;;
  -xpf)
    shift 2
    [ "$1" = -C ] || exit 64
    destination=$2
    mkdir -p "$destination/boot" "$destination/etc" \
      "$destination/var/db/northstar" "$destination/usr/local/bin" \
      "$destination/usr/local/share/xsessions"
    printf '%s\n' loader > "$destination/boot/loader.efi"
    printf '%s\n' 'zfs_load="YES"' > "$destination/boot/loader.conf"
    printf '%s\n' '/dev/msdosfs/NSTAR_EFI /boot/efi msdosfs rw,noatime 0 0' > "$destination/etc/fstab"
    printf '%s\n' 'zfs_enable="YES"' > "$destination/etc/rc.conf"
    cp "$NORTHSTAR_TEST_RUNTIME_MANIFEST" "$destination/var/db/northstar/runtime-manifest.conf"
    printf '%s\n' '#!/bin/sh' 'exit 0' > "$destination/usr/local/bin/northstar-session"
    printf '%s\n' '#!/bin/sh' 'exit 0' > "$destination/usr/local/bin/northstar-shell"
    chmod +x "$destination/usr/local/bin/northstar-session" "$destination/usr/local/bin/northstar-shell"
    printf '%s\n' desktop > "$destination/usr/local/share/xsessions/northstar.desktop" ;;
  *) exit 64 ;;
esac
EOF
cat > "$BIN/install" <<'EOF'
#!/bin/sh
for argument in "$@"; do
    case "$argument" in /*) mkdir -p "$argument" ;; esac
done
EOF
cat > "$BIN/chown" <<'EOF'
#!/bin/sh
exit 0
EOF
for tool in chmod cp mv rm; do
cat > "$BIN/$tool" <<EOF
#!/bin/sh
exec /bin/$tool "\$@"
EOF
done
chmod +x "$BIN"/*

printf '%s\n' 'schema_version=1' 'package_count=236' \
  'project_commit=0123456789abcdef0123456789abcdef01234567' > "$RUNTIME_MANIFEST"
printf '%s\n' 'signed Northstar root filesystem fixture' > "$SOURCE/northstar-rootfs.txz"
PAYLOAD_SIZE=$(wc -c < "$SOURCE/northstar-rootfs.txz" | tr -d ' ')
PAYLOAD_SHA=$("$BIN/sha256" -q "$SOURCE/northstar-rootfs.txz")
RUNTIME_SHA=$("$BIN/sha256" -q "$RUNTIME_MANIFEST")
MANIFEST_SHA=abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789
DESCRIPTION_SHA=$(printf '%s' 'Northstar Disposable Disk' | "$BIN/sha256" -q)

prepare_fixture() {
    fixture_root=$1
    fixture_id=$2
    state=$fixture_root/var/db/northstar/installer
    transaction=$state/transactions/$fixture_id
    mkdir -p "$transaction" "$state/archive"
    request=$transaction/request.conf
    cat > "$request" <<EOF
protocol=1
operation=install
target_device=md42
target_mediasize=68719476736
target_sectorsize=512
target_description_sha256=$DESCRIPTION_SHA
layout=gpt-uefi-zfs
pool_name=nstar_0123456789ab
source_manifest_sha256=$MANIFEST_SHA
confirmation=erase-target
plan_status=verified
EOF
    request_sha=$("$BIN/sha256" -q "$request")
    cat > "$transaction/transaction.conf" <<EOF
schema_version=1
transaction_id=$fixture_id
status=staged
request_sha256=$request_sha
target_device=md42
target_mediasize=68719476736
target_sectorsize=512
target_description_sha256=$DESCRIPTION_SHA
layout=gpt-uefi-zfs
pool_name=nstar_0123456789ab
source_manifest_sha256=$MANIFEST_SHA
payload_kind=northstar-rootfs-v1
payload_name=northstar-rootfs.txz
payload_size=$PAYLOAD_SIZE
payload_sha256=$PAYLOAD_SHA
project_commit=0123456789abcdef0123456789abcdef01234567
runtime_manifest_sha256=$RUNTIME_SHA
confirmation=erase-target
execution=guarded-executor-only
recovery=resume-or-abandon-required
EOF
    printf '%s\n' '0001|request-validated' '0002|source-verified' \
      '0003|target-revalidated' '0004|transaction-staged' > "$transaction/journal.log"
    cat > "$state/active.conf" <<EOF
schema_version=1
transaction_id=$fixture_id
status=staged
EOF
    mkdir -p "$fixture_root/etc/northstar"
    cat > "$fixture_root/etc/northstar/installer-execution.conf" <<EOF
schema_version=1
purpose=northstar-installer-media
boot_environment=installer-media
allow_installer_execution=YES
execution_scope=confirmed-whole-disk
source_manifest_sha256=$MANIFEST_SHA
EOF
}

run_executor() {
    env NORTHSTAR_INSTALLER_EXECUTOR_TEST_MODE=1 \
      NORTHSTAR_INSTALLER_EXECUTOR_ROOT="$FAKE_ROOT" \
      NORTHSTAR_INSTALLER_EXECUTOR_MARKER="$FAKE_ROOT/etc/northstar/installer-execution.conf" \
      NORTHSTAR_INSTALLER_EXECUTOR_SOURCE_ROOT="$SOURCE" \
      NORTHSTAR_INSTALLER_EXECUTOR_SOURCE_VERIFY="$BIN/source-verify" \
      NORTHSTAR_INSTALLER_EXECUTOR_GEOM="$BIN/geom" \
      NORTHSTAR_INSTALLER_EXECUTOR_MOUNT="$BIN/mount" \
      NORTHSTAR_INSTALLER_EXECUTOR_ZPOOL="$BIN/zpool" \
      NORTHSTAR_INSTALLER_EXECUTOR_ZFS="$BIN/zfs" \
      NORTHSTAR_INSTALLER_EXECUTOR_SWAPINFO="$BIN/swapinfo" \
      NORTHSTAR_INSTALLER_EXECUTOR_GPART="$BIN/gpart" \
      NORTHSTAR_INSTALLER_EXECUTOR_NEWFS_MSDOS="$BIN/newfs_msdos" \
      NORTHSTAR_INSTALLER_EXECUTOR_MOUNT_MSDOSFS="$BIN/mount_msdosfs" \
      NORTHSTAR_INSTALLER_EXECUTOR_UMOUNT="$BIN/umount" \
      NORTHSTAR_INSTALLER_EXECUTOR_TAR="$BIN/tar" \
      NORTHSTAR_INSTALLER_EXECUTOR_SHA256="$BIN/sha256" \
      NORTHSTAR_INSTALLER_EXECUTOR_INSTALL="$BIN/install" \
      NORTHSTAR_INSTALLER_EXECUTOR_CHOWN="$BIN/chown" \
      NORTHSTAR_INSTALLER_EXECUTOR_CHMOD="$BIN/chmod" \
      NORTHSTAR_INSTALLER_EXECUTOR_CP="$BIN/cp" \
      NORTHSTAR_INSTALLER_EXECUTOR_MV="$BIN/mv" \
      NORTHSTAR_INSTALLER_EXECUTOR_RM="$BIN/rm" \
      NORTHSTAR_INSTALLER_EXECUTOR_INJECT_AFTER="${NORTHSTAR_TEST_INJECT_AFTER:-}" \
      NORTHSTAR_TEST_LOG="$LOG" \
      NORTHSTAR_TEST_PAYLOAD_SIZE="$PAYLOAD_SIZE" \
      NORTHSTAR_TEST_PAYLOAD_SHA="$PAYLOAD_SHA" \
      NORTHSTAR_TEST_RUNTIME_SHA="$RUNTIME_SHA" \
      NORTHSTAR_TEST_RUNTIME_MANIFEST="$RUNTIME_MANIFEST" \
      NORTHSTAR_TEST_SOURCE_FAIL="${NORTHSTAR_TEST_SOURCE_FAIL:-0}" \
      NORTHSTAR_TEST_TARGET_DRIFT="${NORTHSTAR_TEST_TARGET_DRIFT:-0}" \
      sh "$EXECUTOR" "$@"
}

FAKE_ROOT=$TMP_DIR/capability-root
mkdir -p "$FAKE_ROOT"
run_executor --capabilities | grep -Fx 'authorization=installer-media-marker-plus-exact-confirmation' >/dev/null \
    || fail 'capabilities omit the installer-media and confirmation boundary'

FAKE_ROOT=$TMP_DIR/success-root
TRANSACTION_ID=nstar-install-0123456789abcdef-4201
prepare_fixture "$FAKE_ROOT" "$TRANSACTION_ID"
run_executor --preflight "$TRANSACTION_ID" | grep -Fx 'DISK_MUTATION=none' >/dev/null \
    || fail 'non-destructive executor preflight failed'
if grep -Eq '^(gpart|newfs_msdos|zpool (create|set|export)|zfs|mount_msdosfs|umount) ' "$LOG"; then
    fail 'executor preflight invoked a mutation tool'
fi

if run_executor --execute "$TRANSACTION_ID" --confirm-device md41 >/dev/null 2>&1; then
    fail 'incorrect typed target confirmation was accepted'
fi
if grep -Eq '^(gpart|newfs_msdos|zpool (create|set|export)|zfs|mount_msdosfs|umount) ' "$LOG"; then
    fail 'incorrect confirmation invoked a mutation tool'
fi
if NORTHSTAR_TEST_SOURCE_FAIL=1 run_executor --execute "$TRANSACTION_ID" --confirm-device md42 >/dev/null 2>&1; then
    fail 'source revalidation failure was accepted'
fi
if grep -Eq '^(gpart|newfs_msdos|zpool (create|set|export)|zfs|mount_msdosfs|umount) ' "$LOG"; then
    fail 'source failure invoked a mutation tool'
fi
if NORTHSTAR_TEST_TARGET_DRIFT=1 run_executor --execute "$TRANSACTION_ID" --confirm-device md42 >/dev/null 2>&1; then
    fail 'target identity drift was accepted'
fi
if grep -Eq '^(gpart|newfs_msdos|zpool (create|set|export)|zfs|mount_msdosfs|umount) ' "$LOG"; then
    fail 'target drift invoked a mutation tool'
fi
JOURNAL=$FAKE_ROOT/var/db/northstar/installer/transactions/$TRANSACTION_ID/journal.log
cp "$JOURNAL" "$TMP_DIR/journal.clean"
printf '%s\n' '0005|unknown-event' >> "$JOURNAL"
if run_executor --execute "$TRANSACTION_ID" --confirm-device md42 >/dev/null 2>&1; then
    fail 'tampered pre-execution journal was accepted'
fi
mv "$TMP_DIR/journal.clean" "$JOURNAL"
if grep -Eq '^(gpart|newfs_msdos|zpool (create|set|export)|zfs|mount_msdosfs|umount) ' "$LOG"; then
    fail 'journal tampering invoked a mutation tool'
fi

run_executor --execute "$TRANSACTION_ID" --confirm-device md42 > "$TMP_DIR/success.out"
grep -Fx 'INSTALLER_EXECUTION=PASS' "$TMP_DIR/success.out" >/dev/null \
    || fail 'guarded execution did not complete'
ARCHIVE=$FAKE_ROOT/var/db/northstar/installer/archive/$TRANSACTION_ID
[ -d "$ARCHIVE" ] || fail 'completed transaction was not archived'
[ ! -e "$FAKE_ROOT/var/db/northstar/installer/active.conf" ] || fail 'completed transaction remained active'
grep -Fx 'status=completed' "$ARCHIVE/execution.conf" >/dev/null \
    || fail 'completed execution state was not recorded'
grep -F '|transaction-completed' "$ARCHIVE/journal.log" >/dev/null \
    || fail 'completion was not journaled'
run_executor --status "$TRANSACTION_ID" | grep -Fx 'LOCATION=archive' >/dev/null \
    || fail 'archived completion status was not readable'
TRANSACTIONS=$FAKE_ROOT/var/db/northstar/installer/transactions
mv "$ARCHIVE" "$TRANSACTIONS/$TRANSACTION_ID"
cat > "$FAKE_ROOT/var/db/northstar/installer/active.conf" <<EOF
schema_version=1
transaction_id=$TRANSACTION_ID
status=staged
EOF
run_executor --finalize "$TRANSACTION_ID" | grep -Fx 'INSTALLER_FINALIZE=PASS' >/dev/null \
    || fail 'completed transaction could not finish interrupted archive publication'
run_executor --finalize "$TRANSACTION_ID" | grep -Fx 'INSTALLER_FINALIZE=ALREADY' >/dev/null \
    || fail 'completed transaction finalization is not idempotent'

line_gpart=$(grep -n '^gpart create ' "$LOG" | cut -d: -f1)
line_efi=$(grep -n '^newfs_msdos ' "$LOG" | cut -d: -f1)
line_pool=$(grep -n '^zpool create ' "$LOG" | cut -d: -f1)
line_extract=$(grep -n '^tar -xpf ' "$LOG" | cut -d: -f1)
line_boot=$(grep -n '^mount_msdosfs ' "$LOG" | cut -d: -f1)
line_export=$(grep -n '^zpool export ' "$LOG" | cut -d: -f1)
[ "$line_gpart" -lt "$line_efi" ] && [ "$line_efi" -lt "$line_pool" ] \
  && [ "$line_pool" -lt "$line_extract" ] && [ "$line_extract" -lt "$line_boot" ] \
  && [ "$line_boot" -lt "$line_export" ] || fail 'destructive phase ordering is incorrect'

: > "$LOG"
FAKE_ROOT=$TMP_DIR/failure-root
FAILED_ID=nstar-install-fedcba9876543210-4202
prepare_fixture "$FAKE_ROOT" "$FAILED_ID"
if NORTHSTAR_TEST_INJECT_AFTER=datasets-created \
    run_executor --execute "$FAILED_ID" --confirm-device md42 >/dev/null 2>&1; then
    fail 'injected post-mutation failure unexpectedly succeeded'
fi
FAILED_DIR=$FAKE_ROOT/var/db/northstar/installer/transactions/$FAILED_ID
grep -Fx 'status=interrupted' "$FAILED_DIR/execution.conf" >/dev/null \
    || fail 'post-mutation failure was not marked interrupted'
grep -Fx 'recovery=cleanup-and-restart-required' "$FAILED_DIR/execution.conf" >/dev/null \
    || fail 'interrupted transaction lacks explicit recovery guidance'
grep -F '|execution-interrupted' "$FAILED_DIR/journal.log" >/dev/null \
    || fail 'interruption was not journaled'
grep -F 'zpool export nstar_0123456789ab' "$LOG" >/dev/null \
    || fail 'failure cleanup did not export the temporary pool'
if grep -F 'tar -xpf ' "$LOG" >/dev/null; then
    fail 'execution continued into payload extraction after injected failure'
fi
run_executor --status "$FAILED_ID" | grep -Fx 'INSTALLER_EXECUTION_STATUS=interrupted' >/dev/null \
    || fail 'interrupted status was not readable'
run_executor --diagnostics "$FAILED_ID" > "$TMP_DIR/failure-diagnostics.out"
grep -Fx 'NORTHSTAR_INSTALLER_DIAGNOSTICS=1' "$TMP_DIR/failure-diagnostics.out" >/dev/null \
    || fail 'interrupted diagnostics were not available'
grep -Fx 'PRIVATE_DATA=excluded' "$TMP_DIR/failure-diagnostics.out" >/dev/null \
    || fail 'interrupted diagnostics omit the privacy boundary'
grep -Fx 'LAST_PHASE=datasets-created' "$TMP_DIR/failure-diagnostics.out" >/dev/null \
    || fail 'interrupted diagnostics omit the last safe phase'
if grep -Eq 'request\.conf|journal\.log|runtime-manifest|/home/|password' "$TMP_DIR/failure-diagnostics.out"; then
    fail 'interrupted diagnostics exposed private files or unbounded logs'
fi
env NORTHSTAR_INSTALLER_ENGINE_TEST_MODE=1 \
  NORTHSTAR_INSTALLER_ENGINE_ROOT="$FAKE_ROOT" \
  NORTHSTAR_INSTALLER_ENGINE_SHA256="$BIN/sha256" \
  sh "$ENGINE" --status | grep -Fx 'INSTALLER_STATUS=interrupted' >/dev/null \
  || fail 'installer engine did not surface the interrupted executor state'

: > "$LOG"
if run_executor --prepare-retry "$FAILED_ID" --confirm-device md41 >/dev/null 2>&1; then
    fail 'clean retry accepted the wrong typed target'
fi
if [ -s "$LOG" ]; then fail 'wrong retry confirmation invoked a disk tool'; fi
if NORTHSTAR_TEST_TARGET_DRIFT=1 \
    run_executor --prepare-retry "$FAILED_ID" --confirm-device md42 >/dev/null 2>&1; then
    fail 'clean retry accepted target identity drift'
fi
if [ -s "$LOG" ]; then fail 'retry target drift invoked a disk tool'; fi
cp "$FAILED_DIR/journal.log" "$TMP_DIR/interrupted-journal.clean"
printf '%s\n' '9999|execution-interrupted' >> "$FAILED_DIR/journal.log"
if run_executor --prepare-retry "$FAILED_ID" --confirm-device md42 >/dev/null 2>&1; then
    fail 'clean retry accepted a tampered interruption journal'
fi
mv "$TMP_DIR/interrupted-journal.clean" "$FAILED_DIR/journal.log"
if [ -s "$LOG" ]; then fail 'retry journal tampering invoked a disk tool'; fi
run_executor --prepare-retry "$FAILED_ID" --confirm-device md42 > "$TMP_DIR/retry.out"
grep -Fx 'INSTALLER_RETRY=READY' "$TMP_DIR/retry.out" >/dev/null \
    || fail 'interrupted execution was not prepared for a clean retry'
grep -Fx 'DISK_MUTATION=none' "$TMP_DIR/retry.out" >/dev/null \
    || fail 'retry preparation did not report its no-mutation boundary'
[ ! -e "$FAKE_ROOT/var/db/northstar/installer/active.conf" ] \
    || fail 'retry-ready transaction remained active'
[ -d "$FAKE_ROOT/var/db/northstar/installer/archive/$FAILED_ID" ] \
    || fail 'failed attempt evidence was not archived'
grep -F '|retry-prepared' "$FAKE_ROOT/var/db/northstar/installer/archive/$FAILED_ID/journal.log" >/dev/null \
    || fail 'clean retry preparation was not journaled'
if [ -s "$LOG" ]; then fail 'clean retry preparation invoked a disk tool'; fi
env NORTHSTAR_INSTALLER_ENGINE_TEST_MODE=1 \
  NORTHSTAR_INSTALLER_ENGINE_ROOT="$FAKE_ROOT" \
  NORTHSTAR_INSTALLER_ENGINE_SHA256="$BIN/sha256" \
  sh "$ENGINE" --status | grep -Fx 'INSTALLER_STATUS=idle' >/dev/null \
  || fail 'clean retry did not release the protected engine for a new review'

if grep -Eq 'mdconfig|qemu-img|/dev/(ada|da|nda|vtbd)[0-9]+' "$EXECUTOR"; then
    fail 'executor contains a host-disk allocator or hard-coded physical target'
fi
printf '%s\n' 'PASS: guarded installer execution enforces revalidation, ordered recovery diagnostics, and clean retry preparation'
