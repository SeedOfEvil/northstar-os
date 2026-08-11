#!/bin/sh

set -eu

if [ "$(uname -s)" != FreeBSD ]; then
    printf 'SKIP: runtime bundle fixture requires FreeBSD pkg semantics\n'
    exit 0
fi

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
CAPTURE=$ROOT/image/scripts/capture-runtime-bundle.sh
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-runtime-bundle.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM
mkdir -p "$TMP_DIR/bin"
mkdir -p "$TMP_DIR/cache"

printf 'northstar package fixture\n' > "$TMP_DIR/northstar-0.1.4-amd64.pkg"
printf 'qt package fixture\n' > "$TMP_DIR/cache/qt6-base-6.11.1.pkg"
cp "$TMP_DIR/cache/qt6-base-6.11.1.pkg" "$TMP_DIR/cache/qt6-base-duplicate.pkg"
printf '%s\n' northstar qt6-base > "$TMP_DIR/roots"

cat > "$TMP_DIR/bin/pkg" <<'EOF'
#!/bin/sh
set -eu
case "${1-}" in
    info)
        [ "${2-}" = -e ] || exit 2
        case "${3-}" in northstar|qt6-base) exit 0 ;; *) exit 1 ;; esac
        ;;
    query)
        if [ "${2-}" = -F ]; then
            file=${3-}
            format=${4-}
            filename=${file##*/}
            case "$filename" in
                *northstar*) name=northstar; version=0.1.4; origin=x11/northstar ;;
                *qt6-base*) name=qt6-base; version=6.11.1; origin=devel/qt6-base ;;
                *) exit 3 ;;
            esac
            case "$format" in
                %n) printf '%s\n' "$name" ;;
                '%n|%v') printf '%s|%s\n' "$name" "$version" ;;
                '%n|%v|%o') printf '%s|%s|%s\n' "$name" "$version" "$origin" ;;
                '%At=%Av') printf '%s\n' 'ports_top_git_hash=fixture' ;;
                *) exit 4 ;;
            esac
        elif [ "${2-}" = -e ]; then
            expression=${3-}
            format=${4-}
            case "$format" in
                %dn)
                    case "$expression" in
                        *northstar*) printf '%s\n' qt6-base ;;
                        *qt6-base*) : ;;
                        *) exit 6 ;;
                    esac
                    ;;
                %v)
                    case "$expression" in
                        *qt6-base*) printf '%s\n' 6.11.1 ;;
                        *) exit 6 ;;
                    esac
                    ;;
                '%At=%Av') printf '%s\n' 'ports_top_git_hash=fixture' ;;
                *) exit 5 ;;
            esac
        else
            exit 7
        fi
        ;;
    *) exit 9 ;;
esac
EOF
chmod +x "$TMP_DIR/bin/pkg"

PATH="$TMP_DIR/bin:$PATH" "$CAPTURE" \
    --roots "$TMP_DIR/roots" \
    --northstar-package "$TMP_DIR/northstar-0.1.4-amd64.pkg" \
    --package-cache "$TMP_DIR/cache" \
    --source-date-epoch 1781274780 \
    --output "$TMP_DIR/output" >/dev/null

grep -F 'package_count=2' "$TMP_DIR/output/runtime-bundle.conf" >/dev/null
grep -F '|northstar|0.1.4|x11/northstar' \
    "$TMP_DIR/output/runtime-package-records" >/dev/null
grep -F '|qt6-base|6.11.1|devel/qt6-base' \
    "$TMP_DIR/output/runtime-package-records" >/dev/null

if PATH="$TMP_DIR/bin:$PATH" "$CAPTURE" \
    --roots "$TMP_DIR/roots" \
    --northstar-package "$TMP_DIR/northstar-0.1.4-amd64.pkg" \
    --package-cache "$TMP_DIR/cache" \
    --source-date-epoch 1781274780 \
    --output "$TMP_DIR/output" >/dev/null 2>&1; then
    printf 'FAIL: runtime capture replaced immutable output\n' >&2
    exit 1
fi

printf 'missing-package\n' > "$TMP_DIR/missing-roots"
if PATH="$TMP_DIR/bin:$PATH" "$CAPTURE" \
    --roots "$TMP_DIR/missing-roots" \
    --northstar-package "$TMP_DIR/northstar-0.1.4-amd64.pkg" \
    --package-cache "$TMP_DIR/cache" \
    --source-date-epoch 1781274780 \
    --output "$TMP_DIR/missing-output" >/dev/null 2>&1; then
    printf 'FAIL: runtime capture accepted a missing root package\n' >&2
    exit 1
fi

printf 'PASS: runtime package capture is exact, closed, and immutable\n'
