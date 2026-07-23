#!/usr/bin/env bash
# =============================================================================
# dpx_tc002 test suite
# Usage: bash tools/dpx_test.sh [--auto] [--fast] [--reboot] [--suite=NAME] [IP]
# --auto   : skip all display/audio prompts (CI mode)
# --fast   : skip timing-sensitive sleeps
# --reboot : include persistence-after-reboot test
# --suite  : run one suite only: connectivity|apps|notify|overlay|indicators|tc|sound|settings|persist
# =============================================================================
HOST="${!#}"; [[ "$HOST" == --* || -z "$HOST" ]] && HOST="192.168.2.33"
BASE="http://$HOST"
AUTO=false; FAST=false; REBOOT=false; SUITE=""
for a in "$@"; do case "$a" in
    --auto)    AUTO=true ;;
    --fast)    FAST=true ;;
    --reboot)  REBOOT=true ;;
    --suite=*) SUITE="${a#*=}" ;;
esac; done

G="\033[0;32m"; R="\033[0;31m"; Y="\033[1;33m"
C="\033[0;36m"; B="\033[1;34m"; DIM="\033[2m"; RST="\033[0m"
PASS=0; FAIL=0; SKIP=0
SAVED_TIM="true"; SAVED_DAT="false"

# ── Restore state on exit ─────────────────────────────────────────────────────
restore() {
    curl -sf -X POST "$BASE/api/settings" -H "Content-Type: application/x-www-form-urlencoded" \
         --data-urlencode "plain={\"TIM\":$SAVED_TIM,\"DAT\":$SAVED_DAT}" > /dev/null 2>&1
    curl -sf -X POST "$BASE/api/notify/dismiss" -H "Content-Type: application/x-www-form-urlencoded" \
         --data-urlencode 'plain={}' > /dev/null 2>&1
    curl -sf -X POST "$BASE/api/sound" -H "Content-Type: application/x-www-form-urlencoded" \
         --data-urlencode 'plain={}' > /dev/null 2>&1
    for i in 1 2 3; do
        curl -sf -X POST "$BASE/api/indicator$i" -H "Content-Type: application/x-www-form-urlencoded" \
             --data-urlencode 'plain={"color":"#000000"}' > /dev/null 2>&1
    done
}
trap restore EXIT

# ── Helpers ───────────────────────────────────────────────────────────────────
_post() { curl -sf --compressed --max-time 8 -X POST "$BASE$1" \
               -H "Content-Type: application/x-www-form-urlencoded" --data-urlencode "plain=$2"; }
_get()  { curl -sf --compressed --max-time 8 "$BASE$1"; }
_jq()   { echo "$1" | jq -r "$2" 2>/dev/null; }
_wait() { $FAST || sleep "$1"; }

# POST to /api/custom?name=X with body fields
_app() { local n="$1" b="$2"
    curl -sf --compressed --max-time 8 -X POST "$BASE/api/custom?name=$n" \
         -H "Content-Type: application/x-www-form-urlencoded" --data-urlencode "plain=$b" > /dev/null; }

# Delete a custom app immediately
_del() { curl -sf --compressed --max-time 8 -X POST "$BASE/api/custom?name=$1" \
              -H "Content-Type: application/x-www-form-urlencoded" --data-urlencode 'plain={}' > /dev/null; }

ok()   { echo -e "  ${G}✓${RST} $1"; PASS=$((PASS+1)); }
fail() { echo -e "  ${R}✗${RST} $1"; FAIL=$((FAIL+1)); }
skip() { echo -e "  ${DIM}⊘${RST} $1"; SKIP=$((SKIP+1)); }
info() { echo -e "  ${DIM}  $1${RST}"; }

assert_ok() {
    local got; got=$(echo "$1" | jq -r '.ok' 2>/dev/null)
    [[ "$got" == "true" ]] && ok "$2" || fail "$2 (got: $1)"
}
assert_has() {
    echo "$1" | grep -q "$2" && ok "$3" || fail "$3 (missing: $2)"
}
assert_no() {
    echo "$1" | grep -q "$2" && fail "$3 (found: $2)" || ok "$3"
}
assert_key() {
    local got; got=$(echo "$1" | jq -r "$2" 2>/dev/null)
    [[ "$got" == "$3" ]] && ok "$4" || fail "$4 (expected '$3' got '$got')"
}

# Human check: show ONE thing, ask Y/n, then optionally run cleanup cmd
vis() {
    if $AUTO; then [[ -n "$2" ]] && eval "$2" > /dev/null 2>&1; return; fi
    _blank
    echo -e "\n  ${Y}\xF0\x9F\x91\x81  $1${RST}"
    echo -en "  ${Y}    Looks correct? [Y/n/s] ${RST}"
    read -r _a; _a=$(echo "$_a" | tr '[:upper:]' '[:lower:]')
    [[ -n "$2" ]] && eval "$2" > /dev/null 2>&1
    case "$_a" in n|no) fail "VISUAL: $1" ;; s) skip "VISUAL: $1" ;; *) ok "VISUAL: $1" ;; esac
}
snd() {
    if $AUTO; then return; fi
    echo -e "\n  ${C}🔊 $1${RST}"
    echo -en "  ${C}    Did you hear it? [Y/n/s] ${RST}"
    read -r _a; _a=$(echo "$_a" | tr '[:upper:]' '[:lower:]')
    case "$_a" in n|no) fail "AUDIO: $1" ;; s) skip "AUDIO: $1" ;; *) ok "AUDIO: $1" ;; esac
}

suite() {
    [[ -n "$SUITE" && "$SUITE" != "$1" ]] && return 1
    echo -e "\n${B}━━ $1 ━━${RST}"
    return 0
}

# =============================================================================
echo -e "\n${C}dpx_tc002 — $HOST — $(date '+%H:%M:%S')${RST}"

# ── connectivity ──────────────────────────────────────────────────────────────
suite "connectivity" || { :; } && {
resp=$(_get /dpx) || { fail "unreachable at $HOST"; exit 1; }
assert_has "$resp" '"build"' "GET /dpx"
info "Build: $(_jq "$resp" '.build')"
resp=$(_get /api/stats);  assert_has "$resp" '"ram"'       "GET /api/stats"
resp=$(_get /api/apps);   assert_has "$resp" '"name"'      "GET /api/apps"
resp=$(_get /api/effects); assert_has "$resp" '"dpx Matrix"' "GET /api/effects"
resp=$(_get /api/settings)
for k in BRI ATIME ATRANS SSPEED TIM DAT TC_MUTE SOUND; do assert_has "$resp" "\"$k\"" "settings.$k"; done
# Mute natives so they don't stomp display tests
SAVED_TIM=$(_jq "$resp" '.TIM // "true"')
SAVED_DAT=$(_jq "$resp" '.DAT // "false"')
_post /api/settings '{"TIM":false,"DAT":false}' > /dev/null
info "Natives muted (restored on exit)"
# Sweep ALL custom apps off the device — it could be in any state
_sweeping=$(curl -sf --compressed --max-time 8 "$BASE/api/apps" | jq -r '.[] | select(.native==false) | .name' 2>/dev/null)
if [[ -n "$_sweeping" ]]; then
    while IFS= read -r _n; do _del "$_n"; done <<< "$_sweeping"
    info "Swept custom apps: $(echo "$_sweeping" | tr '\n' ' ')"
fi
# Dismiss any active notification and stop sound
_post /api/notify/dismiss '{}' > /dev/null
_post /api/sound '{}' > /dev/null
for _i in 1 2 3; do _post /api/indicator$_i '{"color":"#000000"}' > /dev/null; done
# Force dpx_matrix effect active by setting BRI (triggers dpxActivateEffect internally)
SAVED_BRI=$(_jq "$resp" '.BRI // "128"')
_post /api/settings "{\"BRI\":$SAVED_BRI}" > /dev/null
sleep 1
info "Device state cleared"
}

# ── apps ──────────────────────────────────────────────────────────────────────
suite "apps" || { :; } && {
# Create + show green HELLO — delete immediately after confirm
_app "_t" '{"text":"HELLO","color":"#00FF00","dur":999}'
assert_has "$(_get /api/apps)" '"_t"' "App appears in loop"
_post /api/switch '{"name":"_t"}' > /dev/null
vis "GREEN text 'HELLO' on display" '_del _t'
# Create + show rainbow — delete after confirm
_app "_t" '{"text":"RAINBOW","rainbow":true,"dur":999}'
_post /api/switch '{"name":"_t"}' > /dev/null
vis "RAINBOW — each letter a different hue" '_del _t'
# Next / previous (no test app, just native loop)
_app "_t_a" '{"text":"AAAA","color":"#FF0000","dur":999}'
_app "_t_b" '{"text":"BBBB","color":"#0088FF","dur":999}'
_post /api/switch '{"name":"_t_a"}' > /dev/null
vis "Red 'AAAA' on display"
resp=$(_post /api/nextapp '{}'); assert_ok "$resp" "Next app"
vis "Advanced — should now show BBBB (blue)" '_del _t_a; _del _t_b'
# Mute test
_app "_t_mute" '{"text":"MUTED","color":"#FF8800","dur":999}'
_app "_t_vis"  '{"text":"VISIBLE","color":"#00FF88","dur":999}'
_post /api/switch '{"name":"_t_vis"}' > /dev/null
_post /api/mute '{"name":"_t_mute","mute":true}' > /dev/null
resp=$(_post /api/nextapp '{}'); assert_ok "$resp" "Next past muted app"
vis "VISIBLE (green) shows — MUTED (orange) was skipped" '_del _t_mute; _del _t_vis'
# API roundtrip: create, verify in list, delete, verify gone
_app "_t_x" '{"text":"X","dur":10}'
assert_has "$(_get /api/apps)" '"_t_x"' "App in list after create"
resp=$(_post /api/custom '{}'); _del _t_x
assert_no "$(_get /api/apps)" '"_t_x"' "App gone after delete"
}

# ── notify ────────────────────────────────────────────────────────────────────
suite "notify" || { :; } && {
# 1. White notification — stays until dismissed after confirm
_post /api/notify '{"text":"TEST","color":"#FFFFFF","duration":30}' > /dev/null
vis "White 'TEST' scrolling" '_post /api/notify/dismiss {}'

# 2. Dismiss test — show it, ask, then we dismiss via API, ask again
_post /api/notify '{"text":"DISMISS ME","color":"#FF8800","duration":30}' > /dev/null
vis "Orange 'DISMISS ME' scrolling — confirm you see it"
resp=$(_post /api/notify/dismiss '{}'); assert_ok "$resp" "Dismiss API"
vis "Display cleared immediately after dismiss (nothing scrolling)"

# 3. Finite repeat — must scroll exactly 2× then vanish on its own
_post /api/notify '{"text":"TWICE","color":"#00FFFF","repeat":2,"duration":30}' > /dev/null
vis "Cyan 'TWICE' — watch it scroll EXACTLY 2 times then disappear on its own"

# 4. Rainbow
_post /api/notify '{"text":"RAINBOW","rainbow":true,"duration":30}' > /dev/null
vis "Rainbow notification — per-letter colors" '_post /api/notify/dismiss {}'
}

# ── overlay ───────────────────────────────────────────────────────────────────
suite "overlay" || { :; } && {
for effect in sparkle twinkle rain drizzle snow storm strobe blink frost; do
    _app "_t_ov" "{\"text\":\"$effect\",\"color\":\"#FFFFFF\",\"overlay\":\"$effect\",\"dur\":999}"
    _post /api/switch '{"name":"_t_ov"}' > /dev/null
    vis "$effect — text visible WITH $effect effect on top" '_del _t_ov'
done
# 1.11 regression: effect must clear when app has no overlay
_app "_t_clean" '{"text":"CLEAN","color":"#FF8800","dur":999}'
_post /api/switch '{"name":"_t_clean"}' > /dev/null
vis "1.11 REGRESSION: 'CLEAN' with NO effects — display must be plain text only" '_del _t_clean'
# 1.12 regression: brightness respected
orig_bri=$(_jq "$(_get /api/settings)" '.BRI')
_post /api/settings '{"BRI":20}' > /dev/null
_app "_t_dim" '{"text":"DIM","overlay":"rain","dur":999}'
_post /api/switch '{"name":"_t_dim"}' > /dev/null
vis "1.12 REGRESSION: BRI=20 — text AND rain drops both dim, not full brightness" '_del _t_dim'
_post /api/settings "{\"BRI\":$orig_bri}" > /dev/null
}

# ── indicators ────────────────────────────────────────────────────────────────
suite "indicators" || { :; } && {
_post /api/indicator1 '{"color":"#FF0000"}' > /dev/null
vis "TOP-LEFT corner: solid RED (3px L-shape)" '_post /api/indicator1 {"color":"#000000"}'
_post /api/indicator2 '{"color":"#00FF00","blink":400}' > /dev/null
vis "TOP-RIGHT corner: GREEN blinking ~400ms" '_post /api/indicator2 {"color":"#000000"}'
_post /api/indicator3 '{"color":"#0088FF","fade":1500}' > /dev/null
vis "BOTTOM-LEFT corner: BLUE pulsing slowly" '_post /api/indicator3 {"color":"#000000"}'
for i in 1 2 3; do resp=$(_post /api/indicator$i '{"color":"#000000"}'); assert_ok "$resp" "Clear indicator $i"; done
}

# ── tc ────────────────────────────────────────────────────────────────────────
suite "tc" || { :; } && {
saved=$(_get /api/dev); saved_mute=$(_jq "$saved" '.tc_mute')
# Settings roundtrip
resp=$(_post /api/dev '{"tc_mute":true}');   assert_ok "$resp" "tc_mute=true"
assert_key "$(_get /api/dev)" '.tc_mute' "true" "tc_mute in dev.json"
assert_key "$(_get /api/settings)" '.TC_MUTE' "true" "TC_MUTE in settings"
resp=$(_post /api/dev '{"tc_mute":false}');  assert_ok "$resp" "tc_mute=false"
resp=$(_post /api/dev '{"tc_dwell":3,"tc_hold":false,"tc_show_frames":false}')
assert_ok "$resp" "TC settings"
assert_key "$(_get /api/dev)" '.tc_dwell' "3" "tc_dwell roundtrip"
# Inject TC signal via JSON API
curl -sf -X POST "$BASE/json" -H 'Content-Type: application/json' \
     -d '{"dpx":{"tc":"01:23:45:12"}}' > /dev/null
_wait 1
assert_has "$(_get /api/apps)" '"tc"'  "TC injection creates tc app"
assert_key "$(_get /dpx)" '.app' "tc" "Display locked to tc"
vis "TC DISPLAY: shows 01:23:45 with frame progress bar"
# Dwell: tc app must auto-remove after 3s — needs real sleep even in fast mode
sleep 4
assert_no "$(_get /api/apps)" '"tc"' "tc app removed after dwell"
# Mute: wait for dwell then test mute
sleep 4
_post /api/dev '{"tc_mute":true}' > /dev/null
curl -sf -X POST "$BASE/json" -H 'Content-Type: application/json' \
     -d '{"dpx":{"tc":"02:00:00:00"}}' > /dev/null
_wait 1
got=$(_jq "$(_get /dpx)" '.app')
[[ "$got" != "tc" ]] && ok "TC mute suppresses takeover (app=$got)" || fail "TC mute failed (still tc)"
_post /api/dev "{\"tc_mute\":$saved_mute,\"tc_dwell\":2}" > /dev/null
ok "TC settings restored"
}

# ── sound ─────────────────────────────────────────────────────────────────────
suite "sound" || { :; } && {
resp=$(_post /api/beeptest '{}')
assert_ok "$resp" "POST /api/beeptest"
assert_key "$resp" '.pin' "15" "Buzzer GPIO 15"
assert_key "$resp" '.ledc_ch' "6" "LEDC ch 6"
_wait 1; snd "Two 880Hz beeps"
# RTTTL parser frequencies — automated, no audio
purl="$BASE/api/buzzer/parse?q=Beep%3Ad%3D4%2Co%3D5%2Cb%3D200%3Ac%2Ce%2Cg%2Cc6"
pr=$(curl -sf --compressed --max-time 5 "$purl")
[[ -n "$pr" ]] && {
    assert_key "$pr" '.[0].f' "524" "C5=524Hz"
    assert_key "$pr" '.[1].f' "660" "E5=660Hz"
    assert_key "$pr" '.[2].f' "784" "G5=784Hz"
    assert_key "$pr" '.[3].f' "1048" "C6=1048Hz"
} || skip "buzzer/parse endpoint not in build"
resp=$(_post /api/sound '{"rtttl":"Scale:d=4,o=5,b=120:c,d,e,f,g,a,b,c6"}')
assert_ok "$resp" "RTTTL scale"
_wait 5; snd "C major scale — 8 ascending notes"
resp=$(_post /api/sound '{"rtttl":"Mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g5,8p"}')
assert_ok "$resp" "RTTTL Mario"
_wait 3; snd "Mario theme opening"
_post /api/sound '{}' > /dev/null
# Sound disable: settings persists and blocks playback
_post /api/settings '{"SOUND":false}' > /dev/null
assert_key "$(_get /api/settings)" '.SOUND' "false" "SOUND=false in settings"
_post /api/sound '{"rtttl":"Test:d=4,o=5,b=200:c,e,g"}' > /dev/null
_wait 1; snd "SOUND DISABLED — silence (nothing should play)"
_post /api/settings '{"SOUND":true}' > /dev/null; ok "Sound re-enabled"
}

# ── settings ──────────────────────────────────────────────────────────────────
suite "settings" || { :; } && {
resp=$(_get /api/settings)
for k in BRI ATIME ATRANS SSPEED UPPERCASE SOUND TIM DAT TC_MUTE MQTT_PREFIX; do
    assert_has "$resp" "\"$k\"" "settings.$k"
done
orig=$(_jq "$resp" '.BRI')
assert_ok "$(_post /api/settings '{"BRI":40}')" "Set BRI=40"
assert_key "$(_get /api/settings)" '.BRI' "40" "BRI=40 roundtrip"
vis "Brightness visibly reduced (BRI=40)" "_post /api/settings '{\"BRI\":$orig}'"
ok "BRI restored to $orig"
assert_ok "$(_post /api/settings '{"TIM":false}')" "TIM=false"
assert_no "$(_get /api/apps)" '"Time"' "Time hidden"
assert_ok "$(_post /api/settings '{"TIM":true}')" "TIM=true"
assert_has "$(_get /api/apps)" '"Time"' "Time visible"
}

# ── persist ───────────────────────────────────────────────────────────────────
suite "persist" || { :; } && {
$REBOOT || { skip "persist (pass --reboot to enable)"; }
$REBOOT && {
_post /api/settings '{"TIM":false}' > /dev/null
assert_key "$(_get /api/dev)" '.show_time' "false" "show_time=false before reboot"
info "Rebooting..."; _post /api/reboot '{}' > /dev/null; sleep 10
resp=$(_get /dpx) && ok "Device back online" || fail "Device offline after reboot"
assert_no "$(_get /api/apps)" '"Time"' "Time still hidden after reboot"
_post /api/settings '{"TIM":true}' > /dev/null; ok "Time restored"
}
}

# ── summary ───────────────────────────────────────────────────────────────────
echo ""
echo -e "${B}━━ PASS ${G}${PASS}${RST}${B}  FAIL ${R}${FAIL}${RST}${B}  SKIP ${DIM}${SKIP}${RST}${B} ━━${RST}"
[[ $FAIL -eq 0 ]] && echo -e "${G}  All checks passed.${RST}" && exit 0
echo -e "${R}  $FAIL check(s) failed.${RST}"; exit 1
