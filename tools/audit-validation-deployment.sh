#!/bin/sh

# Read-only audit for the canonical Northstar VM validation deployment.
# This script never moves, removes, installs, or mutates deployment state.

set -eu

MANIFEST=/usr/local/etc/northstar/validation-deployment.conf
STRICT=0
ALLOW_UNPRIVILEGED_MANIFEST=0
FAILURES=0
WARNINGS=0

usage() {
    cat <<'USAGE'
Usage: audit-validation-deployment.sh [--manifest FILE] [--strict]
       [--allow-unprivileged-manifest]

Verifies the canonical checkout, build, signed repository, package artifact,
active repository configuration, and retention boundaries recorded by a
schema-2 Northstar validation deployment manifest. --strict treats historical
directories outside the declared quarantine/current/previous set as failures.
USAGE
}

pass() { printf 'PASS: %s\n' "$1"; }
warn() {
    WARNINGS=$((WARNINGS + 1))
    printf 'WARN: %s\n' "$1" >&2
}
fail() {
    FAILURES=$((FAILURES + 1))
    printf 'FAIL: %s\n' "$1" >&2
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --manifest) MANIFEST=${2-}; shift 2 ;;
        --strict) STRICT=1; shift ;;
        --allow-unprivileged-manifest) ALLOW_UNPRIVILEGED_MANIFEST=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) printf 'ERROR: unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

[ -f "$MANIFEST" ] && [ ! -L "$MANIFEST" ] \
    || { printf 'ERROR: manifest must be a regular non-symlink file: %s\n' "$MANIFEST" >&2; exit 2; }

if [ "$ALLOW_UNPRIVILEGED_MANIFEST" -ne 1 ]; then
    [ "$(uname -s)" = FreeBSD ] \
        || { printf 'ERROR: privileged manifest ownership checks require FreeBSD\n' >&2; exit 2; }
    owner=$(stat -f '%u' "$MANIFEST")
    mode=$(stat -f '%Lp' "$MANIFEST")
    [ "$owner" = 0 ] || fail "deployment manifest is not root-owned"
    case "$mode" in
        *[2367][0-9]|*[0-9][2367]) fail "deployment manifest is group/world writable ($mode)" ;;
    esac
fi

manifest_count() {
    awk -F= -v key="$1" '$1 == key { count++ } END { print count + 0 }' "$MANIFEST"
}

manifest_value() {
    awk -F= -v key="$1" '$1 == key { sub(/^[^=]*=/, ""); print; exit }' "$MANIFEST"
}

require_value() {
    key=$1
    count=$(manifest_count "$key")
    if [ "$count" -ne 1 ]; then
        fail "manifest key '$key' must appear exactly once"
        printf '%s\n' ''
        return
    fi
    value=$(manifest_value "$key")
    if [ -z "$value" ]; then
        fail "manifest key '$key' is empty"
    fi
    printf '%s\n' "$value"
}

file_sha256() {
    if command -v sha256 >/dev/null 2>&1; then
        sha256 -q "$1"
    else
        sha256sum "$1" | awk '{ print $1 }'
    fi
}

record_value() {
    file=$1
    key=$2
    awk -F= -v key="$key" '$1 == key { sub(/^[^=]*=/, ""); print; exit }' "$file"
}

schema_version=$(require_value schema_version)
canonical_checkout=$(require_value canonical_checkout)
canonical_build=$(require_value canonical_build)
source_branch=$(require_value source_branch)
source_revision=$(require_value source_revision)
development_prefix=$(require_value development_prefix)
repository_revision=$(require_value repository_revision)
repository_path=$(require_value repository_path)
previous_repository_path=$(require_value previous_repository_path)
package_file=$(require_value package_file)
package_sha256=$(require_value package_sha256)
catalogue_sha256=$(require_value catalogue_sha256)
metadata_sha256=$(require_value metadata_sha256)
signature_fingerprint=$(require_value signature_fingerprint)
active_repository_config=$(require_value active_repository_config)
quarantine_root=$(require_value quarantine_root)

[ "$schema_version" = 2 ] || fail "unsupported deployment manifest schema '$schema_version'"
case "$source_branch" in codex/*) : ;; *) fail "source branch is not a codex/* validation branch" ;; esac
printf '%s\n' "$source_revision" | grep -Eq '^[0-9a-f]{40}$' \
    || fail "source revision is not a full lowercase Git commit"
printf '%s\n' "$repository_revision" | grep -Eq '^[0-9]+$' \
    || fail "repository revision is not numeric"

for absolute_path in "$canonical_checkout" "$canonical_build" "$development_prefix" \
    "$repository_path" "$previous_repository_path" "$package_file" \
    "$active_repository_config" "$quarantine_root"; do
    case "$absolute_path" in /*) : ;; *) fail "manifest path is not absolute: $absolute_path" ;; esac
done

if [ -d "$canonical_checkout/.git" ]; then
    actual_revision=$(git -C "$canonical_checkout" rev-parse HEAD 2>/dev/null || true)
    actual_branch=$(git -C "$canonical_checkout" branch --show-current 2>/dev/null || true)
    [ "$actual_revision" = "$source_revision" ] \
        && pass "canonical checkout matches $source_revision" \
        || fail "canonical checkout revision is '$actual_revision'"
    [ "$actual_branch" = "$source_branch" ] \
        && pass "canonical checkout is on $source_branch" \
        || fail "canonical checkout branch is '$actual_branch'"
    [ -z "$(git -C "$canonical_checkout" status --porcelain 2>/dev/null)" ] \
        && pass "canonical checkout is clean" \
        || fail "canonical checkout contains uncommitted changes"
else
    fail "canonical checkout is missing: $canonical_checkout"
fi

[ -d "$canonical_build" ] \
    && pass "canonical build exists" \
    || fail "canonical build is missing: $canonical_build"
[ -x "$development_prefix/bin/northstar-shell" ] \
    && pass "development shell is installed" \
    || fail "development shell is missing from $development_prefix"

publication_record=$repository_path/publication-record.conf
if [ -f "$publication_record" ] && [ ! -L "$publication_record" ]; then
    [ "$(record_value "$publication_record" repository_revision)" = "$repository_revision" ] \
        || fail "publication repository revision does not match the manifest"
    [ "$(record_value "$publication_record" source_revision)" = "$source_revision" ] \
        || fail "publication source revision does not match the manifest"
    [ "$(record_value "$publication_record" catalogue_sha256)" = "$catalogue_sha256" ] \
        || fail "publication catalogue digest does not match the manifest"
    [ "$(record_value "$publication_record" metadata_sha256)" = "$metadata_sha256" ] \
        || fail "publication metadata digest does not match the manifest"
    [ "$(record_value "$publication_record" signature_fingerprint)" = "$signature_fingerprint" ] \
        || fail "publication fingerprint does not match the manifest"
    pass "signed publication record matches the deployment manifest"
else
    fail "publication record is missing: $publication_record"
fi

if [ -f "$repository_path/data.pkg" ]; then
    actual_catalogue_sha256=$(file_sha256 "$repository_path/data.pkg")
    [ "$actual_catalogue_sha256" = "$catalogue_sha256" ] \
        && pass "catalogue digest matches" \
        || fail "catalogue digest mismatch"
else
    fail "repository catalogue is missing"
fi

if [ -f "$repository_path/repository-metadata.json" ]; then
    actual_metadata_sha256=$(file_sha256 "$repository_path/repository-metadata.json")
    [ "$actual_metadata_sha256" = "$metadata_sha256" ] \
        && pass "publication metadata digest matches" \
        || fail "publication metadata digest mismatch"
else
    fail "repository metadata is missing"
fi

if [ -f "$package_file" ]; then
    actual_package_sha256=$(file_sha256 "$package_file")
    [ "$actual_package_sha256" = "$package_sha256" ] \
        && pass "package artifact digest matches" \
        || fail "package artifact digest mismatch"
else
    fail "package artifact is missing: $package_file"
fi

if [ -f "$active_repository_config" ]; then
    grep -F "$repository_path" "$active_repository_config" >/dev/null 2>&1 \
        && pass "active pkg repository points at the canonical publication" \
        || fail "active pkg repository does not point at $repository_path"
else
    fail "active repository configuration is missing"
fi

audit_siblings() {
    parent=$1
    current=$2
    previous=${3-}
    label=$4
    name_prefix=${5-}
    [ -d "$parent" ] || return
    for candidate in "$parent"/*; do
        [ -e "$candidate" ] || continue
        if [ -n "$name_prefix" ]; then
            case "$(basename "$candidate")" in "$name_prefix"*) : ;; *) continue ;; esac
        fi
        [ "$candidate" = "$current" ] && continue
        [ -n "$previous" ] && [ "$candidate" = "$previous" ] && continue
        case "$candidate" in "$quarantine_root"|"$quarantine_root"/*) continue ;; esac
        warn "historical $label remains outside retention boundaries: $candidate"
    done
}

audit_siblings "$(dirname "$canonical_checkout")" "$canonical_checkout" '' checkout northstar
audit_siblings "$(dirname "$canonical_build")" "$canonical_build" '' build
audit_siblings "$(dirname "$repository_path")" "$repository_path" "$previous_repository_path" repository

[ -d "$quarantine_root" ] \
    && pass "quarantine root is present" \
    || warn "quarantine root is not present: $quarantine_root"

if [ "$STRICT" -eq 1 ] && [ "$WARNINGS" -gt 0 ]; then
    FAILURES=$((FAILURES + WARNINGS))
fi

if [ "$FAILURES" -gt 0 ]; then
    printf 'FAIL: deployment audit found %s failure(s) and %s warning(s)\n' "$FAILURES" "$WARNINGS" >&2
    exit 1
fi

printf 'PASS: canonical validation deployment is coherent (%s warning(s))\n' "$WARNINGS"
