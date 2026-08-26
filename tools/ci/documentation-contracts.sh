#!/bin/sh

# Guard the small set of top-level documents that define current project truth.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)

die() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

for path in README.md CONTRIBUTING.md docs/CI.md docs/ROADMAP.md \
    docs/ROADMAP_HISTORY.md docs/QUALITY_GATES.md; do
    [ -s "$ROOT/$path" ] || die "required project document is missing: $path"
done

require_text() {
    path=$1
    text=$2
    grep -F "$text" "$ROOT/$path" >/dev/null \
        || die "$path is missing current project truth: $text"
}

reject_text() {
    path=$1
    text=$2
    if grep -F "$text" "$ROOT/$path" >/dev/null; then
        die "$path contains an archived project claim: $text"
    fi
}

require_text README.md 'one accepted physical Intel lane'
require_text CONTRIBUTING.md '`main` is protected'
require_text docs/CI.md 'Protection is active on `main`'
require_text docs/ROADMAP.md 'Accepted physical Intel lane'
require_text docs/ROADMAP.md 'AMD, native multi-display, and the consolidated Alpha regression remain open'
require_text docs/ROADMAP.md '[`ROADMAP_HISTORY.md`](ROADMAP_HISTORY.md)'
require_text docs/ROADMAP_HISTORY.md 'Archived on 2026-08-25'

reject_text CONTRIBUTING.md 'the first source implementation is deliberately deferred'
reject_text docs/CI.md 'Enable protection only after both workflow checks'
reject_text docs/ROADMAP.md 'physical Intel/AMD evidence pending'
reject_text docs/ROADMAP.md 'writable radios await hardware'

printf 'PASS: top-level documentation reflects the current M6/M7 project state\n'
