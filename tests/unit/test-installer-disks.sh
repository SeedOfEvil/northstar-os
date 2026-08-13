#!/bin/sh

set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
HELPER=$ROOT/apps/installer/northstar-installer-disks
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-installer-disks.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM
BIN=$TMP_DIR/bin
mkdir -p "$BIN"
fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }

cat > "$BIN/sysctl" <<'EOF'
#!/bin/sh
printf '%s\n' 'cd0 da0 da1 da2'
EOF
cat > "$BIN/diskinfo" <<'EOF'
#!/bin/sh
case "$2" in
    /dev/cd0) size=0 ;;
    /dev/da0) size=34359738368 ;;
    /dev/da1) size=68719476736 ;;
    /dev/da2) size=8589934592 ;;
    *) exit 1 ;;
esac
printf '%s # mediasize in bytes\n' "$size"
EOF
cat > "$BIN/mount" <<'EOF'
#!/bin/sh
printf '%s\n' '/dev/da0p2 / ufs rw 1 1'
EOF
cat > "$BIN/zpool" <<'EOF'
#!/bin/sh
printf '%s\n' '  /dev/da0p2 ONLINE'
EOF
cat > "$BIN/swapinfo" <<'EOF'
#!/bin/sh
exit 0
EOF
cat > "$BIN/geom" <<'EOF'
#!/bin/sh
case "$3" in
    cd0) size=0 ;;
    da0) size=34359738368 ;;
    da1) size=68719476736 ;;
    da2) size=8589934592 ;;
    *) exit 1 ;;
esac
printf '  Mediasize: %s (fixture)\n  Sectorsize: 512\n  descr: Test disk %s\n' "$size" "$3"
EOF
chmod +x "$BIN"/*

env NORTHSTAR_INSTALLER_SYSCTL="$BIN/sysctl" \
    NORTHSTAR_INSTALLER_DISKINFO="$BIN/diskinfo" \
    NORTHSTAR_INSTALLER_MOUNT="$BIN/mount" \
    NORTHSTAR_INSTALLER_ZPOOL="$BIN/zpool" \
    NORTHSTAR_INSTALLER_SWAPINFO="$BIN/swapinfo" \
    NORTHSTAR_INSTALLER_GEOM="$BIN/geom" \
    sh "$HELPER" > "$TMP_DIR/output"

[ "$(head -n 1 "$TMP_DIR/output")" = protocol=2 ] || fail 'protocol header is missing'
awk -F '\t' '$1 == "da0" && $3 == "512" && $6 == "yes" && $7 == "no" { found=1 } END { exit !found }' "$TMP_DIR/output" \
    || fail 'running system disk was not excluded'
awk -F '\t' '$1 == "da1" && $6 == "no" && $7 == "yes" { found=1 } END { exit !found }' "$TMP_DIR/output" \
    || fail 'eligible unused disk was not exposed'
awk -F '\t' '$1 == "da2" && $7 == "no" && $8 ~ /16 GiB/ { found=1 } END { exit !found }' "$TMP_DIR/output" \
    || fail 'undersized disk was not excluded'
if awk -F '\t' '$1 == "cd0" { found=1 } END { exit !found }' "$TMP_DIR/output"; then
    fail 'zero-capacity optical device was exposed as an installer disk'
fi

if grep -Eq '(^|[[:space:]])(gpart|newfs|dd)([[:space:]]|$)|zpool[[:space:]]+(create|destroy)' "$HELPER"; then
    fail 'read-only discovery helper contains a disk mutation command'
fi
printf '%s\n' 'PASS: installer disk discovery is read-only and excludes active and undersized targets'
