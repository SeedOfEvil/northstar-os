#!/bin/sh

# Convert one accepted Northstar QCOW2 into a raw USB installer image and add
# a signed, read-only installer-source partition plus installer-only system
# state. Production accepts no disk device and modifies only its new raw file.

set -eu

IMAGE=
PROVENANCE=
SOURCE=
OUTPUT=
BUILDER_MARKER=/etc/northstar/disposable-installer-media-builder.conf
SOURCE_SIZE_GB=4
PREFLIGHT=0
TEST_MODE=${NORTHSTAR_INSTALLER_MEDIA_TEST_MODE:-0}
QEMU_IMG=${NORTHSTAR_INSTALLER_MEDIA_QEMU_IMG:-qemu-img}
OPENSSL=${NORTHSTAR_INSTALLER_MEDIA_OPENSSL:-openssl}
STAGING=
MD_DEVICE=
POOL=
MOUNT_ROOT=
MOUNT_SOURCE=
ROOT_MOUNTED=0
HOME_MOUNTED=0
TMP_MOUNTED=0
SOURCE_MOUNTED=0
POOL_IMPORTED=0
SUCCESS=0

usage() {
    cat <<'USAGE'
Usage: assemble-installer-usb.sh --image ACCEPTED_QCOW2 \
  --image-provenance FILE --installer-source SIGNED_DIRECTORY \
  --output NEW_DIRECTORY [--source-size-gb 4] [--builder-marker FILE] \
  [--preflight]

Preflight verifies all immutable inputs and runs only read-only qemu-img
inspection. Production requires FreeBSD root and a protected disposable-media
builder marker. It never accepts or writes a host disk device.
USAGE
}

die() { printf 'ERROR: %s\n' "$1" >&2; exit 1; }
cleanup() {
    set +e
    [ "$SOURCE_MOUNTED" -ne 1 ] || umount "$MOUNT_SOURCE" >/dev/null 2>&1
    [ "$TMP_MOUNTED" -ne 1 ] || zfs unmount "$POOL/tmp" >/dev/null 2>&1
    [ "$HOME_MOUNTED" -ne 1 ] || zfs unmount "$POOL/home" >/dev/null 2>&1
    [ "$ROOT_MOUNTED" -ne 1 ] || zfs unmount "$POOL/ROOT/default" >/dev/null 2>&1
    [ "$POOL_IMPORTED" -ne 1 ] || zpool export "$POOL" >/dev/null 2>&1
    [ -z "$MD_DEVICE" ] || mdconfig -d -u "${MD_DEVICE#md}" >/dev/null 2>&1
    if [ "$SUCCESS" -ne 1 ] && [ -n "$STAGING" ] && [ -d "$STAGING" ]; then rm -rf "$STAGING"; fi
}
trap cleanup EXIT HUP INT TERM

while [ "$#" -gt 0 ]; do
    case "$1" in
        --image) IMAGE=${2-}; shift 2 ;;
        --image-provenance) PROVENANCE=${2-}; shift 2 ;;
        --installer-source) SOURCE=${2-}; shift 2 ;;
        --output) OUTPUT=${2-}; shift 2 ;;
        --source-size-gb) SOURCE_SIZE_GB=${2-}; shift 2 ;;
        --builder-marker) BUILDER_MARKER=${2-}; shift 2 ;;
        --preflight) PREFLIGHT=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

if [ "$TEST_MODE" = 1 ] && [ "$(id -u)" -eq 0 ]; then die 'installer-media test mode is forbidden for root'; fi
if [ "$TEST_MODE" != 1 ]; then QEMU_IMG=qemu-img; OPENSSL=openssl; fi
for command_name in awk basename cat chmod chown cp cut dirname grep mkdir mktemp mv rm sed stat tar tr uname wc "$QEMU_IMG" "$OPENSSL"; do
    command -v "$command_name" >/dev/null 2>&1 || die "required command is unavailable: $command_name"
done
if command -v sha256 >/dev/null 2>&1; then SHA256_COMMAND=sha256
elif command -v sha256sum >/dev/null 2>&1; then SHA256_COMMAND=sha256sum
else die 'sha256 or sha256sum is required'; fi

file_sha256() { if [ "$SHA256_COMMAND" = sha256 ]; then sha256 -q "$1"; else sha256sum "$1" | awk '{ print $1 }'; fi; }
file_size() { if [ "$(uname -s)" = FreeBSD ]; then stat -f '%z' "$1"; else wc -c < "$1" | tr -d ' '; fi; }
config_count() { awk -F= -v key="$2" '$1 == key { count++ } END { print count + 0 }' "$1"; }
config_value() { awk -F= -v key="$2" '$1 == key { sub(/^[^=]*=/, ""); print; exit }' "$1"; }
required_value() {
    [ "$(config_count "$1" "$2")" -eq 1 ] || die "configuration key must appear once: $2"
    value=$(config_value "$1" "$2"); [ -n "$value" ] || die "configuration key is empty: $2"; printf '%s' "$value"
}
validate_config_keys() {
    config_file=$1; expected_count=$2; allowed_keys=$3; label=$4
    seen='|'; count=0
    while IFS='=' read -r key value || [ -n "${key:-}" ]; do
        [ -n "${key:-}" ] && [ -n "${value:-}" ] || die "$label contains a blank field"
        printf '%s\n' "$key" | grep -Eq '^[A-Za-z][A-Za-z0-9_]*$' || die "$label contains an unsafe key"
        case " $allowed_keys " in *" $key "*) ;; *) die "$label contains unknown field: $key" ;; esac
        case "$seen" in *"|$key|"*) die "$label repeats field: $key" ;; esac
        printf '%s' "$value" | grep -Eq '[[:cntrl:]]' && die "$label contains a control character"
        seen="${seen}${key}|"; count=$((count + 1))
    done < "$config_file"
    [ "$count" -eq "$expected_count" ] || die "$label has an unexpected field count"
}

[ -f "$IMAGE" ] && [ ! -L "$IMAGE" ] && [ -r "$IMAGE" ] || die 'source QCOW2 must be a readable regular file'
[ -f "$PROVENANCE" ] && [ ! -L "$PROVENANCE" ] && [ -r "$PROVENANCE" ] || die 'image provenance must be a readable regular file'
[ -d "$SOURCE" ] && [ ! -L "$SOURCE" ] || die 'installer source must be a real directory'
[ -n "$OUTPUT" ] && [ ! -e "$OUTPUT" ] && [ ! -L "$OUTPUT" ] || die 'output must not exist'
case "$SOURCE_SIZE_GB" in ''|*[!0-9]*) die 'source partition size must be an integer GiB value' ;; esac
[ "$SOURCE_SIZE_GB" -ge 2 ] && [ "$SOURCE_SIZE_GB" -le 32 ] || die 'source partition size must be between 2 and 32 GiB'

validate_config_keys "$PROVENANCE" 19 \
    'schema_version artifact artifact_sha256 artifact_size virtual_size_gib firmware partition_table root_filesystem zpool project_commit builder_id builder_marker_sha256 resolved_inputs_sha256 runtime_package_records_sha256 runtime_package_count installer_payload installer_payload_sha256 installer_payload_size development_autologin' \
    'image provenance'
[ "$(required_value "$PROVENANCE" schema_version)" = 1 ] || die 'unsupported image provenance schema'
image_name=$(required_value "$PROVENANCE" artifact)
[ "$image_name" = northstar-15.1-amd64.qcow2 ] || die 'source provenance is not the accepted Northstar QCOW2 type'
[ "$(required_value "$PROVENANCE" firmware)" = UEFI ] || die 'source image firmware is unsupported'
[ "$(required_value "$PROVENANCE" partition_table)" = GPT ] || die 'source partition table is unsupported'
[ "$(required_value "$PROVENANCE" root_filesystem)" = ZFS ] || die 'source root filesystem is unsupported'
[ "$(required_value "$PROVENANCE" development_autologin)" = 0 ] || die 'development-autologin images cannot become installer media'
image_sha256=$(required_value "$PROVENANCE" artifact_sha256)
image_size=$(required_value "$PROVENANCE" artifact_size)
project_commit=$(required_value "$PROVENANCE" project_commit)
pool=$(required_value "$PROVENANCE" zpool)
virtual_size_gib=$(required_value "$PROVENANCE" virtual_size_gib)
printf '%s\n' "$image_sha256" | grep -Eq '^[0-9a-f]{64}$' || die 'source image digest is unsafe'
printf '%s\n' "$image_size" | grep -Eq '^[0-9]+$' || die 'source image size is unsafe'
printf '%s\n' "$project_commit" | grep -Eq '^[0-9a-f]{40}$' || die 'source project commit is unsafe'
printf '%s\n' "$pool" | grep -Eq '^nstar_[0-9a-f]{12}$' || die 'source pool name is unsafe'
printf '%s\n' "$virtual_size_gib" | grep -Eq '^[0-9]{1,2}$' || die 'source virtual size is unsafe'
[ "$(file_sha256 "$IMAGE" | tr '[:upper:]' '[:lower:]')" = "$image_sha256" ] || die 'source QCOW2 digest does not match provenance'
[ "$(file_size "$IMAGE")" = "$image_size" ] || die 'source QCOW2 size does not match provenance'

manifest=$SOURCE/source-manifest.conf
signature=$SOURCE/source-manifest.conf.sig
trust_key=$SOURCE/source-signing.pem
source_provenance=$SOURCE/installer-source-provenance.conf
runtime_manifest=$SOURCE/runtime-manifest.conf
for path in "$manifest" "$signature" "$trust_key" "$source_provenance" "$runtime_manifest"; do
    [ -f "$path" ] && [ ! -L "$path" ] && [ -r "$path" ] || die "installer source file is missing or unsafe: $(basename "$path")"
done
validate_config_keys "$manifest" 10 \
    'schema_version product freebsd_release architecture project_commit payload_kind payload_name payload_size payload_sha256 runtime_manifest_sha256' \
    'installer source manifest'
validate_config_keys "$source_provenance" 11 \
    'schema_version product source_manifest_sha256 source_signature_sha256 source_trust_key_sha256 payload_name payload_size payload_sha256 runtime_manifest_sha256 project_commit private_key_included' \
    'installer source provenance'
validate_config_keys "$runtime_manifest" 7 \
    'schema_version product freebsd_release architecture project_commit runtime_package_records_sha256 runtime_package_count' \
    'runtime manifest'
[ "$(required_value "$manifest" schema_version)" = 2 ] || die 'unsupported installer source manifest schema'
[ "$(required_value "$manifest" product)" = Northstar ] || die 'installer source product is not Northstar'
[ "$(required_value "$manifest" freebsd_release)" = 15.1-RELEASE ] || die 'installer source release is unsupported'
[ "$(required_value "$manifest" architecture)" = amd64 ] || die 'installer source architecture is unsupported'
[ "$(required_value "$manifest" project_commit)" = "$project_commit" ] || die 'installer source and QCOW2 commits differ'
[ "$(required_value "$manifest" payload_kind)" = northstar-rootfs-v1 ] || die 'installer payload kind is unsupported'
[ "$(required_value "$runtime_manifest" schema_version)" = 1 ] || die 'unsupported runtime manifest schema'
[ "$(required_value "$runtime_manifest" product)" = Northstar ] || die 'runtime manifest product is not Northstar'
[ "$(required_value "$runtime_manifest" freebsd_release)" = 15.1-RELEASE ] || die 'runtime manifest release is unsupported'
[ "$(required_value "$runtime_manifest" architecture)" = amd64 ] || die 'runtime manifest architecture is unsupported'
[ "$(required_value "$runtime_manifest" project_commit)" = "$project_commit" ] || die 'runtime manifest and QCOW2 commits differ'
[ "$(required_value "$runtime_manifest" runtime_package_records_sha256)" = "$(required_value "$PROVENANCE" runtime_package_records_sha256)" ] \
    || die 'runtime package-record digest differs from image provenance'
[ "$(required_value "$runtime_manifest" runtime_package_count)" = "$(required_value "$PROVENANCE" runtime_package_count)" ] \
    || die 'runtime package count differs from image provenance'
payload_name=$(required_value "$manifest" payload_name)
payload_size=$(required_value "$manifest" payload_size)
payload_sha256=$(required_value "$manifest" payload_sha256)
runtime_sha256=$(required_value "$manifest" runtime_manifest_sha256)
printf '%s\n' "$payload_name" | grep -Eq '^northstar-rootfs-v1-[0-9a-f]{12}\.txz$' || die 'installer payload name is unsafe'
[ "$payload_name" = "northstar-rootfs-v1-$(printf '%s' "$project_commit" | cut -c1-12).txz" ] || die 'installer payload name and commit differ'
printf '%s\n' "$payload_size" | grep -Eq '^[0-9]{1,12}$' || die 'installer payload size is unsafe'
printf '%s\n' "$payload_sha256" | grep -Eq '^[0-9a-f]{64}$' || die 'installer payload digest is unsafe'
printf '%s\n' "$runtime_sha256" | grep -Eq '^[0-9a-f]{64}$' || die 'runtime manifest digest is unsafe'
payload=$SOURCE/$payload_name
[ -f "$payload" ] && [ ! -L "$payload" ] || die 'installer payload is missing or unsafe'
[ "$(file_size "$payload")" = "$payload_size" ] || die 'installer payload size mismatch'
[ "$(file_sha256 "$payload" | tr '[:upper:]' '[:lower:]')" = "$payload_sha256" ] || die 'installer payload digest mismatch'
[ "$(file_sha256 "$runtime_manifest" | tr '[:upper:]' '[:lower:]')" = "$runtime_sha256" ] || die 'runtime manifest digest mismatch'
[ "$(required_value "$PROVENANCE" installer_payload)" = "$payload_name" ] || die 'installer source payload differs from image provenance'
[ "$(required_value "$PROVENANCE" installer_payload_sha256)" = "$payload_sha256" ] || die 'installer source payload digest differs from image provenance'
[ "$(required_value "$PROVENANCE" installer_payload_size)" = "$payload_size" ] || die 'installer source payload size differs from image provenance'
"$OPENSSL" dgst -sha256 -verify "$trust_key" -signature "$signature" "$manifest" >/dev/null 2>&1 \
    || die 'installer source signature verification failed'
manifest_sha256=$(file_sha256 "$manifest" | tr '[:upper:]' '[:lower:]')
[ "$(required_value "$source_provenance" schema_version)" = 1 ] || die 'unsupported installer source provenance schema'
[ "$(required_value "$source_provenance" product)" = Northstar ] || die 'installer source provenance product is not Northstar'
[ "$(required_value "$source_provenance" source_manifest_sha256)" = "$manifest_sha256" ] || die 'installer source provenance manifest digest mismatch'
[ "$(required_value "$source_provenance" source_signature_sha256)" = "$(file_sha256 "$signature" | tr '[:upper:]' '[:lower:]')" ] \
    || die 'installer source provenance signature digest mismatch'
[ "$(required_value "$source_provenance" source_trust_key_sha256)" = "$(file_sha256 "$trust_key" | tr '[:upper:]' '[:lower:]')" ] \
    || die 'installer source provenance trust-key digest mismatch'
[ "$(required_value "$source_provenance" payload_name)" = "$payload_name" ] || die 'installer source provenance payload name mismatch'
[ "$(required_value "$source_provenance" payload_size)" = "$payload_size" ] || die 'installer source provenance payload size mismatch'
[ "$(required_value "$source_provenance" payload_sha256)" = "$payload_sha256" ] || die 'installer source provenance payload digest mismatch'
[ "$(required_value "$source_provenance" runtime_manifest_sha256)" = "$runtime_sha256" ] \
    || die 'installer source provenance runtime-manifest digest mismatch'
[ "$(required_value "$source_provenance" project_commit)" = "$project_commit" ] || die 'installer source provenance commit mismatch'
[ "$(required_value "$source_provenance" private_key_included)" = no ] || die 'installer source claims to contain private signing material'
source_capacity=$((SOURCE_SIZE_GB * 1024 * 1024 * 1024))
[ "$payload_size" -le $((source_capacity - 268435456)) ] || die 'installer source partition is too small for its payload'

source_before=$(file_sha256 "$IMAGE" | tr '[:upper:]' '[:lower:]')
"$QEMU_IMG" check -f qcow2 "$IMAGE" >/dev/null || die 'source QCOW2 integrity check failed'
qemu_info=$($QEMU_IMG info -f qcow2 "$IMAGE") || die 'could not inspect source QCOW2'
printf '%s\n' "$qemu_info" | grep -F 'file format: qcow2' >/dev/null || die 'source image is not QCOW2'
printf '%s\n' "$qemu_info" | grep -F "virtual size: $virtual_size_gib GiB" >/dev/null || die 'source virtual size does not match provenance'
[ "$(file_sha256 "$IMAGE" | tr '[:upper:]' '[:lower:]')" = "$source_before" ] || die 'source QCOW2 changed during preflight'

if [ "$PREFLIGHT" -eq 1 ]; then
    printf 'PASS: verified Northstar installer USB inputs for %s (%s GiB source partition)\n' "$project_commit" "$SOURCE_SIZE_GB"
    exit 0
fi

[ "$(uname -s)" = FreeBSD ] || die 'installer USB assembly requires FreeBSD'
[ "$(id -u)" -eq 0 ] || die 'installer USB assembly must run as root on a disposable builder'
for command_name in chroot gpart mdconfig mount newfs pw sysrc umount zfs zpool; do
    command -v "$command_name" >/dev/null 2>&1 || die "required builder command is unavailable: $command_name"
done
[ -f "$BUILDER_MARKER" ] && [ ! -L "$BUILDER_MARKER" ] || die 'disposable-media builder marker is missing or unsafe'
[ "$(stat -f '%u' "$BUILDER_MARKER")" -eq 0 ] || die 'builder marker must be root-owned'
marker_mode=$(stat -f '%Lp' "$BUILDER_MARKER"); case "$marker_mode" in 400|600) ;; *) die 'builder marker mode must be 0400 or 0600' ;; esac
validate_config_keys "$BUILDER_MARKER" 5 \
    'SCHEMA_VERSION PURPOSE ALLOW_INSTALLER_MEDIA_ASSEMBLY EXPECTED_PROJECT_COMMIT BUILDER_ID' \
    'builder marker'
[ "$(required_value "$BUILDER_MARKER" SCHEMA_VERSION)" = 1 ] || die 'unsupported builder marker schema'
[ "$(required_value "$BUILDER_MARKER" PURPOSE)" = northstar-disposable-installer-media-builder ] || die 'builder marker purpose is invalid'
[ "$(required_value "$BUILDER_MARKER" ALLOW_INSTALLER_MEDIA_ASSEMBLY)" = YES ] || die 'builder marker does not authorize media assembly'
[ "$(required_value "$BUILDER_MARKER" EXPECTED_PROJECT_COMMIT)" = "$project_commit" ] || die 'builder marker targets a different project commit'
builder_id=$(required_value "$BUILDER_MARKER" BUILDER_ID)
printf '%s\n' "$builder_id" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9._-]{2,63}$' || die 'builder ID is unsafe'

output_parent=$(dirname "$OUTPUT"); mkdir -p "$output_parent"; output_parent=$(CDPATH= cd -- "$output_parent" && pwd -P)
OUTPUT=$output_parent/$(basename "$OUTPUT")
STAGING=$(mktemp -d "$output_parent/.northstar-installer-usb.XXXXXX")
raw=$STAGING/northstar-15.1-amd64-installer-usb.img
"$QEMU_IMG" convert -f qcow2 -O raw -S 4096 "$IMAGE" "$raw"
truncate -s "+${SOURCE_SIZE_GB}G" "$raw"
MD_DEVICE=$(mdconfig -a -t vnode -f "$raw")
printf '%s\n' "$MD_DEVICE" | grep -Eq '^md[0-9]+$' || die 'mdconfig returned an unsafe device identity'
gpart recover "$MD_DEVICE" >/dev/null
SOURCE_PROVIDER=$(gpart add -a 1m -t freebsd-ufs -l NSTAR_SOURCE "$MD_DEVICE")
[ "$SOURCE_PROVIDER" = "${MD_DEVICE}p3" ] || die 'accepted image did not produce the expected isolated source partition'
gpart show "$MD_DEVICE" > "$STAGING/partition-layout.txt"
newfs -U -L NSTAR_SOURCE "/dev/$SOURCE_PROVIDER" >/dev/null
MOUNT_ROOT=$STAGING/root; MOUNT_SOURCE=$STAGING/source; mkdir "$MOUNT_ROOT" "$MOUNT_SOURCE"
mount "/dev/$SOURCE_PROVIDER" "$MOUNT_SOURCE"; SOURCE_MOUNTED=1
cp "$manifest" "$signature" "$payload" "$MOUNT_SOURCE/"
chown root:wheel "$MOUNT_SOURCE"/*; chmod 0444 "$MOUNT_SOURCE"/*

POOL=$pool
zpool import -N -R "$MOUNT_ROOT" -d /dev "$POOL"; POOL_IMPORTED=1
zfs mount "$POOL/ROOT/default"; ROOT_MOUNTED=1
zfs mount "$POOL/home"; HOME_MOUNTED=1
zfs mount "$POOL/tmp"; TMP_MOUNTED=1
for required_path in usr/local/libexec/northstar-installer usr/local/libexec/northstar-installer-source-verify \
    usr/local/libexec/northstar-installer-engine usr/local/libexec/northstar-installer-executor; do
    [ -x "$MOUNT_ROOT/$required_path" ] || die "source image omits installer component: $required_path"
done
mkdir -p "$MOUNT_ROOT/usr/local/share/northstar/installer" "$MOUNT_ROOT/var/run/northstar-installer/source" \
    "$MOUNT_ROOT/etc/northstar" "$MOUNT_ROOT/usr/local/share/xsessions" \
    "$MOUNT_ROOT/usr/local/etc/sddm.conf.d" "$MOUNT_ROOT/usr/local/etc/polkit-1/rules.d"
cp "$trust_key" "$MOUNT_ROOT/usr/local/share/northstar/installer/source-signing.pem"
chown root:wheel "$MOUNT_ROOT/usr/local/share/northstar/installer/source-signing.pem"
chmod 0444 "$MOUNT_ROOT/usr/local/share/northstar/installer/source-signing.pem"
cat > "$MOUNT_ROOT/etc/northstar/installer-execution.conf" <<EOF
schema_version=1
purpose=northstar-installer-media
boot_environment=installer-media
allow_installer_execution=YES
execution_scope=confirmed-whole-disk
source_manifest_sha256=$manifest_sha256
EOF
chown root:wheel "$MOUNT_ROOT/etc/northstar/installer-execution.conf"; chmod 0400 "$MOUNT_ROOT/etc/northstar/installer-execution.conf"
grep -F '/var/run/northstar-installer/source' "$MOUNT_ROOT/etc/fstab" >/dev/null 2>&1 \
    && die 'source image already contains an installer-source mount'
printf '%s\n' '/dev/ufs/NSTAR_SOURCE /var/run/northstar-installer/source ufs ro,noatime 0 0' >> "$MOUNT_ROOT/etc/fstab"

if ! chroot "$MOUNT_ROOT" /usr/sbin/pw usershow northstar-installer >/dev/null 2>&1; then
    chroot "$MOUNT_ROOT" /usr/sbin/pw useradd northstar-installer -m -s /bin/sh -w none -c 'Northstar Installer'
fi
chroot "$MOUNT_ROOT" /usr/sbin/pw groupmod video -m northstar-installer >/dev/null
cat > "$MOUNT_ROOT/usr/local/bin/northstar-installer-session" <<'EOF'
#!/bin/sh
export QT_QPA_PLATFORM=xcb
export QT_QUICK_CONTROLS_STYLE=Basic
export XDG_SESSION_TYPE=x11
command -v xsetroot >/dev/null 2>&1 && xsetroot -solid '#07111f'
for agent in /usr/local/libexec/lxqt-policykit-agent /usr/local/bin/lxqt-policykit-agent; do
    if [ -x "$agent" ]; then
        "$agent" >/tmp/northstar-installer-policykit.log 2>&1 &
        break
    fi
done
exec /usr/local/libexec/northstar-installer
EOF
chmod 0555 "$MOUNT_ROOT/usr/local/bin/northstar-installer-session"
cat > "$MOUNT_ROOT/usr/local/share/xsessions/northstar-installer.desktop" <<'EOF'
[Desktop Entry]
Name=Northstar Installer
Comment=Install Northstar from verified read-only media
Type=XSession
Exec=/usr/local/bin/northstar-installer-session
TryExec=/usr/local/bin/northstar-installer-session
DesktopNames=Northstar
EOF
cat > "$MOUNT_ROOT/usr/local/etc/sddm.conf.d/40-northstar-installer-media.conf" <<'EOF'
[Autologin]
User=northstar-installer
Session=northstar-installer.desktop
Relogin=true
EOF
cat > "$MOUNT_ROOT/usr/local/etc/polkit-1/rules.d/49-northstar-installer-media.rules" <<'EOF'
polkit.addRule(function(action, subject) {
    var installerAction = action.id == "org.northstar.installer.stage"
        || action.id == "org.northstar.installer.execute"
        || action.id == "org.northstar.installer.recovery";
    if (installerAction && subject.local && subject.active
        && subject.user == "northstar-installer") {
        return polkit.Result.YES;
    }
});
EOF
chmod 0444 "$MOUNT_ROOT/usr/local/share/xsessions/northstar-installer.desktop" \
    "$MOUNT_ROOT/usr/local/etc/sddm.conf.d/40-northstar-installer-media.conf" \
    "$MOUNT_ROOT/usr/local/etc/polkit-1/rules.d/49-northstar-installer-media.rules"
printf '%s\n' 'northstar-installer' > "$MOUNT_ROOT/etc/hostname"
cat > "$MOUNT_ROOT/var/db/northstar/installer-media.conf" <<EOF
schema_version=1
project_commit=$project_commit
source_manifest_sha256=$manifest_sha256
source_partition_label=NSTAR_SOURCE
source_mount=/var/run/northstar-installer/source
installed_target_marker=excluded-from-payload
EOF
chmod 0444 "$MOUNT_ROOT/var/db/northstar/installer-media.conf"

umount "$MOUNT_SOURCE"; SOURCE_MOUNTED=0
zfs unmount "$POOL/tmp"; TMP_MOUNTED=0
zfs unmount "$POOL/home"; HOME_MOUNTED=0
zfs unmount "$POOL/ROOT/default"; ROOT_MOUNTED=0
zpool export "$POOL"; POOL_IMPORTED=0
mdconfig -d -u "${MD_DEVICE#md}"; MD_DEVICE=

"$QEMU_IMG" info -f raw "$raw" > "$STAGING/raw-info.txt"
raw_sha256=$(file_sha256 "$raw" | tr '[:upper:]' '[:lower:]'); raw_size=$(file_size "$raw")
printf '%s  %s\n' "$raw_sha256" "$(basename "$raw")" > "$STAGING/$(basename "$raw").sha256"
cat > "$STAGING/media-provenance.conf" <<EOF
schema_version=1
artifact=$(basename "$raw")
artifact_format=raw
artifact_sha256=$raw_sha256
artifact_size=$raw_size
source_qcow2_sha256=$image_sha256
source_image_provenance_sha256=$(file_sha256 "$PROVENANCE" | tr '[:upper:]' '[:lower:]')
source_manifest_sha256=$manifest_sha256
source_signature_sha256=$(file_sha256 "$signature" | tr '[:upper:]' '[:lower:]')
source_trust_key_sha256=$(file_sha256 "$trust_key" | tr '[:upper:]' '[:lower:]')
payload_sha256=$payload_sha256
runtime_manifest_sha256=$runtime_sha256
project_commit=$project_commit
builder_id=$builder_id
builder_marker_sha256=$(file_sha256 "$BUILDER_MARKER" | tr '[:upper:]' '[:lower:]')
firmware=UEFI
partition_table=GPT
installed_root=ZFS
installer_source=UFS-read-only
installer_source_label=NSTAR_SOURCE
host_disk_write=unsupported
EOF
[ "$(file_sha256 "$IMAGE" | tr '[:upper:]' '[:lower:]')" = "$source_before" ] || die 'source QCOW2 changed during media assembly'
chmod 0444 "$raw" "$STAGING/$(basename "$raw").sha256" "$STAGING/media-provenance.conf" \
    "$STAGING/partition-layout.txt" "$STAGING/raw-info.txt"
mv "$STAGING" "$OUTPUT"; STAGING=; SUCCESS=1
printf 'PASS: assembled Northstar raw USB installer media at %s\n' "$OUTPUT"
