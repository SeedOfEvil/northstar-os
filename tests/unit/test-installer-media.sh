#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
PREPARE=$ROOT/image/scripts/prepare-installer-source.sh
ASSEMBLE=$ROOT/image/scripts/assemble-installer-usb.sh
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-installer-media.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
contains() { grep -F -- "$2" "$ROOT/$1" >/dev/null 2>&1 || fail "$1 missing: $2"; }
file_sha256() {
    if command -v sha256 >/dev/null 2>&1; then sha256 -q "$1"; else sha256sum "$1" | awk '{ print $1 }'; fi
}
file_size() { wc -c < "$1" | tr -d ' '; }

[ -x "$PREPARE" ] || fail 'installer source preparer is not executable'
[ -x "$ASSEMBLE" ] || fail 'installer USB assembler is not executable'
for command_name in git openssl tar; do command -v "$command_name" >/dev/null 2>&1 || fail "$command_name is required"; done

mkdir "$TMP_DIR/project" "$TMP_DIR/rootfs" "$TMP_DIR/rootfs/var" "$TMP_DIR/rootfs/var/db" \
    "$TMP_DIR/rootfs/var/db/northstar" "$TMP_DIR/keys" "$TMP_DIR/output"
git -C "$TMP_DIR/project" init -q
git -C "$TMP_DIR/project" config user.email northstar-tests@localhost.invalid
git -C "$TMP_DIR/project" config user.name 'Northstar tests'
printf '%s\n' fixture > "$TMP_DIR/project/README"
git -C "$TMP_DIR/project" add README
git -C "$TMP_DIR/project" commit -q -m fixture
commit=$(git -C "$TMP_DIR/project" rev-parse HEAD)
cat > "$TMP_DIR/runtime-manifest.conf" <<EOF
schema_version=1
product=Northstar
freebsd_release=15.1-RELEASE
architecture=amd64
project_commit=$commit
runtime_package_records_sha256=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
runtime_package_count=2
EOF
cp "$TMP_DIR/runtime-manifest.conf" "$TMP_DIR/rootfs/var/db/northstar/runtime-manifest.conf"
payload=$TMP_DIR/northstar-rootfs-v1-$(printf '%s' "$commit" | cut -c1-12).txz
tar -cJf "$payload" -C "$TMP_DIR/rootfs" .
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$TMP_DIR/keys/source-private.pem" >/dev/null 2>&1

"$PREPARE" --payload "$payload" --runtime-manifest "$TMP_DIR/runtime-manifest.conf" \
    --project-root "$TMP_DIR/project" --project-commit "$commit" \
    --signing-key "$TMP_DIR/keys/source-private.pem" --output "$TMP_DIR/output/source" >/dev/null
[ -f "$TMP_DIR/output/source/source-manifest.conf" ] || fail 'signed source manifest was not emitted'
[ -f "$TMP_DIR/output/source/source-manifest.conf.sig" ] || fail 'detached source signature was not emitted'
[ -f "$TMP_DIR/output/source/source-signing.pem" ] || fail 'public trust key was not emitted'
[ ! -f "$TMP_DIR/output/source/source-private.pem" ] || fail 'private key leaked into the installer source'
openssl dgst -sha256 -verify "$TMP_DIR/output/source/source-signing.pem" \
    -signature "$TMP_DIR/output/source/source-manifest.conf.sig" \
    "$TMP_DIR/output/source/source-manifest.conf" >/dev/null 2>&1 || fail 'emitted signature does not verify'
grep -Fx 'private_key_included=no' "$TMP_DIR/output/source/installer-source-provenance.conf" >/dev/null \
    || fail 'source provenance does not exclude the private key'

cp "$TMP_DIR/runtime-manifest.conf" "$TMP_DIR/runtime-manifest-mismatch.conf"
printf '%s\n' 'unexpected=field' >> "$TMP_DIR/runtime-manifest-mismatch.conf"
if "$PREPARE" --payload "$payload" --runtime-manifest "$TMP_DIR/runtime-manifest-mismatch.conf" \
    --project-root "$TMP_DIR/project" --project-commit "$commit" \
    --signing-key "$TMP_DIR/keys/source-private.pem" --output "$TMP_DIR/output/mismatch" >/dev/null 2>&1; then
    fail 'runtime manifest differing from the payload was accepted'
fi

cp "$TMP_DIR/keys/source-private.pem" "$TMP_DIR/project/inside-project.pem"
git -C "$TMP_DIR/project" add inside-project.pem
git -C "$TMP_DIR/project" commit -q -m key-fixture
inside_commit=$(git -C "$TMP_DIR/project" rev-parse HEAD)
if "$PREPARE" --payload "$payload" --runtime-manifest "$TMP_DIR/runtime-manifest.conf" \
    --project-root "$TMP_DIR/project" --project-commit "$inside_commit" \
    --signing-key "$TMP_DIR/project/inside-project.pem" --output "$TMP_DIR/output/unsafe" >/dev/null 2>&1; then
    fail 'private signing key inside the project was accepted'
fi
rm "$TMP_DIR/project/inside-project.pem"
git -C "$TMP_DIR/project" reset -q --hard HEAD~1

printf 'fixture-qcow2-%s\n' "$commit" > "$TMP_DIR/source.qcow2"
image_sha=$(file_sha256 "$TMP_DIR/source.qcow2")
image_size=$(file_size "$TMP_DIR/source.qcow2")
cat > "$TMP_DIR/image-provenance.conf" <<EOF
schema_version=1
artifact=northstar-15.1-amd64.qcow2
artifact_sha256=$image_sha
artifact_size=$image_size
virtual_size_gib=16
firmware=UEFI
partition_table=GPT
root_filesystem=ZFS
zpool=nstar_$(printf '%s' "$commit" | cut -c1-12)
project_commit=$commit
builder_id=nstar-fixture
builder_marker_sha256=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
resolved_inputs_sha256=cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
runtime_package_records_sha256=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
runtime_package_count=2
installer_payload=$(basename "$payload")
installer_payload_sha256=$(file_sha256 "$payload")
installer_payload_size=$(file_size "$payload")
development_autologin=0
EOF
cat > "$TMP_DIR/fake-qemu-img" <<'EOF'
#!/bin/sh
case "$1" in
  check) exit 0 ;;
  info) printf '%s\n' 'image: fixture' 'file format: qcow2' 'virtual size: 16 GiB (17179869184 bytes)' ;;
  *) exit 64 ;;
esac
EOF
chmod 700 "$TMP_DIR/fake-qemu-img"
NORTHSTAR_INSTALLER_MEDIA_TEST_MODE=1 NORTHSTAR_INSTALLER_MEDIA_QEMU_IMG="$TMP_DIR/fake-qemu-img" \
    "$ASSEMBLE" --image "$TMP_DIR/source.qcow2" --image-provenance "$TMP_DIR/image-provenance.conf" \
    --installer-source "$TMP_DIR/output/source" --output "$TMP_DIR/output/media" --preflight >/dev/null
[ ! -e "$TMP_DIR/output/media" ] || fail 'preflight created installer media output'

cp -R "$TMP_DIR/output/source" "$TMP_DIR/output/source-unknown-field"
chmod u+w "$TMP_DIR/output/source-unknown-field/installer-source-provenance.conf"
printf '%s\n' 'unexpected=field' >> "$TMP_DIR/output/source-unknown-field/installer-source-provenance.conf"
if NORTHSTAR_INSTALLER_MEDIA_TEST_MODE=1 NORTHSTAR_INSTALLER_MEDIA_QEMU_IMG="$TMP_DIR/fake-qemu-img" \
    "$ASSEMBLE" --image "$TMP_DIR/source.qcow2" --image-provenance "$TMP_DIR/image-provenance.conf" \
    --installer-source "$TMP_DIR/output/source-unknown-field" --output "$TMP_DIR/output/unsafe-media" --preflight >/dev/null 2>&1; then
    fail 'unknown installer-source provenance field was accepted'
fi

sed 's/development_autologin=0/development_autologin=1/' "$TMP_DIR/image-provenance.conf" \
    > "$TMP_DIR/development-image-provenance.conf"
if NORTHSTAR_INSTALLER_MEDIA_TEST_MODE=1 NORTHSTAR_INSTALLER_MEDIA_QEMU_IMG="$TMP_DIR/fake-qemu-img" \
    "$ASSEMBLE" --image "$TMP_DIR/source.qcow2" --image-provenance "$TMP_DIR/development-image-provenance.conf" \
    --installer-source "$TMP_DIR/output/source" --output "$TMP_DIR/output/development-media" --preflight >/dev/null 2>&1; then
    fail 'development-autologin QCOW2 was accepted as installer media'
fi

printf '%s\n' tamper >> "$TMP_DIR/source.qcow2"
if NORTHSTAR_INSTALLER_MEDIA_TEST_MODE=1 NORTHSTAR_INSTALLER_MEDIA_QEMU_IMG="$TMP_DIR/fake-qemu-img" \
    "$ASSEMBLE" --image "$TMP_DIR/source.qcow2" --image-provenance "$TMP_DIR/image-provenance.conf" \
    --installer-source "$TMP_DIR/output/source" --output "$TMP_DIR/output/media" --preflight >/dev/null 2>&1; then
    fail 'tampered source QCOW2 was accepted'
fi

contains image/scripts/assemble-installer-usb.sh 'Production accepts no disk device'
contains image/scripts/assemble-installer-usb.sh 'installer USB assembly must run as root on a disposable builder'
contains image/scripts/assemble-installer-usb.sh 'source_provider_output=$(gpart add -a 1m -t freebsd-ufs -l NSTAR_SOURCE "$MD_DEVICE")'
contains image/scripts/assemble-installer-usb.sh 'SOURCE_PROVIDER=$(printf '\''%s\n'\'' "$source_provider_output" | awk '\''NR == 1 { print $1 }'\'')'
contains image/scripts/assemble-installer-usb.sh "printf '%s\\n' \"\$SOURCE_PROVIDER\" | grep -Eq '^md[0-9]+p[0-9]+\$'"
contains image/scripts/assemble-installer-usb.sh '/dev/ufs/NSTAR_SOURCE /var/run/northstar-installer/source ufs ro,noatime 0 0'
contains image/scripts/assemble-installer-usb.sh 'installer media must disable inherited EFI automount'
contains image/scripts/assemble-installer-usb.sh 'source QCOW2 changed during media assembly'
contains image/scripts/assemble-installer-usb.sh 'host_disk_write=unsupported'
contains image/scripts/assemble-installer-usb.sh 'subject.user == "northstar-installer"'
contains image/scripts/assemble-installer-usb.sh 'usr/local/share/northstar/image-sessions/northstar-installer.desktop'
contains image/scripts/assemble-installer-usb.sh 'installed_target_marker=excluded-from-payload'
contains image/scripts/assemble-qcow2-image.sh 'runtime-manifest.conf'
contains image/scripts/assemble-qcow2-image.sh 'northstar-rootfs-v1-$(printf '\''%s'\'' "$PROJECT_COMMIT" | cut -c1-12).txz'
contains image/scripts/assemble-qcow2-image.sh "--exclude './boot/efi' --exclude './dev' --exclude './home' --exclude './tmp'"

printf '%s\n' 'PASS: signed installer source and raw USB media contracts are immutable and disk-device-free'
