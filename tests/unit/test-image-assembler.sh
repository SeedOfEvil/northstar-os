#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
ASSEMBLER=$ROOT/image/scripts/assemble-qcow2-image.sh
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-image-assembler.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM
mkdir -p "$TMP_DIR/project" "$TMP_DIR/resolved" "$TMP_DIR/artifacts" \
    "$TMP_DIR/runtime/packages"
git -C "$TMP_DIR/project" init -q
git -C "$TMP_DIR/project" config user.name Northstar
git -C "$TMP_DIR/project" config user.email northstar@localhost.invalid
printf 'fixture\n' > "$TMP_DIR/project/README"
git -C "$TMP_DIR/project" add README
git -C "$TMP_DIR/project" commit -qm fixture
PROJECT_COMMIT=$(git -C "$TMP_DIR/project" rev-parse HEAD)

grep -F 'newfs_msdos -F 32 -c 1 -L NSTAR_EFI' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: assembler does not pin valid FAT32 geometry for the EFI partition\n' >&2
    exit 1
}
grep -F 'chroot "$MOUNT_ROOT" /tmp/northstar-pkg-static' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: assembler does not install packages inside the image root\n' >&2
    exit 1
}
if grep -F 'pkg -r "$MOUNT_ROOT"' "$ASSEMBLER" >/dev/null; then
    printf 'FAIL: assembler still uses host-rooted pkg installation\n' >&2
    exit 1
fi

digest() {
    if command -v sha256 >/dev/null 2>&1; then sha256 -q "$1"; else sha256sum "$1" | awk '{ print $1 }'; fi
}
size() { wc -c < "$1" | tr -d ' '; }

printf 'lock fixture\n' > "$TMP_DIR/resolved/input.lock"
for name in base.txz kernel.txz northstar-0.1.4-amd64.pkg; do
    printf 'artifact %s\n' "$name" > "$TMP_DIR/artifacts/$name"
    printf '%s|%s|%s\n' "$name" "$(digest "$TMP_DIR/artifacts/$name")" \
        "$(size "$TMP_DIR/artifacts/$name")"
done | sort > "$TMP_DIR/resolved/artifact-records"
tr -d '\r' < "$TMP_DIR/resolved/artifact-records" \
    > "$TMP_DIR/resolved/artifact-records.normalized"
mv "$TMP_DIR/resolved/artifact-records.normalized" \
    "$TMP_DIR/resolved/artifact-records"

printf 'northstar package\n' > "$TMP_DIR/runtime/packages/northstar-0.1.4-amd64.pkg"
printf 'compat package\n' > "$TMP_DIR/runtime/packages/northstar-wayfire-nested-0.10.1.746bc7e.pkg"
{
    printf '%s|%s|%s|%s|%s|%s\n' \
        northstar-0.1.4-amd64.pkg \
        "$(digest "$TMP_DIR/runtime/packages/northstar-0.1.4-amd64.pkg")" \
        "$(size "$TMP_DIR/runtime/packages/northstar-0.1.4-amd64.pkg")" \
        northstar 0.1.4 x11/northstar
    printf '%s|%s|%s|%s|%s|%s\n' \
        northstar-wayfire-nested-0.10.1.746bc7e.pkg \
        "$(digest "$TMP_DIR/runtime/packages/northstar-wayfire-nested-0.10.1.746bc7e.pkg")" \
        "$(size "$TMP_DIR/runtime/packages/northstar-wayfire-nested-0.10.1.746bc7e.pkg")" \
        northstar-wayfire-nested 0.10.1.746bc7e x11-wm/northstar-wayfire-nested
} > "$TMP_DIR/runtime/runtime-package-records"

cat > "$TMP_DIR/resolved/resolved-image-inputs.conf" <<EOF
schema_version=1
target_format=qcow2
project_commit=$PROJECT_COMMIT
source_date_epoch=1781274780
input_lock_sha256=$(digest "$TMP_DIR/resolved/input.lock")
artifact_records_sha256=$(digest "$TMP_DIR/resolved/artifact-records")
freebsd_release=15.1-RELEASE
freebsd_arch=amd64
EOF
cat > "$TMP_DIR/runtime/runtime-bundle.conf" <<EOF
schema_version=1
freebsd_abi=FreeBSD:15:amd64
source_date_epoch=1781274780
package_count=2
runtime_package_records_sha256=$(digest "$TMP_DIR/runtime/runtime-package-records")
EOF

"$ASSEMBLER" --preflight \
    --resolved-inputs "$TMP_DIR/resolved" --artifacts "$TMP_DIR/artifacts" \
    --runtime-bundle "$TMP_DIR/runtime" --output "$TMP_DIR/output" \
    --project-root "$TMP_DIR/project" --project-commit "$PROJECT_COMMIT" >/dev/null

printf 'tampered\n' >> "$TMP_DIR/runtime/packages/northstar-0.1.4-amd64.pkg"
if "$ASSEMBLER" --preflight \
    --resolved-inputs "$TMP_DIR/resolved" --artifacts "$TMP_DIR/artifacts" \
    --runtime-bundle "$TMP_DIR/runtime" --output "$TMP_DIR/tampered" \
    --project-root "$TMP_DIR/project" --project-commit "$PROJECT_COMMIT" >/dev/null 2>&1; then
    printf 'FAIL: assembler preflight accepted a tampered runtime package\n' >&2
    exit 1
fi

printf 'PASS: QCOW2 assembler preflight is pinned, offline, and tamper-evident\n'
