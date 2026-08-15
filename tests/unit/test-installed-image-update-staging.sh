#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
STAGER=$ROOT/tools/stage-installed-image-update-candidate.sh
GATE=$ROOT/image/scripts/validate-image-update-rollback.sh

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    file=$1
    text=$2
    grep -F "$text" "$file" >/dev/null 2>&1 || fail "$file omits: $text"
}

sh -n "$STAGER"
sh -n "$GATE"

contains "$STAGER" "candidate staging output must remain beneath /home"
contains "$STAGER" "installed image is not the accepted baseline commit"
contains "$STAGER" "repository catalogue digest does not match its publication record"
contains "$STAGER" "repository must contain exactly one Northstar package"
contains "$STAGER" "trusted pkg client did not expose the expected candidate"
contains "$STAGER" "MUTATION=none"
contains "$GATE" "schema_version=2"
contains "$GATE" "transaction repository revision changed"
contains "$GATE" "transaction source revision changed"
contains "$GATE" "transaction catalogue digest changed"
contains "$GATE" "transaction signature fingerprint changed"

if grep -Eq '(^|[[:space:]])bectl([[:space:]]|$)|pkg([[:space:]]+[^#]*[[:space:]])?upgrade' "$STAGER"; then
    fail 'candidate staging helper contains package or boot-environment mutation'
fi

printf 'PASS: installed-image candidate staging is immutable, identity-bound, and non-mutating\n'
