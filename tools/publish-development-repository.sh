#!/bin/sh

# Build a signed Northstar development pkg repository from immutable package
# artifacts. Private signing material is owned by caller-provided executables
# and is never accepted as an argument or copied into the publication.

set -eu

PACKAGES_DIR=
OUTPUT_DIR=
PKG_SIGNER=
PUBLICATION_SIGNER=
PUBLIC_KEY=
SOURCE_LOCK=
REVISION=
SOURCE_REVISION=
GENERATED_AT=
REPOSITORY_URL=
FINGERPRINTS_PATH=
STAGING_DIR=

usage() {
    cat <<USAGE
Usage: $0 --packages DIR --output DIR --pkg-signer FILE \\
  --publication-signer FILE --public-key FILE --source-lock FILE \\
  --revision NUMBER --source-revision COMMIT --generated-at ISO8601 \\
  --repository-url pkg+https://HOST/PATH --fingerprints-path /ABSOLUTE/PATH

The pkg signer implements pkg repo's signing_command protocol. The publication
signer is invoked as SIGNER PAYLOAD_FILE SIGNATURE_FILE. Both signers and all
private key material must remain outside the source tree and output directory.
The output directory must not already exist.
USAGE
}

die() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

cleanup() {
    if [ -n "$STAGING_DIR" ] && [ -d "$STAGING_DIR" ]; then
        rm -rf "$STAGING_DIR"
    fi
}
trap cleanup EXIT HUP INT TERM

while [ "$#" -gt 0 ]; do
    case "$1" in
        --packages) PACKAGES_DIR=${2-}; shift 2 ;;
        --output) OUTPUT_DIR=${2-}; shift 2 ;;
        --pkg-signer) PKG_SIGNER=${2-}; shift 2 ;;
        --publication-signer) PUBLICATION_SIGNER=${2-}; shift 2 ;;
        --public-key) PUBLIC_KEY=${2-}; shift 2 ;;
        --source-lock) SOURCE_LOCK=${2-}; shift 2 ;;
        --revision) REVISION=${2-}; shift 2 ;;
        --source-revision) SOURCE_REVISION=${2-}; shift 2 ;;
        --generated-at) GENERATED_AT=${2-}; shift 2 ;;
        --repository-url) REPOSITORY_URL=${2-}; shift 2 ;;
        --fingerprints-path) FINGERPRINTS_PATH=${2-}; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

[ "$(uname -s)" = FreeBSD ] || die 'development repository publication requires FreeBSD'
for command_name in awk base64 cp date dirname find grep mkdir mktemp mv openssl pkg sed sha256 sort tar tr wc; do
    command -v "$command_name" >/dev/null 2>&1 || die "required command is unavailable: $command_name"
done

[ -d "$PACKAGES_DIR" ] || die 'package input directory does not exist'
[ -n "$OUTPUT_DIR" ] || die 'output directory is required'
[ ! -e "$OUTPUT_DIR" ] || die 'output directory already exists'
[ -x "$PKG_SIGNER" ] || die 'pkg signer must be an executable file'
[ -x "$PUBLICATION_SIGNER" ] || die 'publication signer must be an executable file'
[ -f "$PUBLIC_KEY" ] && [ ! -L "$PUBLIC_KEY" ] || die 'public key must be a regular non-symlink file'
[ -f "$SOURCE_LOCK" ] && [ ! -L "$SOURCE_LOCK" ] || die 'source lock must be a regular non-symlink file'
case "$REVISION" in ''|*[!0-9]*) die 'revision must be a non-negative integer' ;; esac
[ "${#REVISION}" -le 9 ] || die 'revision is too long'
printf '%s\n' "$SOURCE_REVISION" | grep -Eq '^[0-9A-Fa-f]{7,64}$' \
    || die 'source revision must be a resolved commit identifier'
printf '%s\n' "$GENERATED_AT" | grep -Eq '^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$' \
    || die 'generated-at must be a UTC ISO-8601 timestamp'
date -j -u -f '%Y-%m-%dT%H:%M:%SZ' "$GENERATED_AT" '+%Y-%m-%dT%H:%M:%SZ' >/dev/null 2>&1 \
    || die 'generated-at is not a valid UTC timestamp'
printf '%s\n' "$REPOSITORY_URL" | grep -Eq '^pkg\+https://[A-Za-z0-9.-]+(:[0-9]+)?(/[A-Za-z0-9._~:/?&=+-]*)?$' \
    || die 'repository URL must be a safe pkg+https URL'
case "$FINGERPRINTS_PATH" in /*) ;; *) die 'fingerprints path must be absolute' ;; esac
printf '%s\n' "$FINGERPRINTS_PATH" | grep -Eq '^[A-Za-z0-9_./+-]+$' \
    || die 'fingerprints path contains unsafe characters'

for unresolved in UNSET RESOLVED_BY_BUILDER GENERATED_AT_BUILD_TIME; do
    if grep -F "$unresolved" "$SOURCE_LOCK" >/dev/null 2>&1; then
        die "source lock contains unresolved value: $unresolved"
    fi
done

lock_value() {
    key=$1
    value=$(awk -F= -v wanted="$key" '$1 == wanted { if (found++) exit 2; print substr($0, index($0, "=") + 1) }' "$SOURCE_LOCK") \
        || die "source lock key is duplicated: $key"
    [ -n "$value" ] || die "source lock key is missing: $key"
    printf '%s' "$value"
}

FREEBSD_RELEASE=$(lock_value FREEBSD_RELEASE)
FREEBSD_ARCH=$(lock_value FREEBSD_ARCH)
PORTS_BRANCH=$(lock_value PORTS_BRANCH)
PORTS_COMMIT=$(lock_value PORTS_COMMIT)
QT_PACKAGE_VERSION=$(lock_value QT_PACKAGE_VERSION)
WAYFIRE_PACKAGE_VERSION=$(lock_value WAYFIRE_PACKAGE_VERSION)
PROJECT_COMMIT=$(lock_value PROJECT_COMMIT)

[ "$PROJECT_COMMIT" = "$SOURCE_REVISION" ] || die 'source revision does not match PROJECT_COMMIT in source lock'
printf '%s\n' "$PORTS_COMMIT" | grep -Eq '^[0-9A-Fa-f]{7,64}$' || die 'PORTS_COMMIT is not pinned'
printf '%s\n' "$FREEBSD_RELEASE" | grep -Eq '^[0-9]+\.[0-9]+-RELEASE(-p[0-9]+)?$' || die 'FREEBSD_RELEASE is unsafe'
printf '%s\n' "$FREEBSD_ARCH" | grep -Eq '^[A-Za-z0-9_]+$' || die 'FREEBSD_ARCH is unsafe'
for value in "$PORTS_BRANCH" "$QT_PACKAGE_VERSION" "$WAYFIRE_PACKAGE_VERSION"; do
    printf '%s\n' "$value" | grep -Eq '^[A-Za-z0-9_.+~-]+$' || die 'source lock contains an unsafe pinned value'
done

PACKAGE_COUNT=$(find "$PACKAGES_DIR" -type f -name '*.pkg' -print | wc -l | tr -d ' ')
[ "$PACKAGE_COUNT" -gt 0 ] || die 'no .pkg package artifacts were supplied'

OUTPUT_PARENT=$(dirname "$OUTPUT_DIR")
mkdir -p "$OUTPUT_PARENT"
STAGING_DIR=$(mktemp -d "$OUTPUT_PARENT/.northstar-development.XXXXXX")
mkdir -p "$STAGING_DIR/fingerprints/trusted" "$STAGING_DIR/fingerprints/revoked"

pkg repo -q -o "$STAGING_DIR" "$PACKAGES_DIR" signing_command: "$PKG_SIGNER"
[ -s "$STAGING_DIR/meta.conf" ] || die 'pkg repo did not produce meta.conf'
[ -s "$STAGING_DIR/data.pkg" ] || die 'pkg repo did not produce data.pkg'
catalogue_entries=$(tar -tf "$STAGING_DIR/data.pkg")
for entry in data data.sig data.pub; do
    printf '%s\n' "$catalogue_entries" | grep -Fx "$entry" >/dev/null 2>&1 \
        || die "signed catalogue is missing member: $entry"
done

find "$PACKAGES_DIR" -type f -name '*.pkg' -print | sort |
    while IFS= read -r package_path; do
        cp -p "$package_path" "$STAGING_DIR/"
    done

PUBLIC_KEY_TEXT=$(sed -e 's/[[:space:]]*$//' "$PUBLIC_KEY")
printf '%s\n' "$PUBLIC_KEY_TEXT" | grep -F 'BEGIN PUBLIC KEY' >/dev/null 2>&1 \
    || die 'public key is not PEM public-key material'
printf '%s' "$PUBLIC_KEY_TEXT" > "$STAGING_DIR/public-key.pem"
FINGERPRINT=$(sha256 -q "$STAGING_DIR/public-key.pem")
printf '%s\n' 'function: sha256' "fingerprint: $FINGERPRINT" \
    > "$STAGING_DIR/fingerprints/trusted/northstar-development"

CATALOGUE_SHA256=$(sha256 -q "$STAGING_DIR/data.pkg")

PACKAGE_ROWS="$STAGING_DIR/package-rows"
: > "$PACKAGE_ROWS"
find "$PACKAGES_DIR" -type f -name '*.pkg' -print | sort |
    while IFS= read -r package_path; do
        row=$(pkg query -F "$package_path" '%n|%v|%o') || exit 20
        [ "$(printf '%s\n' "$row" | wc -l | tr -d ' ')" = 1 ] || exit 21
        package_name=$(printf '%s' "$row" | awk -F'|' '{print $1}')
        package_version=$(printf '%s' "$row" | awk -F'|' '{print $2}')
        package_origin=$(printf '%s' "$row" | awk -F'|' '{print $3}')
        printf '%s\n' "$package_name" | grep -Eq '^[a-z0-9][a-z0-9+_.-]*$' || exit 22
        printf '%s\n' "$package_version" | grep -Eq '^[A-Za-z0-9_.+~,:-]+$' || exit 23
        printf '%s\n' "$package_origin" | grep -Eq '^[A-Za-z0-9_.+~-]+/[A-Za-z0-9_.+~-]+$' || exit 24
        printf '%s|%s|%s\n' "$package_name" "$package_version" "$package_origin" >> "$PACKAGE_ROWS"
    done || die 'package metadata is missing or unsafe'
grep -Eq '^northstar\|' "$PACKAGE_ROWS" || die 'publication must contain the Northstar package'
awk -F'|' 'seen[$1]++ { exit 1 }' "$PACKAGE_ROWS" \
    || die 'publication contains duplicate package names'

ABI_MAJOR=$(printf '%s' "$FREEBSD_RELEASE" | awk -F. '{print $1}')
ABI="FreeBSD:$ABI_MAJOR:$FREEBSD_ARCH"
METADATA="$STAGING_DIR/repository-metadata.json"
{
    printf '%s\n' '{'
    printf '%s\n' '  "schema_version": 1,'
    printf '%s\n' '  "repository_tag": "northstar-development",'
    printf '%s\n' '  "channel": "development",'
    printf '  "abi": "%s",\n' "$ABI"
    printf '  "revision": %s,\n' "$REVISION"
    printf '  "generated_at": "%s",\n' "$GENERATED_AT"
    printf '  "source_revision": "%s",\n' "$SOURCE_REVISION"
    printf '%s\n' '  "signature_status": "verified",'
    printf '  "signature_fingerprint": "%s",\n' "$FINGERPRINT"
    printf '%s\n' '  "signature_envelope": "signature.json",'
    printf '%s\n' '  "catalogue_file": "data.pkg",'
    printf '  "catalogue_sha256": "%s",\n' "$CATALOGUE_SHA256"
    printf '%s\n' '  "packages": ['
    row_number=0
    row_count=$(wc -l < "$PACKAGE_ROWS" | tr -d ' ')
    while IFS='|' read -r package_name package_version package_origin; do
        row_number=$((row_number + 1))
        comma=,
        [ "$row_number" -eq "$row_count" ] && comma=
        printf '    {"name":"%s","version":"%s","origin":"%s","source":"cpack/northstar","project_revision":"%s"}%s\n' \
            "$package_name" "$package_version" "$package_origin" "$SOURCE_REVISION" "$comma"
    done < "$PACKAGE_ROWS"
    printf '%s\n' '  ]'
    printf '%s\n' '}'
} > "$METADATA"

METADATA_SHA256=$(sha256 -q "$METADATA")
printf '%s' "$METADATA_SHA256" > "$STAGING_DIR/publication-payload"
"$PUBLICATION_SIGNER" "$STAGING_DIR/publication-payload" "$STAGING_DIR/publication-signature.bin" \
    || die 'external publication signer failed'
[ -s "$STAGING_DIR/publication-signature.bin" ] || die 'publication signer produced no signature'
openssl dgst -sha256 -verify "$STAGING_DIR/public-key.pem" \
    -signature "$STAGING_DIR/publication-signature.bin" "$STAGING_DIR/publication-payload" >/dev/null 2>&1 \
    || die 'public key did not verify the publication signature'

PUBLIC_KEY_JSON=$(awk 'BEGIN { first=1 } { gsub(/\\/, "\\\\"); gsub(/\"/, "\\\""); if (!first) printf "\\n"; printf "%s", $0; first=0 }' "$STAGING_DIR/public-key.pem")
SIGNATURE_BASE64=$(base64 < "$STAGING_DIR/publication-signature.bin" | tr -d '\n')
{
    printf '%s\n' '{'
    printf '%s\n' '  "schema_version": 2,'
    printf '%s\n' '  "type": "rsa",'
    printf '%s\n' '  "payload_type": "repository-metadata-sha256",'
    printf '  "payload": "%s",\n' "$METADATA_SHA256"
    printf '  "public_key_pem": "%s",\n' "$PUBLIC_KEY_JSON"
    printf '  "signature_base64": "%s",\n' "$SIGNATURE_BASE64"
    printf '  "fingerprint_sha256": "%s"\n' "$FINGERPRINT"
    printf '%s\n' '}'
} > "$STAGING_DIR/signature.json"

SOURCE_LOCK_SHA256=$(sha256 -q "$SOURCE_LOCK")
printf '%s\n' \
    'schema_version=1' \
    'channel=development' \
    'repository_tag=northstar-development' \
    "repository_revision=$REVISION" \
    "generated_at=$GENERATED_AT" \
    "source_revision=$SOURCE_REVISION" \
    "source_lock_sha256=$SOURCE_LOCK_SHA256" \
    "metadata_sha256=$METADATA_SHA256" \
    "catalogue_sha256=$CATALOGUE_SHA256" \
    "signature_fingerprint=$FINGERPRINT" \
    "ports_branch=$PORTS_BRANCH" \
    "ports_commit=$PORTS_COMMIT" \
    "qt_package_version=$QT_PACKAGE_VERSION" \
    "wayfire_package_version=$WAYFIRE_PACKAGE_VERSION" \
    > "$STAGING_DIR/publication-record.conf"

printf '%s\n' \
    'channel=development' \
    'repository_tag=northstar-development' \
    'repository_name=Northstar Development' \
    "repository_url=$REPOSITORY_URL" \
    'mirror_type=none' \
    'signature_type=fingerprints' \
    "fingerprints_path=$FINGERPRINTS_PATH" \
    'trust_mode=required' \
    > "$STAGING_DIR/repository-policy.conf"

printf '%s\n' \
    'northstar-development: {' \
    "    url: \"$REPOSITORY_URL\"," \
    '    mirror_type: "none",' \
    '    signature_type: "FINGERPRINTS",' \
    "    fingerprints: \"$FINGERPRINTS_PATH\"," \
    '    enabled: yes' \
    '}' \
    > "$STAGING_DIR/northstar-development.conf"

rm -f "$STAGING_DIR/public-key.pem" "$STAGING_DIR/publication-payload" \
    "$STAGING_DIR/publication-signature.bin" "$STAGING_DIR/package-rows"
if find "$STAGING_DIR" -type f \( -name '*.key' -o -name '*private*' \) | grep . >/dev/null 2>&1; then
    die 'publication unexpectedly contains private key material'
fi

mv "$STAGING_DIR" "$OUTPUT_DIR"
STAGING_DIR=
printf 'PASS: published signed Northstar development repository revision %s\n' "$REVISION"
printf 'PASS: catalogue=%s metadata=%s fingerprint=%s\n' \
    "$CATALOGUE_SHA256" "$METADATA_SHA256" "$FINGERPRINT"
