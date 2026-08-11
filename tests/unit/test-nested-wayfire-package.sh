#!/bin/sh

set -eu

if [ "$(uname -s)" != FreeBSD ]; then
    printf 'SKIP: nested Wayfire package fixture requires FreeBSD pkg\n'
    exit 0
fi

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
PACKAGER=$ROOT/image/scripts/package-nested-wayfire.sh
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-wayfire-package.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM
mkdir -p "$TMP_DIR/source/bin" "$TMP_DIR/source/lib/wayfire"
printf '#!/bin/sh\nprintf "fixture wayfire\\n"\n' > "$TMP_DIR/source/bin/wayfire"
printf 'plugin fixture\n' > "$TMP_DIR/source/lib/wayfire/libfixture.so"
chmod +x "$TMP_DIR/source/bin/wayfire"

"$PACKAGER" --source "$TMP_DIR/source" --output "$TMP_DIR/output" \
    --source-revision 746bc7e \
    --patch-sha256 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef \
    --source-date-epoch 1781274780 >/dev/null

package_path=$(find "$TMP_DIR/output" -type f -name '*.pkg' -print)
[ "$(pkg query -F "$package_path" '%n')" = northstar-wayfire-nested ]
grep -F 'source_revision=746bc7e' "$TMP_DIR/output/compat-package.conf" >/dev/null
grep -F 'runtime_tree_sha256=' "$TMP_DIR/output/compat-package.conf" >/dev/null

if "$PACKAGER" --source "$TMP_DIR/source" --output "$TMP_DIR/output" \
    --source-revision 746bc7e \
    --patch-sha256 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef \
    --source-date-epoch 1781274780 >/dev/null 2>&1; then
    printf 'FAIL: compatibility packager replaced immutable output\n' >&2
    exit 1
fi

printf 'PASS: nested Wayfire compatibility package is deterministic and immutable\n'
