#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
SMOKE=$ROOT/image/scripts/boot-smoke-qcow2.sh

[ -x "$SMOKE" ] || {
    printf 'FAIL: QCOW2 boot-smoke script is not executable\n' >&2
    exit 1
}
"$SMOKE" --help | grep -F 'snapshot=on' >/dev/null || {
    printf 'FAIL: help omits the immutable snapshot boundary\n' >&2
    exit 1
}
grep -F 'file=$IMAGE,if=virtio,format=qcow2,snapshot=on' "$SMOKE" >/dev/null || {
    printf 'FAIL: boot smoke can mutate its source QCOW2\n' >&2
    exit 1
}
grep -F 'source QCOW2 changed during snapshot-only boot smoke' "$SMOKE" >/dev/null || {
    printf 'FAIL: boot smoke does not verify source-image immutability\n' >&2
    exit 1
}
grep -F "Trying to mount root from zfs:" "$SMOKE" >/dev/null || {
    printf 'FAIL: boot smoke does not require ZFS root evidence\n' >&2
    exit 1
}
grep -F "FreeBSD/amd64 (northstar-image)" "$SMOKE" >/dev/null || {
    printf 'FAIL: boot smoke does not require the Northstar first-boot identity\n' >&2
    exit 1
}
grep -F 'graphical_acceptance=separate_proxmox_gate' "$SMOKE" >/dev/null || {
    printf 'FAIL: boot smoke conflates serial and graphical acceptance\n' >&2
    exit 1
}

printf 'PASS: QCOW2 boot smoke is bounded, snapshot-only, and evidence-backed\n'
