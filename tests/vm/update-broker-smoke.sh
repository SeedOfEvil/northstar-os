#!/bin/sh

# Native broker smoke. Run explicitly as root on a disposable test checkout.
# Every input, key, fake tool, request, and log lives under one temporary
# directory; the real bectl and pkg commands are never invoked.

set -eu

BROKER_BIN=${NORTHSTAR_UPDATE_BROKER_BIN:-build/src/update/northstar-update-broker}
TMP_DIR=

die() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

cleanup() {
    if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
        rm -rf "$TMP_DIR"
    fi
}
trap cleanup EXIT HUP INT TERM

[ "$(uname -s)" = FreeBSD ] || die 'update broker smoke requires FreeBSD'
[ "$(id -u)" -eq 0 ] || die 'run update broker smoke as root; it validates root-owned staging inputs'
[ -x "$BROKER_BIN" ] || die "broker binary is unavailable: $BROKER_BIN"

for command_name in awk base64 chmod head mkdir mktemp openssl sha256 stat tr wc; do
    command -v "$command_name" >/dev/null 2>&1 || die "required command is unavailable: $command_name"
done

TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-update-broker.XXXXXX")
mkdir -m 700 \
    "$TMP_DIR/fingerprints" \
    "$TMP_DIR/fingerprints/trusted" \
    "$TMP_DIR/fingerprints/revoked" \
    "$TMP_DIR/publication" \
    "$TMP_DIR/request"

printf '%s\n' \
    'channel=development' \
    'repository_tag=northstar-development' \
    'repository_name=Northstar Development' \
    'repository_url=pkg+https://packages.example.test/northstar' \
    'mirror_type=none' \
    'signature_type=fingerprints' \
    "fingerprints_path=$TMP_DIR/fingerprints" \
    'trust_mode=required' \
    > "$TMP_DIR/policy.conf"

printf '%s\n' 'catalogue-fixture' > "$TMP_DIR/publication/data.pkg"
catalogue_sha256=$(sha256 -q "$TMP_DIR/publication/data.pkg")
printf '%s\n' "$catalogue_sha256" > "$TMP_DIR/publication/payload"

openssl genrsa -out "$TMP_DIR/repo.key" 2048 >/dev/null 2>&1
openssl rsa -in "$TMP_DIR/repo.key" -pubout -out "$TMP_DIR/repo.pub" >/dev/null 2>&1
public_key_bytes=$(wc -c < "$TMP_DIR/repo.pub")
head -c $((public_key_bytes - 1)) "$TMP_DIR/repo.pub" > "$TMP_DIR/repo.pub.trimmed"
openssl dgst -sha256 -sign "$TMP_DIR/repo.key" \
    -out "$TMP_DIR/publication/signature.bin" "$TMP_DIR/publication/payload"
signature_fingerprint=$(sha256 -q "$TMP_DIR/repo.pub.trimmed")
printf '%s\n' \
    'function: sha256' \
    "fingerprint: $signature_fingerprint" \
    > "$TMP_DIR/fingerprints/trusted/northstar"

public_key_json=$(awk 'NR > 1 { printf "\\n" } { printf "%s", $0 }' "$TMP_DIR/repo.pub.trimmed")
signature_base64=$(base64 < "$TMP_DIR/publication/signature.bin" | tr -d '\n')
printf '%s\n' \
    '{' \
    '  "schema_version": 1,' \
    '  "repository_tag": "northstar-development",' \
    '  "channel": "development",' \
    '  "abi": "FreeBSD:15:amd64",' \
    '  "revision": 42,' \
    '  "generated_at": "2026-08-08T12:00:00Z",' \
    '  "source_revision": "abcdef1",' \
    '  "signature_status": "verified",' \
    "  \"signature_fingerprint\": \"$signature_fingerprint\"," \
    '  "signature_envelope": "signature.json",' \
    '  "catalogue_file": "data.pkg",' \
    "  \"catalogue_sha256\": \"$catalogue_sha256\"," \
    '  "packages": [' \
    '    {' \
    '      "name": "northstar-shell",' \
    '      "version": "0.2.0",' \
    '      "origin": "desk/northstar-shell",' \
    '      "source": "ports/northstar",' \
    '      "project_revision": "abcdef1"' \
    '    }' \
    '  ]' \
    '}' \
    > "$TMP_DIR/publication/repository-metadata.json"

printf '%s\n' \
    '{' \
    '  "schema_version": 1,' \
    '  "type": "rsa",' \
    "  \"payload\": \"$catalogue_sha256\"," \
    "  \"public_key_pem\": \"$public_key_json\"," \
    "  \"signature_base64\": \"$signature_base64\"," \
    "  \"fingerprint_sha256\": \"$signature_fingerprint\"" \
    '}' \
    > "$TMP_DIR/publication/signature.json"

printf '%s\n' 'northstar-shell|0.1.0' > "$TMP_DIR/installed.txt"
printf '%s\n' '#!/bin/sh' 'exit 0' > "$TMP_DIR/fake-bectl"
printf '%s\n' '#!/bin/sh' 'exit 0' > "$TMP_DIR/fake-zfs"
chmod 700 "$TMP_DIR/fake-bectl" "$TMP_DIR/fake-zfs"

"$BROKER_BIN" \
    --stage-create-before \
    --confirm \
    --policy "$TMP_DIR/policy.conf" \
    --metadata "$TMP_DIR/publication/repository-metadata.json" \
    --installed-file "$TMP_DIR/installed.txt" \
    --request "$TMP_DIR/request/update.request" \
    --bectl "$TMP_DIR/fake-bectl" \
    --zfs "$TMP_DIR/fake-zfs"

[ "$(stat -f '%u %Lp' "$TMP_DIR/request/update.request")" = '0 600' ] || {
    die 'broker did not write a root-owned mode-0600 request'
}
grep -Fx 'operation=create-before' "$TMP_DIR/request/update.request" >/dev/null 2>&1 || die 'request operation is wrong'
grep -Fx 'boot_environment=northstar-before-development-r42-abcdef1' \
    "$TMP_DIR/request/update.request" >/dev/null 2>&1 || die 'request boot environment is wrong'
grep -Fx 'authorization=interactive-confirmation' \
    "$TMP_DIR/request/update.request" >/dev/null 2>&1 || die 'request confirmation field is missing'

printf '%s\n' 'PASS: broker independently verified the publication and staged a root-owned request'
printf '%s\n' 'PASS: no pkg or real bectl command was run'
