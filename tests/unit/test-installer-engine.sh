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
if command -v sha256sum >/dev/null 2>&1; then sha256sum | awk '{ print $1 }'; else /sbin/sha256 -q; fi
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
        NORTHSTAR_TEST_ACTIVE="${NORTHSTAR_TEST_ACTIVE:-0}" \
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

run_engine --stage "$REQUEST" > "$TMP_DIR/output"
grep -Fx 'INSTALLER_PREFLIGHT=PASS' "$TMP_DIR/output" >/dev/null \
    || fail 'verified target did not pass independent preflight'
STATE=$FAKE_ROOT/var/db/northstar/installer/transaction.request
[ -f "$STATE" ] || fail 'root-owned transaction state was not staged'
grep -Fx 'status=preflight-passed' "$STATE" >/dev/null || fail 'staged status is wrong'
grep -Fx 'execution=disabled' "$STATE" >/dev/null || fail 'staged state does not disable execution'
grep -Fx 'target_device=da1' "$STATE" >/dev/null || fail 'staged target is wrong'
if run_engine --stage "$REQUEST" >/dev/null 2>&1; then
    fail 'existing unresolved installer state was overwritten'
fi

if grep -Eq '(^|[[:space:]])(gpart|newfs|dd)([[:space:]]|$)|zpool[[:space:]]+(create|destroy)' "$ENGINE"; then
    fail 'engine foundation contains a disk mutation command'
fi
printf '%s\n' 'PASS: installer engine independently revalidates and stages a no-mutation transaction'
