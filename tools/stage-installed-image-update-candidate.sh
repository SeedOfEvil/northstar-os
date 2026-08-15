#!/bin/sh

# Stage one immutable signed repository and acceptance driver on an installed
# Northstar image. This prepares trust and pkg inputs but never mutates a
# package or boot environment.

set -eu

EX_USAGE=64
EX_DATAERR=65
EX_UNAVAILABLE=69
EX_NOPERM=77

REPOSITORY=
GATE=
OUTPUT=/home/.northstar-update-candidate
BASELINE_VERSION=
CANDIDATE_VERSION=
IMAGE_COMMIT=
REPOSITORY_REVISION=
CANDIDATE_SOURCE=

fail() {
    status=$1
    shift
    printf 'ERROR: %s\n' "$*" >&2
    exit "$status"
}

usage() {
    cat <<'EOF'
Usage: stage-installed-image-update-candidate.sh \
  --repository DIR --gate FILE --baseline-version V --candidate-version V \
  --image-commit COMMIT --repository-revision N --candidate-source COMMIT \
  [--output /home/DIR]

The command requires root on an installed Northstar image. It verifies and
stages update inputs only; it never runs pkg upgrade or bectl.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --repository) REPOSITORY=${2-}; shift 2 ;;
        --gate) GATE=${2-}; shift 2 ;;
        --output) OUTPUT=${2-}; shift 2 ;;
        --baseline-version) BASELINE_VERSION=${2-}; shift 2 ;;
        --candidate-version) CANDIDATE_VERSION=${2-}; shift 2 ;;
        --image-commit) IMAGE_COMMIT=${2-}; shift 2 ;;
        --repository-revision) REPOSITORY_REVISION=${2-}; shift 2 ;;
        --candidate-source) CANDIDATE_SOURCE=${2-}; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) fail "$EX_USAGE" "unknown option: $1" ;;
    esac
done

[ "$(uname -s)" = FreeBSD ] || fail "$EX_UNAVAILABLE" 'candidate staging requires FreeBSD'
[ "$(id -u)" -eq 0 ] || fail "$EX_NOPERM" 'candidate staging requires root'
for command_name in awk basename cat chmod chown cp find grep install mkdir pkg sha256 stat tr wc; do
    command -v "$command_name" >/dev/null 2>&1 || fail "$EX_UNAVAILABLE" "required command is unavailable: $command_name"
done

case "$OUTPUT" in /home/*) ;; *) fail "$EX_NOPERM" 'candidate staging output must remain beneath /home' ;; esac
[ ! -e "$OUTPUT" ] || fail "$EX_DATAERR" "candidate staging output already exists: $OUTPUT"
[ -d "$REPOSITORY" ] && [ ! -L "$REPOSITORY" ] || fail "$EX_DATAERR" 'repository is missing or unsafe'
[ -f "$GATE" ] && [ ! -L "$GATE" ] || fail "$EX_DATAERR" 'acceptance driver is missing or unsafe'
for version in "$BASELINE_VERSION" "$CANDIDATE_VERSION"; do
    printf '%s\n' "$version" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9_.+~,:-]{0,63}$' \
        || fail "$EX_DATAERR" 'package version is unsafe'
done
for commit in "$IMAGE_COMMIT" "$CANDIDATE_SOURCE"; do
    printf '%s\n' "$commit" | grep -Eq '^[0-9A-Fa-f]{40,64}$' \
        || fail "$EX_DATAERR" 'source commit is not resolved'
done
case "$REPOSITORY_REVISION" in ''|*[!0-9]*) fail "$EX_DATAERR" 'repository revision is not numeric' ;; esac
[ "${#REPOSITORY_REVISION}" -le 9 ] || fail "$EX_DATAERR" 'repository revision is too long'

for artifact in data.pkg meta.conf repository-metadata.json signature.json publication-record.conf; do
    [ -f "$REPOSITORY/$artifact" ] && [ ! -L "$REPOSITORY/$artifact" ] \
        || fail "$EX_DATAERR" "repository artifact is missing or unsafe: $artifact"
done
[ -d "$REPOSITORY/fingerprints/trusted" ] && [ ! -L "$REPOSITORY/fingerprints" ] \
    || fail "$EX_DATAERR" 'repository fingerprint store is missing or unsafe'

record_value() {
    key=$1
    value=$(awk -F= -v key="$key" '$1 == key { if (found++) exit 2; print substr($0, length(key) + 2) }' \
        "$REPOSITORY/publication-record.conf") || fail "$EX_DATAERR" "publication record repeats $key"
    [ -n "$value" ] || fail "$EX_DATAERR" "publication record omits $key"
    printf '%s\n' "$value"
}

CATALOGUE_SHA256=$(record_value catalogue_sha256)
SIGNATURE_FINGERPRINT=$(record_value signature_fingerprint)
[ "$(record_value repository_revision)" = "$REPOSITORY_REVISION" ] \
    || fail "$EX_DATAERR" 'repository revision does not match the requested candidate'
[ "$(record_value source_revision)" = "$CANDIDATE_SOURCE" ] \
    || fail "$EX_DATAERR" 'repository source does not match the requested candidate'
[ "$(sha256 -q "$REPOSITORY/data.pkg")" = "$CATALOGUE_SHA256" ] \
    || fail "$EX_DATAERR" 'repository catalogue digest does not match its publication record'
for digest_value in "$CATALOGUE_SHA256" "$SIGNATURE_FINGERPRINT"; do
    printf '%s\n' "$digest_value" | grep -Eq '^[0-9A-Fa-f]{64}$' \
        || fail "$EX_DATAERR" 'repository digest or fingerprint is unsafe'
done

package_count=$(find "$REPOSITORY" -maxdepth 1 -type f -name 'northstar-*.pkg' -print | wc -l | tr -d ' ')
[ "$package_count" -eq 1 ] || fail "$EX_DATAERR" 'repository must contain exactly one Northstar package'
PACKAGE_PATH=$(find "$REPOSITORY" -maxdepth 1 -type f -name 'northstar-*.pkg' -print)
[ "$(pkg query -F "$PACKAGE_PATH" '%n')" = northstar ] || fail "$EX_DATAERR" 'candidate package name is not northstar'
[ "$(pkg query -F "$PACKAGE_PATH" '%v')" = "$CANDIDATE_VERSION" ] || fail "$EX_DATAERR" 'candidate package version is unexpected'
[ "$(pkg query -e '%n == northstar' '%v')" = "$BASELINE_VERSION" ] || fail "$EX_DATAERR" 'installed Northstar version is not the accepted baseline'

IMAGE_MARKER=/var/db/northstar/image-build.conf
[ -f "$IMAGE_MARKER" ] && [ ! -L "$IMAGE_MARKER" ] || fail "$EX_NOPERM" 'installed-image marker is missing'
actual_image_commit=$(awk -F= '$1 == "project_commit" { if (found++) exit 2; print $2 }' "$IMAGE_MARKER") \
    || fail "$EX_DATAERR" 'installed-image marker repeats project_commit'
[ "$actual_image_commit" = "$IMAGE_COMMIT" ] || fail "$EX_DATAERR" 'installed image is not the accepted baseline commit'

mkdir -m 0700 "$OUTPUT"
mkdir -m 0755 "$OUTPUT/repository" "$OUTPUT/prestage"
cp -Rp "$REPOSITORY/." "$OUTPUT/repository/"
install -m 0500 "$GATE" "$OUTPUT/validate-image-update-rollback.sh"
chown -R root:wheel "$OUTPUT"
find "$OUTPUT" -type d -exec chmod go-w {} \;
find "$OUTPUT" -type f -exec chmod go-w {} \;

POLICY_DIR=/usr/local/etc/northstar
FINGERPRINTS_DIR=/usr/local/etc/pkg/fingerprints/northstar-development
REPOS_DIR=/usr/local/etc/pkg/repos
install -d -m 0755 "$POLICY_DIR" "$FINGERPRINTS_DIR/trusted" "$FINGERPRINTS_DIR/revoked" "$REPOS_DIR"
for existing in \
    "$POLICY_DIR/repository-policy.conf" "$POLICY_DIR/repository-metadata.json" \
    "$POLICY_DIR/signature.json" "$POLICY_DIR/data.pkg" \
    "$REPOS_DIR/northstar-development.conf"; do
    if [ -f "$existing" ] && [ ! -L "$existing" ]; then
        cp -p "$existing" "$OUTPUT/prestage/$(basename "$existing")"
    fi
done

install -m 0644 "$REPOSITORY/repository-metadata.json" "$POLICY_DIR/repository-metadata.json"
install -m 0644 "$REPOSITORY/signature.json" "$POLICY_DIR/signature.json"
install -m 0644 "$REPOSITORY/data.pkg" "$POLICY_DIR/data.pkg"
cp -Rp "$REPOSITORY/fingerprints/trusted/." "$FINGERPRINTS_DIR/trusted/"
cp -Rp "$REPOSITORY/fingerprints/revoked/." "$FINGERPRINTS_DIR/revoked/"
chown -R root:wheel "$FINGERPRINTS_DIR"
find "$FINGERPRINTS_DIR" -type d -exec chmod 0755 {} \;
find "$FINGERPRINTS_DIR" -type f -exec chmod 0644 {} \;

cat > "$POLICY_DIR/repository-policy.conf" <<EOF
channel=development
repository_tag=northstar-development
repository_name=Northstar Development
repository_url=pkg+https://packages.northstar.invalid/development
mirror_type=none
signature_type=fingerprints
fingerprints_path=$FINGERPRINTS_DIR
trust_mode=required
EOF
chmod 0644 "$POLICY_DIR/repository-policy.conf"

cat > "$REPOS_DIR/northstar-development.conf" <<EOF
northstar-development: {
    url: "file://$OUTPUT/repository",
    mirror_type: "none",
    signature_type: "FINGERPRINTS",
    fingerprints: "$FINGERPRINTS_DIR",
    enabled: yes
}
EOF
chmod 0644 "$REPOS_DIR/northstar-development.conf"

pkg -o ASSUME_ALWAYS_YES=yes update -f -r northstar-development
available=$(pkg rquery -r northstar-development -e '%n == northstar' '%v')
[ "$available" = "$CANDIDATE_VERSION" ] || fail "$EX_DATAERR" 'trusted pkg client did not expose the expected candidate'

PACKAGE_SHA256=$(sha256 -q "$PACKAGE_PATH")
cat > "$OUTPUT/candidate.conf" <<EOF
schema_version=1
baseline_version=$BASELINE_VERSION
candidate_version=$CANDIDATE_VERSION
image_commit=$IMAGE_COMMIT
repository_revision=$REPOSITORY_REVISION
candidate_source_revision=$CANDIDATE_SOURCE
catalogue_sha256=$CATALOGUE_SHA256
signature_fingerprint=$SIGNATURE_FINGERPRINT
package_sha256=$PACKAGE_SHA256
EOF
chmod 0600 "$OUTPUT/candidate.conf"

printf '%s\n' \
    'CANDIDATE_STAGED=yes' \
    "BASELINE_VERSION=$BASELINE_VERSION" \
    "CANDIDATE_VERSION=$CANDIDATE_VERSION" \
    "IMAGE_COMMIT=$IMAGE_COMMIT" \
    "REPOSITORY_REVISION=$REPOSITORY_REVISION" \
    "CANDIDATE_SOURCE_REVISION=$CANDIDATE_SOURCE" \
    "CATALOGUE_SHA256=$CATALOGUE_SHA256" \
    "SIGNATURE_FINGERPRINT=$SIGNATURE_FINGERPRINT" \
    "PACKAGE_SHA256=$PACKAGE_SHA256" \
    'MUTATION=none'
