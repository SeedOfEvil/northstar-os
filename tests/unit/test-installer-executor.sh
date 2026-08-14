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

grep -Fx 'operation=$1' "$EXECUTOR" >/dev/null \
    || fail 'executor does not preserve the requested operation independently'
if grep -Eq '^[[:space:]]*mode=\$1$' "$EXECUTOR"; then
    fail 'executor operation can be overwritten by a production metadata check'
fi

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
if [ "$1" = label ] && [ "$2" = status ]; then
  printf '%s\n' 'Name Status Components'
  if [ "${NORTHSTAR_TEST_MOUNT_ALIAS:-0}" = 1 ]; then
    printf '%s\n' 'msdosfs/NSTAR_EFI N/A md42p1'
  fi
  exit 0
fi
[ "$3" = "${NORTHSTAR_TEST_TARGET:-md42}" ] || exit 1
size=68719476736
[ "${NORTHSTAR_TEST_TARGET_DRIFT:-0}" != 1 ] || size=68719476737
printf '%s\n' "Geom name: $3" "  Mediasize: $size (64G)" \
  '  Sectorsize: 512' '  descr: Northstar Disposable Disk'
EOF
cat > "$BIN/mount" <<'EOF'
#!/bin/sh
if [ "${NORTHSTAR_TEST_MOUNT_ALIAS:-0}" = 1 ]; then
  printf '%s\n' '/dev/msdosfs/NSTAR_EFI /boot/efi msdosfs rw 0 0'
fi
exit 0
EOF
cat > "$BIN/swapinfo" <<'EOF'
#!/bin/sh
exit 0
EOF
cat > "$BIN/gpart" <<'EOF'
#!/bin/sh
printf 'gpart %s\n' "$*" >> "$NORTHSTAR_TEST_LOG"
if [ "$1" = show ]; then
  if [ "${NORTHSTAR_TEST_EXISTING_ZFS:-0}" = 1 ]; then
    printf '%s\n' \
      '=>       40  134217648  md42  GPT  (64G)' \
      '         40     532480      1  efi  (260M)' \
      '     534528  133683152      2  freebsd-zfs  (64G)'
    exit 0
  fi
  exit 1
fi
flags=$(cat "$NORTHSTAR_TEST_GEOM_FLAGS")
[ $((flags & 16)) -eq 16 ] || exit 77
[ "${NORTHSTAR_TEST_REQUIRE_LABELCLEAR:-0}" != 1 ] || [ "$1" != destroy ] || {
  grep -Fx 'zpool labelclear -f /dev/md42p2' "$NORTHSTAR_TEST_LOG" >/dev/null \
    && grep -Fx 'zpool labelclear -f /dev/md42' "$NORTHSTAR_TEST_LOG" >/dev/null
}
[ "${NORTHSTAR_TEST_GPART_FAIL:-0}" != 1 ] || exit 69
# Real FreeBSD gpart prints created provider names on successful operations.
# The executor must keep this command output out of its completion protocol.
printf '%s\n' "${NORTHSTAR_TEST_TARGET:-md42}"
exit 0
EOF
cat > "$BIN/sysctl" <<'EOF'
#!/bin/sh
if [ "$1" = -n ] && [ "$2" = kern.geom.debugflags ]; then
  cat "$NORTHSTAR_TEST_GEOM_FLAGS"
  exit 0
fi
case "$1" in
  kern.geom.debugflags=*)
    value=${1#*=}
    printf '%s\n' "$value" > "$NORTHSTAR_TEST_GEOM_FLAGS"
    printf 'sysctl %s\n' "$1" >> "$NORTHSTAR_TEST_LOG"
    exit 0 ;;
esac
exit 64
EOF
cat > "$BIN/zpool" <<'EOF'
#!/bin/sh
if [ "$1" = status ]; then exit 0; fi
printf 'zpool %s\n' "$*" >> "$NORTHSTAR_TEST_LOG"
if [ "$1" = labelclear ]; then
  [ "${NORTHSTAR_TEST_LABELCLEAR_FAIL:-0}" != 1 ] || exit 69
  provider=$3
  grep -Fvx "$provider" "$NORTHSTAR_TEST_ZFS_LABELS" > "$NORTHSTAR_TEST_ZFS_LABELS.new" || true
  mv "$NORTHSTAR_TEST_ZFS_LABELS.new" "$NORTHSTAR_TEST_ZFS_LABELS"
fi
exit 0
EOF
cat > "$BIN/zdb" <<'EOF'
#!/bin/sh
[ "$1" = -l ] && [ "$#" -eq 2 ] || exit 64
grep -Fx "$2" "$NORTHSTAR_TEST_ZFS_LABELS" >/dev/null 2>&1
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
    if [ "${NORTHSTAR_TEST_LARGE_LIST:-0}" = 1 ]; then
      awk 'BEGIN { for (i=0; i<140000; i++) printf "./usr/share/northstar/fixtures/entry-%06d-bounded-payload-record.txt\n", i }'
    else
      printf '%s\n' boot/loader.efi boot/loader.conf etc/fstab etc/rc.conf \
        var/db/northstar/runtime-manifest.conf \
        usr/local/bin/northstar-session usr/local/bin/northstar-shell \
        usr/local/share/xsessions/northstar-image-proxmox.desktop
    fi ;;
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
    printf '%s\n' 'northstar-setup:*:1001:1001:Northstar Setup:/home/northstar-setup:/bin/sh' > "$destination/etc/passwd"
    printf '%s\n' 'zfs_enable="YES"' > "$destination/etc/rc.conf"
    cp "$NORTHSTAR_TEST_RUNTIME_MANIFEST" "$destination/var/db/northstar/runtime-manifest.conf"
    printf '%s\n' '#!/bin/sh' 'exit 0' > "$destination/usr/local/bin/northstar-session"
    printf '%s\n' '#!/bin/sh' 'exit 0' > "$destination/usr/local/bin/northstar-shell"
    chmod +x "$destination/usr/local/bin/northstar-session" "$destination/usr/local/bin/northstar-shell"
    printf '%s\n' desktop > "$destination/usr/local/share/xsessions/northstar-image-proxmox.desktop" ;;
  *) exit 64 ;;
esac
EOF
cat > "$BIN/install" <<'EOF'
#!/bin/sh
printf 'install %s\n' "$*" >> "$NORTHSTAR_TEST_LOG"
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
GEOM_FLAGS=$TMP_DIR/geom-debugflags
printf '%s\n' 0 > "$GEOM_FLAGS"
ZFS_LABELS=$TMP_DIR/zfs-labels
: > "$ZFS_LABELS"

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
      NORTHSTAR_INSTALLER_EXECUTOR_ZDB="$BIN/zdb" \
      NORTHSTAR_INSTALLER_EXECUTOR_SWAPINFO="$BIN/swapinfo" \
      NORTHSTAR_INSTALLER_EXECUTOR_SYSCTL="$BIN/sysctl" \
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
      NORTHSTAR_TEST_GEOM_FLAGS="$GEOM_FLAGS" \
      NORTHSTAR_TEST_GPART_FAIL="${NORTHSTAR_TEST_GPART_FAIL:-0}" \
      NORTHSTAR_TEST_EXISTING_ZFS="${NORTHSTAR_TEST_EXISTING_ZFS:-0}" \
      NORTHSTAR_TEST_REQUIRE_LABELCLEAR="${NORTHSTAR_TEST_REQUIRE_LABELCLEAR:-0}" \
      NORTHSTAR_TEST_LABELCLEAR_FAIL="${NORTHSTAR_TEST_LABELCLEAR_FAIL:-0}" \
      NORTHSTAR_TEST_MOUNT_ALIAS="${NORTHSTAR_TEST_MOUNT_ALIAS:-0}" \
      NORTHSTAR_TEST_ZFS_LABELS="$ZFS_LABELS" \
      NORTHSTAR_TEST_PAYLOAD_SIZE="$PAYLOAD_SIZE" \
      NORTHSTAR_TEST_PAYLOAD_SHA="$PAYLOAD_SHA" \
      NORTHSTAR_TEST_RUNTIME_SHA="$RUNTIME_SHA" \
      NORTHSTAR_TEST_RUNTIME_MANIFEST="$RUNTIME_MANIFEST" \
      NORTHSTAR_TEST_SOURCE_FAIL="${NORTHSTAR_TEST_SOURCE_FAIL:-0}" \
      NORTHSTAR_TEST_TARGET_DRIFT="${NORTHSTAR_TEST_TARGET_DRIFT:-0}" \
      NORTHSTAR_TEST_LARGE_LIST="${NORTHSTAR_TEST_LARGE_LIST:-0}" \
      sh "$EXECUTOR" "$@"
}

FAKE_ROOT=$TMP_DIR/capability-root
mkdir -p "$FAKE_ROOT"
run_executor --capabilities | grep -Fx 'authorization=installer-media-marker-plus-exact-confirmation' >/dev/null \
    || fail 'capabilities omit the installer-media and confirmation boundary'

FAKE_ROOT=$TMP_DIR/alias-mounted-root
ALIAS_MOUNTED_ID=nstar-install-0011223344556677-4200
prepare_fixture "$FAKE_ROOT" "$ALIAS_MOUNTED_ID"
if NORTHSTAR_TEST_MOUNT_ALIAS=1 \
    run_executor --preflight "$ALIAS_MOUNTED_ID" >/dev/null 2>&1; then
    fail 'target mounted through a GEOM label alias was accepted'
fi
[ ! -s "$LOG" ] || fail 'alias-mounted target preflight invoked a mutation tool'

SUCCESS_ROOT=$TMP_DIR/success-root
FAKE_ROOT=$SUCCESS_ROOT
TRANSACTION_ID=nstar-install-0123456789abcdef-4201
prepare_fixture "$FAKE_ROOT" "$TRANSACTION_ID"
run_executor --preflight "$TRANSACTION_ID" | grep -Fx 'DISK_MUTATION=none' >/dev/null \
    || fail 'non-destructive executor preflight failed'
NORTHSTAR_TEST_LARGE_LIST=1 run_executor --preflight "$TRANSACTION_ID" \
    | grep -Fx 'DISK_MUTATION=none' >/dev/null \
    || fail 'realistic bounded release payload listing was rejected'
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

: > "$LOG"
FAKE_ROOT=$TMP_DIR/rank1-failure-root
RANK1_FAILED_ID=nstar-install-aabbccddeeff0011-4203
prepare_fixture "$FAKE_ROOT" "$RANK1_FAILED_ID"
printf '%s\n' 0 > "$GEOM_FLAGS"
if NORTHSTAR_TEST_GPART_FAIL=1 \
    run_executor --execute "$RANK1_FAILED_ID" --confirm-device md42 >/dev/null 2>&1; then
    fail 'injected GPT replacement failure unexpectedly succeeded'
fi
[ "$(cat "$GEOM_FLAGS")" = 0 ] \
    || fail 'GPT replacement failure did not restore GEOM write protection'
grep -F 'sysctl kern.geom.debugflags=16' "$LOG" >/dev/null \
    || fail 'GPT replacement failure did not enter the bounded GEOM window'
grep -F 'sysctl kern.geom.debugflags=0' "$LOG" >/dev/null \
    || fail 'GPT replacement failure cleanup did not restore GEOM protection'
grep -Fx 'last_phase=mutation-started' \
    "$FAKE_ROOT/var/db/northstar/installer/transactions/$RANK1_FAILED_ID/execution.conf" >/dev/null \
    || fail 'GPT replacement failure recorded an incorrect last phase'

: > "$LOG"
FAKE_ROOT=$TMP_DIR/existing-zfs-root
EXISTING_ZFS_ID=nstar-install-1122334455667788-4204
prepare_fixture "$FAKE_ROOT" "$EXISTING_ZFS_ID"
printf '%s\n' 0 > "$GEOM_FLAGS"
printf '%s\n' /dev/md42p2 /dev/md42 > "$ZFS_LABELS"
NORTHSTAR_TEST_EXISTING_ZFS=1 NORTHSTAR_TEST_REQUIRE_LABELCLEAR=1 \
    run_executor --execute "$EXISTING_ZFS_ID" --confirm-device md42 \
    > "$TMP_DIR/existing-zfs.out"
grep -Fx 'INSTALLER_EXECUTION=PASS' "$TMP_DIR/existing-zfs.out" >/dev/null \
    || fail 'whole-disk reinstall over existing ZFS did not complete'
grep -Fx 'zpool labelclear -f /dev/md42p2' "$LOG" >/dev/null \
    || fail 'existing partition-level ZFS label was not cleared'
grep -Fx 'zpool labelclear -f /dev/md42' "$LOG" >/dev/null \
    || fail 'existing whole-disk ZFS label was not cleared'
line_partition_label=$(grep -n '^zpool labelclear -f /dev/md42p2$' "$LOG" | cut -d: -f1)
line_disk_label=$(grep -n '^zpool labelclear -f /dev/md42$' "$LOG" | cut -d: -f1)
line_destroy=$(grep -n '^gpart destroy -F md42$' "$LOG" | cut -d: -f1)
line_create=$(grep -n '^gpart create -s gpt md42$' "$LOG" | cut -d: -f1)
[ "$line_partition_label" -lt "$line_destroy" ] \
    && [ "$line_disk_label" -lt "$line_destroy" ] \
    && [ "$line_destroy" -lt "$line_create" ] \
    || fail 'existing ZFS labels were not cleared before GPT replacement'
[ "$(cat "$GEOM_FLAGS")" = 0 ] \
    || fail 'existing-ZFS reinstall did not restore GEOM write protection'

: > "$LOG"
: > "$ZFS_LABELS"
FAKE_ROOT=$TMP_DIR/already-cleared-zfs-root
ALREADY_CLEARED_ID=nstar-install-2233445566778899-4205
prepare_fixture "$FAKE_ROOT" "$ALREADY_CLEARED_ID"
printf '%s\n' 0 > "$GEOM_FLAGS"
NORTHSTAR_TEST_EXISTING_ZFS=1 \
    run_executor --execute "$ALREADY_CLEARED_ID" --confirm-device md42 \
    > "$TMP_DIR/already-cleared-zfs.out"
grep -Fx 'INSTALLER_EXECUTION=PASS' "$TMP_DIR/already-cleared-zfs.out" >/dev/null \
    || fail 'retry over an already-cleared ZFS partition did not complete'
if grep -F 'zpool labelclear ' "$LOG" >/dev/null; then
    fail 'already-cleared ZFS provider was passed to labelclear again'
fi
[ "$(cat "$GEOM_FLAGS")" = 0 ] \
    || fail 'already-cleared retry did not restore GEOM write protection'

: > "$LOG"
FAKE_ROOT=$SUCCESS_ROOT
TRANSACTION_ID=nstar-install-0123456789abcdef-4201
run_executor --execute "$TRANSACTION_ID" --confirm-device md42 > "$TMP_DIR/success.out"
grep -Fx 'INSTALLER_EXECUTION=PASS' "$TMP_DIR/success.out" >/dev/null \
    || fail 'guarded execution did not complete'
[ "$(wc -l < "$TMP_DIR/success.out" | tr -d ' ')" -eq 5 ] \
    || fail 'completion protocol contains output from a mutation command'
if grep -Ev '^[A-Z][A-Z0-9_]*=' "$TMP_DIR/success.out" >/dev/null; then
    fail 'completion protocol contains a malformed record'
fi
grep -F "/var/run/northstar-installer/execution/$TRANSACTION_ID/dev" "$LOG" >/dev/null \
    || fail 'installed root does not recreate the required devfs mountpoint'
grep -F -- "-m 1777" "$LOG" | grep -F "/tmp" >/dev/null \
    || fail 'installed root does not restore sticky world-writable tmp permissions'
grep -F -- "-o 1001 -g 1001 -m 0755" "$LOG" \
    | grep -F "/home/northstar-setup" >/dev/null \
    || fail 'installed root does not recreate the first-boot home directory'
[ "$(cat "$GEOM_FLAGS")" = 0 ] || fail 'successful execution did not restore GEOM write protection'
grep -F 'sysctl kern.geom.debugflags=16' "$LOG" >/dev/null \
    || fail 'execution did not narrowly enable Rank-1 target replacement'
grep -F 'sysctl kern.geom.debugflags=0' "$LOG" >/dev/null \
    || fail 'execution did not restore the original GEOM write protection'
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
grep -F 'could not clear an existing target ZFS partition label' "$EXECUTOR" >/dev/null \
    || fail 'executor still hides a failed partition ZFS label clear'
printf '%s\n' 'PASS: guarded installer execution enforces revalidation, ordered recovery diagnostics, and clean retry preparation'
