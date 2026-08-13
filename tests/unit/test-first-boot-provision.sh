#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
HELPER=$ROOT/apps/first-boot/northstar-first-boot-provision
CONTROLLER=$ROOT/apps/first-boot/firstbootcontroller.cpp
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/northstar-first-boot.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

if grep -Eq 'password(_confirmation|_hash)?=' "$CONTROLLER"; then
    fail 'controller writes password material into its request payload'
fi

FAKE_ROOT=$TMP_DIR/root
BIN=$TMP_DIR/bin
mkdir -p "$FAKE_ROOT/usr/share/zoneinfo/America" \
    "$FAKE_ROOT/usr/share/zoneinfo/Pacific" "$FAKE_ROOT/etc" \
    "$FAKE_ROOT/var/db" "$FAKE_ROOT/home" \
    "$FAKE_ROOT/usr/local/share/northstar/session" "$BIN"
printf 'zone\n' > "$FAKE_ROOT/usr/share/zoneinfo/America/Denver"
printf 'zone\n' > "$FAKE_ROOT/usr/share/zoneinfo/Pacific/Auckland"
printf '[output:WL-1]\nmode = 1280x800\n' \
    > "$FAKE_ROOT/usr/local/share/northstar/session/wayfire-nested.ini"

cat > "$BIN/pw" <<'EOF'
#!/bin/sh
set -eu
printf '%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
case " $* " in
    *" usershow "*) exit 1 ;;
    *" useradd "*)
        IFS= read -r secret
        [ -n "$secret" ]
        printf 'password-bytes=%s\n' "${#secret}" >> "$NORTHSTAR_TEST_EVENTS"
        user=
        previous=
        for argument in "$@"; do
            if [ "$previous" = useradd ]; then user=$argument; break; fi
            previous=$argument
        done
        mkdir -p "$NORTHSTAR_FIRST_BOOT_ROOT/home/$user"
        ;;
esac
EOF
cat > "$BIN/sysrc" <<'EOF'
#!/bin/sh
printf 'sysrc:%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
[ "${NORTHSTAR_TEST_FAIL_SYSRC:-0}" != 1 ]
EOF
cat > "$BIN/install" <<'EOF'
#!/bin/sh
set -eu
last=
for argument in "$@"; do last=$argument; done
mkdir -p "$last"
EOF
cat > "$BIN/chown" <<'EOF'
#!/bin/sh
exit 0
EOF
cat > "$BIN/chmod" <<'EOF'
#!/bin/sh
printf 'chmod:%s\n' "$*" >> "$NORTHSTAR_TEST_EVENTS"
exec /bin/chmod "$@"
EOF
cat > "$BIN/cp" <<'EOF'
#!/bin/sh
exec /bin/cp "$@"
EOF
chmod +x "$BIN"/*

REQUEST=$TMP_DIR/request.conf
cat > "$REQUEST" <<'EOF'
protocol=1
username=hector
display_name=Hector Northstar
locale=en_US.UTF-8
timezone=America/Denver
keyboard=us
admin_confirmation=yes
EOF
chmod 600 "$REQUEST"

run_helper() {
    env NORTHSTAR_FIRST_BOOT_TEST_MODE=1 \
        NORTHSTAR_FIRST_BOOT_ROOT="$FAKE_ROOT" \
        NORTHSTAR_FIRST_BOOT_PW_PATH="$BIN/pw" \
        NORTHSTAR_FIRST_BOOT_SYSRC_PATH="$BIN/sysrc" \
        NORTHSTAR_FIRST_BOOT_INSTALL_PATH="$BIN/install" \
        NORTHSTAR_FIRST_BOOT_CHOWN_PATH="$BIN/chown" \
        NORTHSTAR_FIRST_BOOT_CHMOD_PATH="$BIN/chmod" \
        NORTHSTAR_FIRST_BOOT_CP_PATH="$BIN/cp" \
        NORTHSTAR_FIRST_BOOT_RM_PATH=/bin/rm \
        NORTHSTAR_TEST_FAIL_SYSRC="${NORTHSTAR_TEST_FAIL_SYSRC:-0}" \
        NORTHSTAR_TEST_EVENTS="$TMP_DIR/events" \
        sh "$HELPER" "$@"
}

run_helper --validate "$REQUEST" | grep -Fx 'FIRST_BOOT_REQUEST=VALID' >/dev/null \
    || fail 'valid request was rejected'

cp "$REQUEST" "$TMP_DIR/password-request.conf"
printf '%s\n' 'password=must-not-be-here' >> "$TMP_DIR/password-request.conf"
if run_helper --validate "$TMP_DIR/password-request.conf" >/dev/null 2>&1; then
    fail 'request containing password material was accepted'
fi

cp "$REQUEST" "$TMP_DIR/bad-user.conf"
sed -i.bak 's/username=hector/username=Root User/' "$TMP_DIR/bad-user.conf"
if run_helper --validate "$TMP_DIR/bad-user.conf" >/dev/null 2>&1; then
    fail 'unsafe username was accepted'
fi

cp "$REQUEST" "$TMP_DIR/bad-name.conf"
sed -i.bak 's/display_name=Hector Northstar/display_name=Hector:root/' "$TMP_DIR/bad-name.conf"
if run_helper --validate "$TMP_DIR/bad-name.conf" >/dev/null 2>&1; then
    fail 'passwd-delimiter character in display name was accepted'
fi

cp "$REQUEST" "$TMP_DIR/world-timezone.conf"
sed -i.bak 's|timezone=America/Denver|timezone=Pacific/Auckland|' "$TMP_DIR/world-timezone.conf"
run_helper --validate "$TMP_DIR/world-timezone.conf" \
    | grep -Fx 'FIRST_BOOT_REQUEST=VALID' >/dev/null \
    || fail 'an installed worldwide timezone was rejected'

cp "$REQUEST" "$TMP_DIR/bad-timezone.conf"
sed -i.bak 's|timezone=America/Denver|timezone=../../etc/passwd|' "$TMP_DIR/bad-timezone.conf"
if run_helper --validate "$TMP_DIR/bad-timezone.conf" >/dev/null 2>&1; then
    fail 'unsafe timezone traversal was accepted'
fi

cp "$REQUEST" "$TMP_DIR/missing-timezone.conf"
sed -i.bak 's|timezone=America/Denver|timezone=Pacific/Missing|' "$TMP_DIR/missing-timezone.conf"
if run_helper --validate "$TMP_DIR/missing-timezone.conf" >/dev/null 2>&1; then
    fail 'timezone absent from the installed database was accepted'
fi

cp "$REQUEST" "$TMP_DIR/retry.conf"
sed -i.bak 's/username=hector/username=retryuser/' "$TMP_DIR/retry.conf"
if printf '%s\n' 'retry-password' | NORTHSTAR_TEST_FAIL_SYSRC=1 \
    run_helper --apply "$TMP_DIR/retry.conf" >/dev/null 2>&1; then
    fail 'provisioning unexpectedly passed after a regional-settings failure'
fi
grep -F 'userdel retryuser -r' "$TMP_DIR/events" >/dev/null \
    || fail 'failed provisioning did not remove its partially created account'
[ ! -e "$FAKE_ROOT/var/db/northstar/first-boot.conf" ] \
    || fail 'failed provisioning published a completion marker'
[ ! -e "$FAKE_ROOT/usr/local/etc/sudoers.d/northstar-first-administrator" ] \
    || fail 'failed provisioning retained its administrator authorization'

printf '%s\n' 'correct-horse-battery' | run_helper --apply "$REQUEST" > "$TMP_DIR/output"
grep -Fx 'FIRST_BOOT_PROVISIONING=PASS' "$TMP_DIR/output" >/dev/null \
    || fail 'provisioning did not pass'
grep -Fx 'password-bytes=21' "$TMP_DIR/events" >/dev/null \
    || fail 'password was not delivered over stdin'
if grep -F 'correct-horse-battery' "$TMP_DIR/events" "$REQUEST" \
    "$FAKE_ROOT/var/db/northstar/first-boot.conf" >/dev/null 2>&1; then
    fail 'password leaked into request, events, or completion state'
fi
grep -Fx 'status=complete' "$FAKE_ROOT/var/db/northstar/first-boot.conf" >/dev/null \
    || fail 'completion marker is missing'
grep -Fx 'administrator=hector' "$FAKE_ROOT/var/db/northstar/first-boot.conf" >/dev/null \
    || fail 'completion marker has wrong administrator'
grep -F ':lang=en_US.UTF-8:' "$FAKE_ROOT/home/hector/.login_conf" >/dev/null \
    || fail 'user locale was not recorded'
grep -Fx 'America/Denver' "$FAKE_ROOT/var/db/zoneinfo" >/dev/null \
    || fail 'timezone was not recorded'
cmp "$FAKE_ROOT/usr/local/share/northstar/session/wayfire-nested.ini" \
    "$FAKE_ROOT/home/hector/.config/wayfire.ini" >/dev/null \
    || fail 'new administrator did not receive the Northstar Wayfire configuration'
grep -Fx 'hector ALL=(ALL:ALL) ALL' \
    "$FAKE_ROOT/usr/local/etc/sudoers.d/northstar-first-administrator" >/dev/null \
    || fail 'new administrator did not receive persistent sudo authorization'
grep -Fx "chmod:0440 $FAKE_ROOT/usr/local/etc/sudoers.d/northstar-first-administrator" \
    "$TMP_DIR/events" >/dev/null \
    || fail 'first-administrator sudoers policy was not set to mode 0440'

if printf '%s\n' 'another-password' | run_helper --apply "$REQUEST" >/dev/null 2>&1; then
    fail 'one-time setup was allowed to run twice'
fi

printf '%s\n' 'PASS: first-boot provisioning validates profile data, keeps passwords off disk, and seals setup'
