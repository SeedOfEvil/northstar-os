#!/bin/sh

# Fast, non-mutating checks that are safe for untrusted pull-request source.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)

cd "$REPO_ROOT"

die() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

command -v git >/dev/null 2>&1 || die 'git is required'
command -v sh >/dev/null 2>&1 || die 'sh is required'

[ -f .github/workflows/ci.yml ] || die 'the required CI workflow is missing'

sh "$REPO_ROOT/tools/ci/version-contracts.sh"

git ls-files '*.sh' | while IFS= read -r path; do
    [ -n "$path" ] || continue
    [ -f "$path" ] || die "tracked shell file is missing: $path"
    sh -n "$path" || die "shell syntax check failed: $path"
done

# Workflow actions execute third-party code. Require every remote action to be
# bound to a full immutable commit rather than a mutable tag or branch.
workflow_uses=$(git grep -n -E '^[[:space:]]*uses:[[:space:]]*' -- '.github/workflows/*.yml' '.github/workflows/*.yaml' 2>/dev/null || true)
printf '%s\n' "$workflow_uses" | while IFS=: read -r path line_number declaration; do
    [ -n "$path" ] || continue
    action=$(printf '%s\n' "$declaration" \
        | sed -E 's/^[[:space:]]*uses:[[:space:]]*//; s/[[:space:]]+#.*$//' \
        | tr -d "\"'")
    case "$action" in
        ./*|docker://*) continue ;;
    esac
    revision=${action##*@}
    printf '%s\n' "$revision" | grep -Eq '^[0-9a-f]{40}$' \
        || die "$path:$line_number uses a remote action without a full commit SHA: $action"
done

conflicts=$(git grep -n -E '^(<<<<<<< |>>>>>>> )' -- . ':!.codex-remote-attachments' 2>/dev/null || true)
[ -z "$conflicts" ] || die "unresolved merge-conflict markers found:\n$conflicts"

printf 'PASS: repository contracts and tracked POSIX shell syntax are valid\n'
