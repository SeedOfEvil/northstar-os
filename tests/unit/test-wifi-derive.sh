#!/bin/sh
set -eu
DERIVE=$1
expected=f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e
actual=$(printf '%s\n' password | "$DERIVE" IEEE)
[ "$actual" = "$expected" ] || { printf '%s\n' 'FAIL: WPA2 derivation did not match the standard vector' >&2; exit 1; }
if printf '%s\n' short | "$DERIVE" IEEE >/dev/null 2>&1; then
    printf '%s\n' 'FAIL: short passphrase was accepted' >&2
    exit 1
fi
if printf '%s\n' password | "$DERIVE" IEEE password >/dev/null 2>&1; then
    printf '%s\n' 'FAIL: a passphrase process argument was accepted' >&2
    exit 1
fi
printf '%s\n' 'PASS: Wi-Fi PSK derivation matches the standard vector and accepts secrets only on stdin'
