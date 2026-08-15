#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
ASSEMBLER=$ROOT/image/scripts/assemble-qcow2-image.sh
EXECUTOR=$ROOT/apps/installer/northstar-installer-executor
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
grep -F '"/.northstar-package-index/$name-$version.pkg"' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: assembler omits the canonical offline package index\n' >&2
    exit 1
}
grep -F 'offline package bootstrap did not converge after three passes' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: assembler omits bounded offline package convergence\n' >&2
    exit 1
}
grep -F 'package_source=/.northstar-primary/$filename' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: assembler does not replace a stale runtime Northstar package with the locked primary artifact\n' >&2
    exit 1
}
grep -F 'locked primary Northstar package was not installed exactly' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: assembler does not verify the exact installed Northstar version\n' >&2
    exit 1
}
grep -F "pkg query -F \"\$primary_northstar_path\" '%v'" "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: assembler does not verify primary package metadata before mutation\n' >&2
    exit 1
}
grep -F 'installed installer executor differs from the locked Northstar package' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: assembler does not verify the installed executor payload\n' >&2
    exit 1
}
grep -F 'pw -R "$MOUNT_ROOT" usermod northstar -w none' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: development autologin does not unlock its local image account\n' >&2
    exit 1
}
grep -F 'development_passwordless_local_account=$DEVELOPMENT_AUTOLOGIN' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: image provenance omits the development local-account policy\n' >&2
    exit 1
}
grep -F 'runtime-manifest.conf' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: assembler omits the installed runtime manifest\n' >&2
    exit 1
}
grep -F 'northstar-rootfs-v1-$(printf' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: assembler omits the installer rootfs payload\n' >&2
    exit 1
}
for field in installer_payload installer_payload_sha256 installer_payload_size; do
    grep -F "${field}=" "$ASSEMBLER" >/dev/null || {
        printf 'FAIL: image provenance omits %s\n' "$field" >&2
        exit 1
    }
done
grep -F "useradd northstar-setup -u 1001" "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: production image does not create the bounded setup identity\n' >&2
    exit 1
}
grep -F 'Session=northstar-first-boot.desktop' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: production image does not enter the first-boot session\n' >&2
    exit 1
}
grep -F 'status=pending' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: production image omits protected pending state\n' >&2
    exit 1
}
grep -F 'useradd northstar-setup -u 1001' "$ASSEMBLER" >/dev/null \
    && grep -F 'home/northstar-setup' "$ROOT/apps/installer/northstar-installer-executor" >/dev/null || {
    printf 'FAIL: production installer does not recreate the separate first-boot home\n' >&2
    exit 1
}
grep -F 'runtime bundle omits production first-boot component' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: production image does not require packaged first-boot components\n' >&2
    exit 1
}
grep -F 'xauth -f "$candidate" list "$DISPLAY"' \
    "$ROOT/apps/first-boot/northstar-first-boot-session" >/dev/null || {
    printf 'FAIL: first-boot session omits FreeBSD SDDM Xauthority recovery\n' >&2
    exit 1
}
if grep -F 'zfs create -o mountpoint=/var "$POOL/var"' "$ASSEMBLER" >/dev/null; then
    printf 'FAIL: assembler places the package database outside boot environments\n' >&2
    exit 1
fi
grep -F "'/var must belong to the root boot environment so package state rolls back'" \
    "$ROOT/image/scripts/validate-image-update-rollback.sh" >/dev/null || {
    printf 'FAIL: installed-image gate does not reject shared package state\n' >&2
    exit 1
}
grep -F 'northstar-image-proxmox.desktop' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: image omits its explicit packaged-runtime session entry\n' >&2
    exit 1
}
grep -F 'rm -f "$MOUNT_ROOT/usr/local/share/xsessions/northstar-proxmox.desktop"' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: production image retains the ambiguous development fallback session\n' >&2
    exit 1
}
grep -F 'usr/local/share/xsessions/northstar-image-proxmox.desktop' "$EXECUTOR" >/dev/null || {
    printf 'FAIL: installer does not validate the image-managed runtime session entry\n' >&2
    exit 1
}
grep -F 'image/session/northstar-image-session-x11' "$ASSEMBLER" >/dev/null || {
    printf 'FAIL: image does not install its SDDM authorization launcher\n' >&2
    exit 1
}
IMAGE_SESSION=$ROOT/image/session/northstar-image-session-x11
grep -Fx 'xf86-input-libinput' "$ROOT/image/manifests/northstar-runtime-roots.txt" >/dev/null || {
    printf 'FAIL: image runtime roots omit the Proxmox Xorg input driver\n' >&2
    exit 1
}
grep -F 'NORTHSTAR_SESSION_BIN=/usr/local/bin/northstar-session' "$IMAGE_SESSION" >/dev/null || {
    printf 'FAIL: image session does not bind the packaged supervisor path\n' >&2
    exit 1
}
grep -F 'NORTHSTAR_SESSION_SHELL=/usr/local/bin/northstar-shell' "$IMAGE_SESSION" >/dev/null || {
    printf 'FAIL: image session does not bind the packaged shell path\n' >&2
    exit 1
}
grep -F 'usr/local/libexec/northstar-wayfire-nested/bin/wayfire' \
    "$ROOT/src/session/northstar-session-x11" >/dev/null || {
    printf 'FAIL: generic fallback cannot resolve the packaged image Wayfire runtime\n' >&2
    exit 1
}
grep -F 'xauth -f "$candidate" list "$DISPLAY"' "$IMAGE_SESSION" >/dev/null || {
    printf 'FAIL: image session does not validate the SDDM Xauthority cookie\n' >&2
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

printf 'NORTHSTAR_PACKAGE=northstar-0.1.4-amd64.pkg\n' > "$TMP_DIR/resolved/input.lock"
for name in base.txz kernel.txz northstar-0.1.4-amd64.pkg; do
    printf 'artifact %s\n' "$name" > "$TMP_DIR/artifacts/$name"
    printf '%s|%s|%s\n' "$name" "$(digest "$TMP_DIR/artifacts/$name")" \
        "$(size "$TMP_DIR/artifacts/$name")"
done | sort > "$TMP_DIR/resolved/artifact-records"
tr -d '\r' < "$TMP_DIR/resolved/artifact-records" \
    > "$TMP_DIR/resolved/artifact-records.normalized"
mv "$TMP_DIR/resolved/artifact-records.normalized" \
    "$TMP_DIR/resolved/artifact-records"

printf 'stale northstar package\n' > "$TMP_DIR/runtime/packages/northstar-0.1.3-amd64.pkg"
printf 'compat package\n' > "$TMP_DIR/runtime/packages/northstar-wayfire-nested-0.10.1.746bc7e.pkg"
{
    printf '%s|%s|%s|%s|%s|%s\n' \
        northstar-0.1.3-amd64.pkg \
        "$(digest "$TMP_DIR/runtime/packages/northstar-0.1.3-amd64.pkg")" \
        "$(size "$TMP_DIR/runtime/packages/northstar-0.1.3-amd64.pkg")" \
        northstar 0.1.3 x11/northstar
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
northstar_package_version=0.1.4
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

printf 'tampered\n' >> "$TMP_DIR/runtime/packages/northstar-0.1.3-amd64.pkg"
if "$ASSEMBLER" --preflight \
    --resolved-inputs "$TMP_DIR/resolved" --artifacts "$TMP_DIR/artifacts" \
    --runtime-bundle "$TMP_DIR/runtime" --output "$TMP_DIR/tampered" \
    --project-root "$TMP_DIR/project" --project-commit "$PROJECT_COMMIT" >/dev/null 2>&1; then
    printf 'FAIL: assembler preflight accepted a tampered runtime package\n' >&2
    exit 1
fi

printf 'PASS: QCOW2 assembler preflight is pinned, offline, and tamper-evident\n'
