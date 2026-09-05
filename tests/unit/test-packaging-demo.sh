#!/bin/sh
set -eu
packager=$1
sample=$2
test_root=$(mktemp -d "${TMPDIR:-/tmp}/northstar-packaging-test.XXXXXX")
trap 'rm -rf "$test_root"' EXIT HUP INT TERM
# The disposable XDG root keeps this test out of the real user's applications.
export XDG_DATA_HOME="$test_root/data"
export QT_QPA_PLATFORM=offscreen
mkdir -m 700 "$XDG_DATA_HOME"
"$packager" package "$sample/recipe.json" "$test_root/Demo.app"
"$packager" inspect "$test_root/Demo.app" | grep -F 'BundleIdentifier=org.northstar.PackagingDemo'
"$test_root/Demo.app/Contents/Executable/app" --self-test
"$packager" install "$test_root/Demo.app"
"$XDG_DATA_HOME/northstar/apps/org.northstar.PackagingDemo.app/Contents/Executable/app" --self-test
if "$packager" install "$test_root/Demo.app"; then
    echo 'ERROR: duplicate install succeeded' >&2
    exit 1
fi
"$packager" remove org.northstar.PackagingDemo
test -d "$XDG_DATA_HOME/Trash/files/org.northstar.PackagingDemo.app"
test -d "$test_root/Demo.app"
echo 'PASS: native graphical sample package, inspect, launch, install, duplicate rejection and Trash removal'
