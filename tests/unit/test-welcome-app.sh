#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
welcome_script="$project_root/apps/samples/NorthstarWelcome.app/Contents/Executable/northstar-welcome"
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/northstar-welcome-test.XXXXXX")

cleanup() {
    rm -rf "$tmp_dir"
}

trap cleanup EXIT HUP INT TERM

mkdir -p "$tmp_dir/bin"

cat >"$tmp_dir/bin/qterminal" <<'STUB'
#!/bin/sh

printf '%s\n' "$@" > "$QTERMINAL_ARGS"
STUB

cat >"$tmp_dir/bin/xterm" <<'STUB'
#!/bin/sh

printf '%s\n' "$@" > "$XTERM_ARGS"
STUB

chmod +x "$tmp_dir/bin/qterminal" "$tmp_dir/bin/xterm"

QTERMINAL_ARGS="$tmp_dir/qterminal.args" \
XTERM_ARGS="$tmp_dir/xterm.args" \
PATH="$tmp_dir/bin:/bin" \
sh "$welcome_script" >/dev/null 2>&1

if ! grep -Fx -- '-e' "$tmp_dir/qterminal.args" >/dev/null; then
    printf '%s\n' 'FAIL: qterminal was not launched with -e' >&2
    exit 1
fi

if grep -Fx -- '--title' "$tmp_dir/qterminal.args" >/dev/null; then
    printf '%s\n' 'FAIL: qterminal received unsupported --title' >&2
    exit 1
fi

if [ -e "$tmp_dir/xterm.args" ]; then
    printf '%s\n' 'FAIL: xterm fallback was used while qterminal was available' >&2
    exit 1
fi

rm -f "$tmp_dir/bin/qterminal" "$tmp_dir/qterminal.args"

XTERM_ARGS="$tmp_dir/xterm.args" \
PATH="$tmp_dir/bin:$PATH" \
sh "$welcome_script" >/dev/null 2>&1

if ! grep -Fx -- '-T' "$tmp_dir/xterm.args" >/dev/null || \
   ! grep -Fx -- 'Northstar Welcome' "$tmp_dir/xterm.args" >/dev/null; then
    printf '%s\n' 'FAIL: xterm fallback did not receive the Welcome title' >&2
    exit 1
fi

printf '%s\n' 'PASS: Northstar Welcome selects supported terminal arguments'
