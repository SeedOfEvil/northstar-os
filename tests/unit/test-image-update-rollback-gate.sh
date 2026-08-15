#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
GATE=$ROOT/image/scripts/validate-image-update-rollback.sh
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-image-update-gate.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

BIN=$TMP_DIR/bin
STATE_DIR=$TMP_DIR/home/.northstar-image-validation
MODEL=$TMP_DIR/model
mkdir -p "$BIN" "$MODEL"
BASELINE_VERSION=0.2.5
CANDIDATE_VERSION=0.2.6
IMAGE_COMMIT=d561e06519cd78aef9e2918fadd22fc3fe0ee4d1
CANDIDATE_SOURCE=1111111111111111111111111111111111111111
REPOSITORY_REVISION=86
CATALOGUE_SHA256=2222222222222222222222222222222222222222222222222222222222222222
SIGNATURE_FINGERPRINT=3333333333333333333333333333333333333333333333333333333333333333
IMAGE_MARKER=$TMP_DIR/image-build.conf
printf 'schema_version=1\nproject_commit=%s\n' "$IMAGE_COMMIT" > "$IMAGE_MARKER"
printf '%s\n' "$BASELINE_VERSION" > "$MODEL/installed-version"
printf '%s\n' "$CANDIDATE_VERSION" > "$MODEL/repository-version"
printf 'northstar-baseline\n' > "$MODEL/active-be"
printf 'northstar-baseline|NR\n' > "$MODEL/be-list"

cat > "$BIN/pkg" <<'EOF'
#!/bin/sh
set -eu
case "$1" in
query)
    cat "$NORTHSTAR_TEST_MODEL/installed-version"
    ;;
rquery)
    cat "$NORTHSTAR_TEST_MODEL/repository-version"
    ;;
*)
    printf 'unexpected fake pkg operation: %s\n' "$1" >&2
    exit 2
    ;;
esac
EOF

cat > "$BIN/bectl" <<'EOF'
#!/bin/sh
set -eu
case "$1" in
list)
    while IFS='|' read -r name flags; do
        printf '%s\t%s\t-\t-\t0B\t-\n' "$name" "$flags"
    done < "$NORTHSTAR_TEST_MODEL/be-list"
    ;;
destroy)
    name=$3
    awk -F'|' -v name="$name" '$1 != name' "$NORTHSTAR_TEST_MODEL/be-list" > "$NORTHSTAR_TEST_MODEL/be-list.new"
    mv "$NORTHSTAR_TEST_MODEL/be-list.new" "$NORTHSTAR_TEST_MODEL/be-list"
    ;;
rename)
    old=$2
    new=$3
    awk -F'|' -v old="$old" -v new="$new" 'BEGIN { OFS="|" } { if ($1 == old) $1=new; print }' \
        "$NORTHSTAR_TEST_MODEL/be-list" > "$NORTHSTAR_TEST_MODEL/be-list.new"
    mv "$NORTHSTAR_TEST_MODEL/be-list.new" "$NORTHSTAR_TEST_MODEL/be-list"
    if [ "$(cat "$NORTHSTAR_TEST_MODEL/active-be")" = "$old" ]; then printf '%s\n' "$new" > "$NORTHSTAR_TEST_MODEL/active-be"; fi
    ;;
*)
    printf 'unexpected fake bectl operation: %s\n' "$1" >&2
    exit 2
    ;;
esac
EOF

cat > "$BIN/transaction" <<'EOF'
#!/bin/sh
set -eu
STATE=$NORTHSTAR_UPDATE_STATE_DIR/update-state.conf
mkdir -p "$NORTHSTAR_UPDATE_STATE_DIR"
case "$1" in
--apply-update)
    rollback=northstar-before-update
    if [ "${NORTHSTAR_UPDATE_PKG:-}" != "" ] && echo "$NORTHSTAR_UPDATE_PKG" | grep -F 'pkg-failure-wrapper' >/dev/null; then
        printf 'northstar-failed|NR\n%s|R\n' "$rollback" > "$NORTHSTAR_TEST_MODEL/be-list"
        printf '%s\n' northstar-failed > "$NORTHSTAR_TEST_MODEL/active-be"
        printf 'protocol=1\nstatus=rollback-scheduled\nboot_environment=%s\nrepository_revision=%s\nsource_revision=%s\ncatalogue_sha256=%s\nsignature_fingerprint=%s\n' \
            "$rollback" "$NORTHSTAR_TEST_REPOSITORY_REVISION" "$NORTHSTAR_TEST_CANDIDATE_SOURCE" \
            "$NORTHSTAR_TEST_CATALOGUE_SHA256" "$NORTHSTAR_TEST_SIGNATURE_FINGERPRINT" > "$STATE"
        exit 70
    fi
    printf '%s\n' "$NORTHSTAR_TEST_CANDIDATE_VERSION" > "$NORTHSTAR_TEST_MODEL/installed-version"
    printf 'northstar-baseline|N\n%s|R\n' "$rollback" > "$NORTHSTAR_TEST_MODEL/be-list"
    printf 'protocol=1\nstatus=updated\nboot_environment=%s\nrepository_revision=%s\nsource_revision=%s\ncatalogue_sha256=%s\nsignature_fingerprint=%s\n' \
        "$rollback" "$NORTHSTAR_TEST_REPOSITORY_REVISION" "$NORTHSTAR_TEST_CANDIDATE_SOURCE" \
        "$NORTHSTAR_TEST_CATALOGUE_SHA256" "$NORTHSTAR_TEST_SIGNATURE_FINGERPRINT" > "$STATE"
    ;;
--rollback)
    ;;
*) exit 2 ;;
esac
EOF

chmod +x "$BIN/pkg" "$BIN/bectl" "$BIN/transaction"

run_gate() {
    NORTHSTAR_IMAGE_VALIDATION_TEST_MODE=1 \
    NORTHSTAR_IMAGE_VALIDATION_STATE_DIR="$STATE_DIR" \
    NORTHSTAR_IMAGE_MARKER="$IMAGE_MARKER" \
    NORTHSTAR_IMAGE_UPDATE_TRANSACTION="$BIN/transaction" \
    NORTHSTAR_IMAGE_PKG="$BIN/pkg" \
    NORTHSTAR_IMAGE_BECTL="$BIN/bectl" \
    NORTHSTAR_UPDATE_STATE_DIR="$TMP_DIR/update-state" \
    NORTHSTAR_TEST_MODEL="$MODEL" \
    NORTHSTAR_TEST_REPOSITORY_REVISION="$REPOSITORY_REVISION" \
    NORTHSTAR_TEST_CANDIDATE_SOURCE="$CANDIDATE_SOURCE" \
    NORTHSTAR_TEST_CATALOGUE_SHA256="$CATALOGUE_SHA256" \
    NORTHSTAR_TEST_SIGNATURE_FINGERPRINT="$SIGNATURE_FINGERPRINT" \
    NORTHSTAR_TEST_CANDIDATE_VERSION="$CANDIDATE_VERSION" \
        sh "$GATE" "$@"
}

UNSAFE_STATE=$TMP_DIR/unsafe-state
if NORTHSTAR_IMAGE_VALIDATION_TEST_MODE=0 \
    NORTHSTAR_IMAGE_VALIDATION_STATE_DIR="$UNSAFE_STATE" \
    NORTHSTAR_IMAGE_UPDATE_TRANSACTION="$BIN/transaction" \
    NORTHSTAR_IMAGE_PKG="$BIN/pkg" NORTHSTAR_IMAGE_BECTL="$BIN/bectl" \
        sh "$GATE" --prepare --baseline-version "$BASELINE_VERSION" --candidate-version "$CANDIDATE_VERSION" \
            --image-commit "$IMAGE_COMMIT" --repository-revision "$REPOSITORY_REVISION" \
            --candidate-source "$CANDIDATE_SOURCE" --catalogue-sha256 "$CATALOGUE_SHA256" \
            --signature-fingerprint "$SIGNATURE_FINGERPRINT" >/dev/null 2>&1; then
    printf 'FAIL: production gate accepted a non-FreeBSD host\n' >&2
    exit 1
fi

run_gate --prepare --baseline-version "$BASELINE_VERSION" --candidate-version "$CANDIDATE_VERSION" \
    --image-commit "$IMAGE_COMMIT" --repository-revision "$REPOSITORY_REVISION" \
    --candidate-source "$CANDIDATE_SOURCE" --catalogue-sha256 "$CATALOGUE_SHA256" \
    --signature-fingerprint "$SIGNATURE_FINGERPRINT" >/dev/null
SENTINEL_SHA=$(sha256sum "$STATE_DIR/home-sentinel.txt" | awk '{ print $1 }')
run_gate --inject-failure >/dev/null

# Simulate reboot into the rollback environment after the injected failure.
printf 'northstar-before-update\n' > "$MODEL/active-be"
printf 'northstar-failed|R\nnorthstar-before-update|N\n' > "$MODEL/be-list"
printf '%s\n' "$BASELINE_VERSION" > "$MODEL/installed-version"
run_gate --verify-failure-recovery >/dev/null
run_gate --normalize-after-failure >/dev/null

run_gate --apply-update >/dev/null
run_gate --schedule-rollback >/dev/null

# Simulate reboot into the explicit rollback environment.
printf 'northstar-before-update\n' > "$MODEL/active-be"
printf 'northstar-baseline|R\nnorthstar-before-update|N\n' > "$MODEL/be-list"
printf '%s\n' "$BASELINE_VERSION" > "$MODEL/installed-version"
run_gate --verify-rollback >/dev/null

grep -Fx 'stage=passed' "$STATE_DIR/state.conf" >/dev/null || {
    printf 'FAIL: gate did not record final pass state\n' >&2
    exit 1
}
[ "$(sha256sum "$STATE_DIR/home-sentinel.txt" | awk '{ print $1 }')" = "$SENTINEL_SHA" ] || {
    printf 'FAIL: home sentinel changed across simulated rollback\n' >&2
    exit 1
}

printf 'PASS: image update gate proves failure recovery, update, explicit rollback, and home preservation\n'
