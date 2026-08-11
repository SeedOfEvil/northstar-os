#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
AUDITOR=$ROOT/tools/audit-validation-deployment.sh
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-validation-audit.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

CHECKOUT=$TMP_DIR/src/northstar
BUILD=$TMP_DIR/builds/pr74-test
REPOSITORY=$TMP_DIR/validation/development-channel-r76
PREVIOUS_REPOSITORY=$TMP_DIR/validation/development-channel-r75
PREFIX=$TMP_DIR/prefix
QUARANTINE=$TMP_DIR/quarantine
ACTIVE_CONFIG=$TMP_DIR/northstar-development.conf
MANIFEST=$TMP_DIR/validation-deployment.conf

mkdir -p "$CHECKOUT" "$BUILD" "$REPOSITORY" "$PREVIOUS_REPOSITORY" \
    "$PREFIX/bin" "$QUARANTINE"
git -C "$CHECKOUT" init -q
git -C "$CHECKOUT" config user.name Northstar
git -C "$CHECKOUT" config user.email northstar@localhost.invalid
git -C "$CHECKOUT" switch -qc codex/fixture
printf 'fixture\n' > "$CHECKOUT/README"
git -C "$CHECKOUT" add README
git -C "$CHECKOUT" commit -qm fixture
BRANCH=$(git -C "$CHECKOUT" branch --show-current)
REVISION=$(git -C "$CHECKOUT" rev-parse HEAD)

printf '#!/bin/sh\nexit 0\n' > "$PREFIX/bin/northstar-shell"
chmod 0755 "$PREFIX/bin/northstar-shell"
printf 'catalogue\n' > "$REPOSITORY/data.pkg"
printf '{"revision":76}\n' > "$REPOSITORY/repository-metadata.json"
printf 'package\n' > "$REPOSITORY/northstar-0.1.2-amd64.pkg"

digest() {
    if command -v sha256 >/dev/null 2>&1; then
        sha256 -q "$1"
    else
        sha256sum "$1" | awk '{ print $1 }'
    fi
}

CATALOGUE_SHA=$(digest "$REPOSITORY/data.pkg")
METADATA_SHA=$(digest "$REPOSITORY/repository-metadata.json")
PACKAGE_SHA=$(digest "$REPOSITORY/northstar-0.1.2-amd64.pkg")
FINGERPRINT=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef

cat > "$REPOSITORY/publication-record.conf" <<EOF
repository_revision=76
source_revision=$REVISION
catalogue_sha256=$CATALOGUE_SHA
metadata_sha256=$METADATA_SHA
signature_fingerprint=$FINGERPRINT
EOF

cat > "$ACTIVE_CONFIG" <<EOF
northstar-development: { url: "file://$REPOSITORY" }
EOF

cat > "$MANIFEST" <<EOF
schema_version=2
canonical_checkout=$CHECKOUT
canonical_build=$BUILD
source_branch=$BRANCH
source_revision=$REVISION
development_prefix=$PREFIX
repository_revision=76
repository_path=$REPOSITORY
previous_repository_path=$PREVIOUS_REPOSITORY
package_file=$REPOSITORY/northstar-0.1.2-amd64.pkg
package_sha256=$PACKAGE_SHA
catalogue_sha256=$CATALOGUE_SHA
metadata_sha256=$METADATA_SHA
signature_fingerprint=$FINGERPRINT
active_repository_config=$ACTIVE_CONFIG
quarantine_root=$QUARANTINE
EOF

"$AUDITOR" --manifest "$MANIFEST" --allow-unprivileged-manifest --strict >/dev/null

# Unrelated source dependencies are not historical Northstar deployments.
mkdir -p "$TMP_DIR/src/wayfire-nested-v0.10.1"
"$AUDITOR" --manifest "$MANIFEST" --allow-unprivileged-manifest --strict >/dev/null

mkdir -p "$TMP_DIR/builds/orphaned-build"
if "$AUDITOR" --manifest "$MANIFEST" --allow-unprivileged-manifest --strict >/dev/null 2>&1; then
    printf 'FAIL: strict audit accepted an orphaned build\n' >&2
    exit 1
fi

printf 'PASS: validation deployment auditor accepts canonical state and rejects drift\n'
