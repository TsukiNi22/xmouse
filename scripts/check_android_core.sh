#!/usr/bin/env bash
#
# Edition:
##  22/11/2026 by Tsukini
#
# File Name:
##  check_android_core.sh
#
# File Description:
##  Check the member core of the android app (android/src/.../core, plain Java) against the xmouse built at the root:
##  the "phone" joins a network of xmouse, then creates its own network that an xmouse joins (identity, Argon2id,
##  handshake, state, cursor). Needs a JDK (javac, keytool) and ./xmouse.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'kill $(jobs -p) 2> /dev/null || true; rm -rf "$WORK"' EXIT

die() { echo "check_android_core: $*" >&2; exit 1; }
[[ -x "$ROOT/xmouse" ]] || die "xmouse not built at the root"

# The core and the check, compiled for the PC
mkdir -p "$WORK/classes"
javac -d "$WORK/classes" "$ROOT"/android/src/io/github/tsukini22/xmouse/core/*.java "$ROOT/android/check/NodeCheck.java"
keytool -genkeypair -keystore "$WORK/phone.p12" -storetype PKCS12 -storepass checks -keypass checks -alias phone -keyalg EC -groupname secp256r1 -sigalg SHA256withECDSA -validity 30 -dname "CN=xmouse phone" > /dev/null 2>&1 || die "keytool cannot create the identity of the phone"

# One xmouse (a terminal panel fed by a fifo, offscreen: it has a screen for the cursor)
run_xmouse() { # <name> <port>
    mkfifo "$WORK/$1.in"
    # shellcheck disable=SC2034 # the fifo stays open for writing (the commands of say)
    exec {fd}<> "$WORK/$1.in"
    # DISPLAY set: the terminal panel runs a QApplication (offscreen: one virtual screen, nothing captured or shown)
    DISPLAY="${DISPLAY:-:99}" XMOUSE_HOME="$WORK/$1" QT_QPA_PLATFORM=offscreen "$ROOT/xmouse" --tty --port "$2" --name "$1" < "$WORK/$1.in" > "$WORK/$1.log" 2>&1 &
    sleep 2
}
say() { echo "$2" > "$WORK/$1.in"; }

echo "== 1. the phone joins a network of xmouse"
run_xmouse desk 47962
say desk "create desk"
sleep 2
LINK="$(grep -E '^link' "$WORK/desk.log" | tail -1 | awk '{print $2}' | sed -E 's#https://[^:/]+:#https://127.0.0.1:#')"
java -cp "$WORK/classes" NodeCheck join "$LINK" "$WORK/phone.p12" > "$WORK/phone1.log" 2>&1 &
PHONE=$!
for _ in $(seq 1 30); do grep -q "gave the right code" "$WORK/desk.log" && break; sleep 1; done
say desk "accept"
wait "$PHONE" || true
grep -E "^JOIN|^MEMBER" "$WORK/phone1.log"
say desk "members"
sleep 1
grep -E "test-phone" "$WORK/desk.log" | tail -1
grep -q "^JOIN OK" "$WORK/phone1.log" || die "the phone could not join (phone1.log: $(tail -3 "$WORK/phone1.log"))"

echo "== 2. the phone creates its network, an xmouse joins it, the cursor of the phone goes there"
java -cp "$WORK/classes" NodeCheck host "$WORK/phone.p12" > "$WORK/phone2.log" 2>&1 &
PHONE=$!
for _ in $(seq 1 20); do grep -q "^LINK" "$WORK/phone2.log" && break; sleep 1; done
PLINK="$(grep '^LINK' "$WORK/phone2.log" | awk '{print $2}')"
run_xmouse laptop 47963
say laptop "join $PLINK"
wait "$PHONE" || true
grep -E "^JOIN-REQUEST|^MEMBER|^NOTICE" "$WORK/phone2.log"
grep -E "joined|linked" "$WORK/laptop.log" | head -3
python3 - "$WORK/laptop/data/logs" <<'PY'
import json, os, sys
d = sys.argv[1]
lines = [json.loads(l) for f in os.listdir(d) for l in open(os.path.join(d, f))]
entered = [e["msg"] for e in lines if "entered" in e.get("msg", "")]
print("laptop:", entered[-1] if entered else "no cursor entered")
sys.exit(0 if entered else 1)
PY
grep -q "^MEMBER laptop desktop online" "$WORK/phone2.log" || die "the xmouse did not join the network of the phone"
echo "check_android_core: ok"
