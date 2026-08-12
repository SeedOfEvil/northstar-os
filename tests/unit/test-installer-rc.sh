#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
DRIVER=$ROOT/image/scripts/assemble-installer-rc.sh
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-installer-rc.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM
fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
contains() { grep -F -- "$2" "$ROOT/$1" >/dev/null 2>&1 || fail "$1 missing: $2"; }

[ -x "$DRIVER" ] || fail 'installer RC driver is not executable'
mkdir "$TMP_DIR/project" "$TMP_DIR/resolved" "$TMP_DIR/artifacts" "$TMP_DIR/runtime" "$TMP_DIR/keys"
git -C "$TMP_DIR/project" init -q
git -C "$TMP_DIR/project" config user.email northstar-tests@localhost.invalid
git -C "$TMP_DIR/project" config user.name 'Northstar tests'
printf '%s\n' fixture > "$TMP_DIR/project/README"
git -C "$TMP_DIR/project" add README
git -C "$TMP_DIR/project" commit -q -m fixture
commit=$(git -C "$TMP_DIR/project" rev-parse HEAD)
short_commit=$(printf '%s' "$commit" | cut -c1-12)
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$TMP_DIR/keys/source-private.pem" >/dev/null 2>&1
events=$TMP_DIR/events

cat > "$TMP_DIR/fake-image" <<'EOF'
#!/bin/sh
set -eu
output=
preflight=0
commit=
while [ "$#" -gt 0 ]; do
    case "$1" in
        --output) output=$2; shift 2 ;;
        --project-commit) commit=$2; shift 2 ;;
        --preflight) preflight=1; shift ;;
        *) shift ;;
    esac
done
printf '%s\n' image >> "$NORTHSTAR_RC_TEST_EVENTS"
[ "$preflight" -eq 0 ] || exit 0
mkdir "$output"
short=$(printf '%s' "$commit" | cut -c1-12)
printf '%s\n' qcow2 > "$output/northstar-15.1-amd64.qcow2"
printf '%s\n' provenance > "$output/image-provenance.conf"
printf '%s\n' payload > "$output/northstar-rootfs-v1-$short.txz"
printf '%s\n' runtime > "$output/runtime-manifest.conf"
EOF
cat > "$TMP_DIR/fake-source" <<'EOF'
#!/bin/sh
set -eu
output=
while [ "$#" -gt 0 ]; do case "$1" in --output) output=$2; shift 2 ;; *) shift ;; esac; done
printf '%s\n' source >> "$NORTHSTAR_RC_TEST_EVENTS"
mkdir "$output"
printf '%s\n' manifest > "$output/source-manifest.conf"
printf '%s\n' signature > "$output/source-manifest.conf.sig"
EOF
cat > "$TMP_DIR/fake-media" <<'EOF'
#!/bin/sh
set -eu
output=
while [ "$#" -gt 0 ]; do case "$1" in --output) output=$2; shift 2 ;; *) shift ;; esac; done
printf '%s\n' media >> "$NORTHSTAR_RC_TEST_EVENTS"
[ "${NORTHSTAR_RC_TEST_FAIL_MEDIA:-0}" != 1 ] || exit 70
mkdir "$output"
printf '%s\n' raw > "$output/northstar-15.1-amd64-installer-usb.img"
printf '%s\n' provenance > "$output/media-provenance.conf"
EOF
chmod 700 "$TMP_DIR/fake-image" "$TMP_DIR/fake-source" "$TMP_DIR/fake-media"

export NORTHSTAR_INSTALLER_RC_TEST_MODE=1
export NORTHSTAR_INSTALLER_RC_IMAGE_ASSEMBLER=$TMP_DIR/fake-image
export NORTHSTAR_INSTALLER_RC_SOURCE_PREPARER=$TMP_DIR/fake-source
export NORTHSTAR_INSTALLER_RC_MEDIA_ASSEMBLER=$TMP_DIR/fake-media
export NORTHSTAR_RC_TEST_EVENTS=$events

"$DRIVER" --resolved-inputs "$TMP_DIR/resolved" --artifacts "$TMP_DIR/artifacts" \
    --runtime-bundle "$TMP_DIR/runtime" --project-root "$TMP_DIR/project" \
    --project-commit "$commit" --signing-key "$TMP_DIR/keys/source-private.pem" \
    --output "$TMP_DIR/preflight-output" --preflight >/dev/null
[ ! -e "$TMP_DIR/preflight-output" ] || fail 'RC preflight created output'
[ "$(cat "$events")" = image ] || fail 'RC preflight invoked a mutating child stage'
: > "$events"

"$DRIVER" --resolved-inputs "$TMP_DIR/resolved" --artifacts "$TMP_DIR/artifacts" \
    --runtime-bundle "$TMP_DIR/runtime" --project-root "$TMP_DIR/project" \
    --project-commit "$commit" --signing-key "$TMP_DIR/keys/source-private.pem" \
    --output "$TMP_DIR/release-candidate" >/dev/null
[ "$(tr '\n' ' ' < "$events")" = 'image source media ' ] || fail 'RC child stages ran out of order'
[ -f "$TMP_DIR/release-candidate/release-candidate.conf" ] || fail 'RC top-level provenance is missing'
grep -Fx "project_commit=$commit" "$TMP_DIR/release-candidate/release-candidate.conf" >/dev/null \
    || fail 'RC provenance omits its project commit'
grep -Fx 'host_disk_write=unsupported' "$TMP_DIR/release-candidate/release-candidate.conf" >/dev/null \
    || fail 'RC provenance does not reject host-disk writing'
[ -f "$TMP_DIR/release-candidate/image/northstar-rootfs-v1-$short_commit.txz" ] \
    || fail 'RC rootfs payload is missing'

export NORTHSTAR_RC_TEST_FAIL_MEDIA=1
if "$DRIVER" --resolved-inputs "$TMP_DIR/resolved" --artifacts "$TMP_DIR/artifacts" \
    --runtime-bundle "$TMP_DIR/runtime" --project-root "$TMP_DIR/project" \
    --project-commit "$commit" --signing-key "$TMP_DIR/keys/source-private.pem" \
    --output "$TMP_DIR/failed-release" >/dev/null 2>&1; then
    fail 'RC assembly accepted a failed media stage'
fi
unset NORTHSTAR_RC_TEST_FAIL_MEDIA
[ ! -e "$TMP_DIR/failed-release" ] || fail 'failed RC assembly published output'
if find "$TMP_DIR" -maxdepth 1 -type d -name '.northstar-installer-rc.*' | grep . >/dev/null 2>&1; then
    fail 'failed RC assembly left staging state'
fi

cp "$TMP_DIR/keys/source-private.pem" "$TMP_DIR/project/private.pem"
git -C "$TMP_DIR/project" add private.pem
git -C "$TMP_DIR/project" commit -q -m unsafe
unsafe_commit=$(git -C "$TMP_DIR/project" rev-parse HEAD)
if "$DRIVER" --resolved-inputs "$TMP_DIR/resolved" --artifacts "$TMP_DIR/artifacts" \
    --runtime-bundle "$TMP_DIR/runtime" --project-root "$TMP_DIR/project" \
    --project-commit "$unsafe_commit" --signing-key "$TMP_DIR/project/private.pem" \
    --output "$TMP_DIR/unsafe" --preflight >/dev/null 2>&1; then
    fail 'project-internal RC signing key was accepted'
fi

contains image/scripts/assemble-installer-rc.sh 'no host disk device is accepted'
contains image/scripts/assemble-installer-rc.sh 'installer RC assembly must run as root on a disposable builder'
contains image/scripts/assemble-installer-rc.sh 'disposable-image-builder.conf'
contains image/scripts/assemble-installer-rc.sh 'disposable-installer-media-builder.conf'
contains image/scripts/assemble-installer-rc.sh 'host_disk_write=unsupported'
printf '%s\n' 'PASS: integrated installer RC orchestration is ordered, provenance-bound, and host-disk-free'
