#!/bin/sh
set -eu
packager=$1
sample=$2
test_root=$(mktemp -d "${TMPDIR:-/tmp}/northstar-web-test.XXXXXX")
trap 'rm -rf "$test_root"' EXIT HUP INT TERM
export XDG_DATA_HOME="$test_root/data"
mkdir -m 700 "$XDG_DATA_HOME"
"$packager" package "$sample/recipe.json" "$test_root/Web.app"
"$packager" inspect "$test_root/Web.app" > "$test_root/details"
grep -Fx 'Type=Web application' "$test_root/details"
grep -Fx 'URL=https://example.org/' "$test_root/details"
grep -F 'Shares Firefox cookies' "$test_root/details"
test ! -e "$test_root/Web.app/Contents/Executable/app"
"$packager" install "$test_root/Web.app"
"$packager" remove org.northstar.WebDemo
test -d "$XDG_DATA_HOME/Trash/files/org.northstar.WebDemo.app"
test -d "$test_root/Web.app"
echo 'PASS: web bundle CLI lifecycle without launching a browser or contacting a website'
