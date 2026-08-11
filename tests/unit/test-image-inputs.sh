#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
PREPARE=$ROOT/image/scripts/prepare-image-inputs.sh
STATIC_LOCK=$ROOT/image/manifests/northstar-15.1-amd64-qcow2.lock
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-image-inputs.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

ARTIFACTS=$TMP_DIR/artifacts
LOCK=$TMP_DIR/test.lock
PROJECT_ROOT=$TMP_DIR/project
mkdir -p "$ARTIFACTS" "$PROJECT_ROOT"
git -C "$PROJECT_ROOT" init -q
git -C "$PROJECT_ROOT" config user.name Northstar
git -C "$PROJECT_ROOT" config user.email northstar@localhost.invalid
printf 'project fixture\n' > "$PROJECT_ROOT/README"
git -C "$PROJECT_ROOT" add README
git -C "$PROJECT_ROOT" commit -qm fixture
PROJECT_COMMIT=$(git -C "$PROJECT_ROOT" rev-parse HEAD)
printf 'base fixture\n' > "$ARTIFACTS/base.txz"
printf 'kernel fixture\n' > "$ARTIFACTS/kernel.txz"
printf 'northstar fixture\n' > "$ARTIFACTS/northstar-0.1.4-amd64.pkg"

digest() {
    if command -v sha256 >/dev/null 2>&1; then
        sha256 -q "$1"
    else
        sha256sum "$1" | awk '{ print $1 }'
    fi
}

size() { wc -c < "$1" | tr -d ' '; }

write_lock() {
    cat > "$LOCK" <<EOF
SCHEMA_VERSION=1
TARGET_FORMAT=qcow2
FIRMWARE=UEFI
ROOT_FILESYSTEM=ZFS
FREEBSD_RELEASE=15.1-RELEASE
FREEBSD_ARCH=amd64
FREEBSD_ABI=FreeBSD:15:amd64
SOURCE_DATE_EPOCH=1781274780
RELEASE_BASE_URL=https://download.freebsd.org/releases/amd64/amd64/15.1-RELEASE
RELEASE_MANIFEST=MANIFEST
RELEASE_MANIFEST_SHA256=70ba0347054099662f22434797f1f33be289fc868ba58d6069a490b3e395d684
BASE_ARTIFACT=base.txz
BASE_SHA256=$(digest "$ARTIFACTS/base.txz")
BASE_SIZE=$(size "$ARTIFACTS/base.txz")
KERNEL_ARTIFACT=kernel.txz
KERNEL_SHA256=$(digest "$ARTIFACTS/kernel.txz")
KERNEL_SIZE=$(size "$ARTIFACTS/kernel.txz")
NORTHSTAR_PACKAGE=northstar-0.1.4-amd64.pkg
NORTHSTAR_PACKAGE_VERSION=0.1.4
NORTHSTAR_PACKAGE_SHA256=$(digest "$ARTIFACTS/northstar-0.1.4-amd64.pkg")
NORTHSTAR_SOURCE_REVISION=017fc81040bb33879596b6a3dde630212e30524f
NORTHSTAR_REPOSITORY_REVISION=78
NORTHSTAR_CATALOGUE_SHA256=490893129ad74b1a4377fd4da73ae35fe4978a2b10c2ee96eb2959ba255b2142
NORTHSTAR_METADATA_SHA256=5f6f47c82c02974886740dace9fa27198df67978a689700cb159544b68390bda
NORTHSTAR_SIGNATURE_FINGERPRINT=ba1a1b56f3c1bf4cb56c70391bfa73a72e2ed15d45104202f2b3a9b0d6cba2ee
EOF
}

"$PREPARE" --lock "$STATIC_LOCK" --check-lock >/dev/null
write_lock
"$PREPARE" --lock "$LOCK" --check-lock >/dev/null
"$PREPARE" --lock "$LOCK" --artifacts "$ARTIFACTS" \
    --output "$TMP_DIR/prepared" \
    --project-root "$PROJECT_ROOT" --project-commit "$PROJECT_COMMIT" >/dev/null

grep -F 'target_format=qcow2' "$TMP_DIR/prepared/resolved-image-inputs.conf" >/dev/null
grep -F "project_commit=$PROJECT_COMMIT" \
    "$TMP_DIR/prepared/resolved-image-inputs.conf" >/dev/null
[ "$(wc -l < "$TMP_DIR/prepared/artifact-records" | tr -d ' ')" -eq 3 ]

if "$PREPARE" --lock "$LOCK" --artifacts "$ARTIFACTS" \
    --output "$TMP_DIR/prepared" \
    --project-root "$PROJECT_ROOT" --project-commit "$PROJECT_COMMIT" >/dev/null 2>&1; then
    printf 'FAIL: preparation replaced an immutable output\n' >&2
    exit 1
fi

printf 'tampered\n' >> "$ARTIFACTS/base.txz"
if "$PREPARE" --lock "$LOCK" --artifacts "$ARTIFACTS" \
    --output "$TMP_DIR/tampered" \
    --project-root "$PROJECT_ROOT" --project-commit "$PROJECT_COMMIT" >/dev/null 2>&1; then
    printf 'FAIL: preparation accepted a tampered artifact\n' >&2
    exit 1
fi

write_lock
sed 's/NORTHSTAR_PACKAGE_VERSION=0.1.4/NORTHSTAR_PACKAGE_VERSION=UNSET/' \
    "$LOCK" > "$TMP_DIR/unresolved.lock"
if "$PREPARE" --lock "$TMP_DIR/unresolved.lock" --check-lock >/dev/null 2>&1; then
    printf 'FAIL: lock validation accepted an unresolved value\n' >&2
    exit 1
fi

sed 's/BASE_SIZE=[0-9]*/BASE_SIZE=999999/' "$LOCK" > "$TMP_DIR/wrong-size.lock"
if "$PREPARE" --lock "$TMP_DIR/wrong-size.lock" --artifacts "$ARTIFACTS" \
    --output "$TMP_DIR/wrong-size" \
    --project-root "$PROJECT_ROOT" --project-commit "$PROJECT_COMMIT" >/dev/null 2>&1; then
    printf 'FAIL: preparation accepted an incorrectly sized artifact\n' >&2
    exit 1
fi

printf 'UNKNOWN_INPUT=value\n' >> "$TMP_DIR/unknown.lock"
cat "$LOCK" >> "$TMP_DIR/unknown.lock"
if "$PREPARE" --lock "$TMP_DIR/unknown.lock" --check-lock >/dev/null 2>&1; then
    printf 'FAIL: lock validation accepted an unknown key\n' >&2
    exit 1
fi

mv "$ARTIFACTS/kernel.txz" "$ARTIFACTS/kernel.txz.real"
ln -s kernel.txz.real "$ARTIFACTS/kernel.txz"
if [ -L "$ARTIFACTS/kernel.txz" ]; then
    if "$PREPARE" --lock "$LOCK" --artifacts "$ARTIFACTS" \
        --output "$TMP_DIR/symlinked" \
        --project-root "$PROJECT_ROOT" --project-commit "$PROJECT_COMMIT" >/dev/null 2>&1; then
        printf 'FAIL: preparation accepted a symlinked artifact\n' >&2
        exit 1
    fi
else
    printf 'SKIP: filesystem did not create a symbolic-link fixture\n'
fi
rm -f "$ARTIFACTS/kernel.txz"
mv "$ARTIFACTS/kernel.txz.real" "$ARTIFACTS/kernel.txz"

if "$PREPARE" --lock "$LOCK" --artifacts "$ARTIFACTS" \
    --output "$TMP_DIR/wrong-project" --project-root "$PROJECT_ROOT" \
    --project-commit 0123456789abcdef0123456789abcdef01234567 >/dev/null 2>&1; then
    printf 'FAIL: preparation accepted a project commit that does not match HEAD\n' >&2
    exit 1
fi

printf 'dirty\n' >> "$PROJECT_ROOT/README"
if "$PREPARE" --lock "$LOCK" --artifacts "$ARTIFACTS" \
    --output "$TMP_DIR/dirty-project" --project-root "$PROJECT_ROOT" \
    --project-commit "$PROJECT_COMMIT" >/dev/null 2>&1; then
    printf 'FAIL: preparation accepted a dirty project checkout\n' >&2
    exit 1
fi
git -C "$PROJECT_ROOT" checkout -q -- README

cp "$LOCK" "$TMP_DIR/duplicate.lock"
printf 'SCHEMA_VERSION=1\n' >> "$TMP_DIR/duplicate.lock"
if "$PREPARE" --lock "$TMP_DIR/duplicate.lock" --check-lock >/dev/null 2>&1; then
    printf 'FAIL: lock validation accepted a duplicate key\n' >&2
    exit 1
fi

if grep -ERi 'PRIVATE KEY|BEGIN.*PRIVATE' "$TMP_DIR/prepared" >/dev/null 2>&1; then
    printf 'FAIL: prepared inputs contain private key material\n' >&2
    exit 1
fi

printf 'PASS: image inputs are pinned, deterministic, immutable, and tamper-evident\n'
