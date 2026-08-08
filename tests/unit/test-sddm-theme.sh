#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
THEME=$ROOT/config/sddm/northstar
LOGO=$ROOT/assets/branding/northstar-logo.png
THEME_LOGO=$THEME/assets/northstar-logo.png

[ -f "$THEME/metadata.desktop" ]
[ -f "$THEME/theme.conf" ]
[ -f "$THEME/Main.qml" ]
[ -s "$LOGO" ]
[ -s "$THEME_LOGO" ]

grep -q '^Name=Northstar$' "$THEME/metadata.desktop"
grep -q '^MainScript=Main.qml$' "$THEME/metadata.desktop"
grep -q '^ConfigFile=theme.conf$' "$THEME/metadata.desktop"
grep -q '^logo=assets/northstar-logo.png$' "$THEME/theme.conf"
grep -q '^backgroundImage=assets/northstar-logo.png$' "$THEME/theme.conf"
grep -q 'sddm.login' "$THEME/Main.qml"
grep -q 'sddm.reboot' "$THEME/Main.qml"
grep -q 'sddm.powerOff' "$THEME/Main.qml"
grep -q 'backgroundImageSource' "$THEME/Main.qml"

if ! cmp -s "$LOGO" "$THEME_LOGO"; then
    printf '%s\n' 'FAIL: SDDM theme logo differs from the canonical Northstar logo' >&2
    exit 1
fi

printf '%s\n' 'PASS: Northstar SDDM theme assets and authentication boundary are present'
