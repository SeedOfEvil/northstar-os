#!/bin/sh

# Boot a Northstar QCOW2 through UEFI without modifying the source image.
# Success requires the serial console to prove ZFS root mount and multi-user
# login. Graphical acceptance remains a separate Proxmox/noVNC check.

set -eu

IMAGE=
FIRMWARE_CODE=
FIRMWARE_VARS=
OUTPUT=
TIMEOUT_SECONDS=300
MEMORY_MB=2048
STAGING=
QEMU_PID=
SUCCESS=0

usage() {
    cat <<'USAGE'
Usage: boot-smoke-qcow2.sh --image FILE --firmware-code FILE \
  --firmware-vars FILE --output NEW_DIRECTORY \
  [--timeout-seconds 300] [--memory-mb 2048]

Boots the image with TCG, a private user network, and snapshot=on. The source
QCOW2 and firmware templates are never modified. Passing evidence proves UEFI,
virtio disk discovery, ZFS root mount, rc startup, and a multi-user login prompt.
USAGE
}

die() { printf 'ERROR: %s\n' "$1" >&2; exit 1; }

cleanup() {
    set +e
    if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
        kill -TERM "$QEMU_PID" 2>/dev/null
        wait "$QEMU_PID" 2>/dev/null
    fi
    if [ "$SUCCESS" -ne 1 ] && [ -n "$STAGING" ] && [ -d "$STAGING" ]; then
        rm -rf "$STAGING"
    fi
}
trap cleanup EXIT HUP INT TERM

while [ "$#" -gt 0 ]; do
    case "$1" in
        --image) IMAGE=${2-}; shift 2 ;;
        --firmware-code) FIRMWARE_CODE=${2-}; shift 2 ;;
        --firmware-vars) FIRMWARE_VARS=${2-}; shift 2 ;;
        --output) OUTPUT=${2-}; shift 2 ;;
        --timeout-seconds) TIMEOUT_SECONDS=${2-}; shift 2 ;;
        --memory-mb) MEMORY_MB=${2-}; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

for command_name in awk basename cat chmod cp dirname grep kill mkdir mktemp \
    mv qemu-img qemu-system-x86_64 rm sed sha256 sleep stat tail tr wc; do
    command -v "$command_name" >/dev/null 2>&1 \
        || die "required command is unavailable: $command_name"
done

for path in "$IMAGE" "$FIRMWARE_CODE" "$FIRMWARE_VARS"; do
    [ -f "$path" ] && [ ! -L "$path" ] || die 'image and firmware inputs must be regular non-symlink files'
done
[ -n "$OUTPUT" ] && [ ! -e "$OUTPUT" ] && [ ! -L "$OUTPUT" ] \
    || die 'output must not exist'
case "$TIMEOUT_SECONDS" in ''|*[!0-9]*) die 'timeout must be an integer' ;; esac
case "$MEMORY_MB" in ''|*[!0-9]*) die 'memory must be an integer' ;; esac
[ "$TIMEOUT_SECONDS" -ge 30 ] && [ "$TIMEOUT_SECONDS" -le 900 ] \
    || die 'timeout must be between 30 and 900 seconds'
[ "$MEMORY_MB" -ge 1024 ] && [ "$MEMORY_MB" -le 8192 ] \
    || die 'memory must be between 1024 and 8192 MiB'

IMAGE=$(CDPATH= cd -- "$(dirname "$IMAGE")" && pwd)/$(basename "$IMAGE")
FIRMWARE_CODE=$(CDPATH= cd -- "$(dirname "$FIRMWARE_CODE")" && pwd)/$(basename "$FIRMWARE_CODE")
FIRMWARE_VARS=$(CDPATH= cd -- "$(dirname "$FIRMWARE_VARS")" && pwd)/$(basename "$FIRMWARE_VARS")
output_parent=$(dirname "$OUTPUT")
mkdir -p "$output_parent"
output_parent=$(CDPATH= cd -- "$output_parent" && pwd)
OUTPUT=$output_parent/$(basename "$OUTPUT")
STAGING=$(mktemp -d "$output_parent/.northstar-boot-smoke.XXXXXX")

qemu-img check "$IMAGE" > "$STAGING/qcow2-check.txt"
qemu-img info "$IMAGE" > "$STAGING/qcow2-info.txt"
grep -F 'file format: qcow2' "$STAGING/qcow2-info.txt" >/dev/null \
    || die 'input is not a QCOW2 image'
image_sha256_before=$(sha256 -q "$IMAGE")
cp "$FIRMWARE_VARS" "$STAGING/uefi-vars.fd"

serial_log=$STAGING/serial.log
pid_file=$STAGING/qemu.pid
qemu-system-x86_64 \
    -machine q35,accel=tcg -cpu qemu64 -m "$MEMORY_MB" -smp 2 \
    -drive "if=pflash,format=raw,readonly=on,file=$FIRMWARE_CODE" \
    -drive "if=pflash,format=raw,file=$STAGING/uefi-vars.fd" \
    -drive "file=$IMAGE,if=virtio,format=qcow2,snapshot=on" \
    -netdev user,id=net0 -device virtio-net-pci,netdev=net0 \
    -display none -serial "file:$serial_log" -pidfile "$pid_file" -daemonize
QEMU_PID=$(sed -n '1p' "$pid_file")
printf '%s\n' "$QEMU_PID" | grep -Eq '^[0-9]+$' || die 'QEMU did not record a valid PID'

elapsed=0
while [ "$elapsed" -lt "$TIMEOUT_SECONDS" ]; do
    if grep -F 'panic:' "$serial_log" >/dev/null 2>&1; then
        tail -80 "$serial_log" >&2
        die 'guest kernel panic detected'
    fi
    if grep -F 'Trying to mount root from zfs:' "$serial_log" >/dev/null 2>&1 \
        && grep -F 'FreeBSD/amd64 (northstar-image)' "$serial_log" >/dev/null 2>&1 \
        && grep -F 'login:' "$serial_log" >/dev/null 2>&1; then
        break
    fi
    if ! kill -0 "$QEMU_PID" 2>/dev/null; then
        tail -80 "$serial_log" >&2
        die 'QEMU exited before first boot completed'
    fi
    sleep 5
    elapsed=$((elapsed + 5))
done
[ "$elapsed" -lt "$TIMEOUT_SECONDS" ] || {
    tail -80 "$serial_log" >&2
    die 'guest did not reach the multi-user login prompt before timeout'
}

kill -TERM "$QEMU_PID" 2>/dev/null || true
attempt=0
while kill -0 "$QEMU_PID" 2>/dev/null && [ "$attempt" -lt 10 ]; do
    sleep 1
    attempt=$((attempt + 1))
done
kill -0 "$QEMU_PID" 2>/dev/null && die 'QEMU did not stop after boot smoke'
QEMU_PID=
rm -f "$pid_file" "$STAGING/uefi-vars.fd"

image_sha256=$(sha256 -q "$IMAGE")
[ "$image_sha256" = "$image_sha256_before" ] \
    || die 'source QCOW2 changed during snapshot-only boot smoke'
serial_sha256=$(sha256 -q "$serial_log")
cat > "$STAGING/boot-smoke.conf" <<EOF
schema_version=1
source_image_sha256=$image_sha256
serial_log_sha256=$serial_sha256
firmware=UEFI
disk_interface=virtio
root_filesystem=ZFS
source_image_mutation=blocked_by_snapshot
graphical_acceptance=separate_proxmox_gate
EOF
chmod 444 "$STAGING"/*
mv "$STAGING" "$OUTPUT"
STAGING=
SUCCESS=1
printf 'PASS: Northstar QCOW2 reached ZFS multi-user boot at %s\n' "$OUTPUT"
