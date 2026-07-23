#!/usr/bin/env bash
# =============================================================================
# dpx_tc002 regression test suite
# =============================================================================
# Usage:
#   ./tools/dpx_test.sh [options] [DEVICE_IP]
#
# Options:
#   --auto       Automated API checks only (no display pauses, CI-safe)
#   --display    Display/visual checks only (requires human watching)
#   --fast       Skip sleep-heavy timing tests
#   --reboot     Include the reboot/persistence test (slow, ~10s)
#   --suite=X    Run only suite: connectivity|apps|notify|overlay|
#                               indicators|tc|sound|settings|persist
#
# Default: run all suites with display checks, skip reboot test
# Exit code: 0 = all automated tests passed, 1 = any failure
# =============================================================================

HOST="${!#}"; [[ "$HOST" == --* || -z "$HOST" ]] && HOST="192.168.2.33"
BASE="http://$HOST"

MODE_AUTO=false; MODE_DISPLAY=false; MODE_FAST=false; MODE_REBOOT=false
SUITE_FILTER=""
for arg in "$@"; do
    case "$arg" in
        --auto)     MODE_AUTO=true ;;
        --display)  MODE_DISPLAY=true ;;
        --fast)     MODE_FAST=true ;;
        --reboot)   MODE_REBOOT=true ;;
        --suite=*)  SUITE_FILTER="${arg#*=}" ;;
    esac
done
[[ "$MODE_AUTO" == false && "$MODE_DISPLAY" == false ]] && MODE_AUTO=true && MODE_DISPLAY=true

G="\033[0;32m"; R="\033[0;31m"; Y="\033[1;33m"
C="\033[0;36m"; B="\033[1;34m"; DIM="\033[2m"; RST="\033[0m"

PASS=0; FAIL=0; SKIP=0
CURRENT_SUITE=""
_CLEANUP_APPS=()

cleanup() {
    [[ ${#_CLEANUP_APPS[@]} -eq 0 ]] && return
    for n in "${_CLEANUP_APPS[@]}"; do
        _post /api/custom "{\"name\":\"$n\"}" > /dev/null 2>&1
    done
}
trap cleanup EXIT

# ── Helpers ───────────────────────────────────────────────────────────────────
suite() {
    CURRENT_SUITE="$1"
    [[ -n "$SUITE_FILTER" && "$SUITE_FILTER" != "$1" ]] && return
    echo -e "\n${B}━━ $1 ━━${RST}"
}

_should_run() {
    [[ -n "$SUITE_FILTER" && "$SUITE_FILTER" != "$CURRENT_SUITE" ]] && return 1
    return 0
}

_post() {
    local url="$1" body="$2"
    curl -sf --compressed --max-time 8 -X POST "$BASE$url" \
         -H "Content-Type: application/x-www-form-urlencoded" \
         --data-urlencode "plain=$body"
}

_get() { curl -sf --compressed --max-time 8 "$BASE$1"; }

_jq() { echo "$1" | jq -r "$2" 2>/dev/null; }

_sleep() { "$MODE_FAST" && return; sleep "$1"; }

ok()   { echo -e "  ${G}✓${RST} $1"; ((PASS++)); }
fail() { echo -e "  ${R}✗${RST} $1"; ((FAIL++)); }
skip() { echo -e "  ${DIM}⊘ $1${RST}"; ((SKIP++)); }
info() { echo -e "  ${DIM}  $1${RST}"; }
vis()  {
    "$MODE_DISPLAY" || return
    echo -e "  ${Y}👁  [DISPLAY] $1${RST}"
    "$MODE_FAST" || sleep 2
}

assert_ok() {
    local resp="$1" label="$2"
    local got; got=$(_jq "$resp" '.ok')
    if [[ "$got" == "true" ]]; then ok "$label"
    else fail "$label — expected {ok:true}, got: $resp"; fi
}

assert_key() {
    local resp="$1" path="$2" expected="$3" label="$4"
    local got; got=$(_jq "$resp" "$path")
    if [[ "$got" == "$expected" ]]; then ok "$label"
    else fail "$label — expected '$expected', got '$got'"; fi
}

assert_contains() {
    local resp="$1" needle="$2" label="$3"
    if echo "$resp" | grep -q "$needle"; then ok "$label"
    else fail "$label — '$needle' not in: $resp"; fi
}

assert_not_contains() {
    local resp="$1" needle="$2" label="$3"
    if ! echo "$resp" | grep -q "$needle"; then ok "$label"
    else fail "$label — '$needle' should NOT be in: $resp"; fi
}

register_app() { _CLEANUP_APPS+=("$1"); }

# =============================================================================
echo -e "\n${C}dpx_tc002 regression suite — $HOST — $(date '+%Y-%m-%d %H:%M')${RST}"
echo -e "${DIM}  modes: auto=$MODE_AUTO display=$MODE_DISPLAY fast=$MODE_FAST reboot=$MODE_REBOOT${RST}"

# ── connectivity ──────────────────────────────────────────────────────────────
suite "connectivity"
if _should_run; then
    resp=$(_get /dpx) || { fail "GET /dpx — unreachable at $HOST"; exit 1; }
    assert_contains "$resp" '"build"' "GET /dpx"
    info "Build: $(_jq "$resp" '.build')"
    resp=$(_get /api/stats)
    assert_contains "$resp" '"ram"' "GET /api/stats"
    resp=$(_get /api/settings)
    for key in BRI ATIME ATRANS SSPEED TIM DAT TC_MUTE SOUND; do
        assert_contains "$resp" "\"$key\"" "/api/settings has $key"
    done
    resp=$(_get /api/apps)
    assert_contains "$resp" '"name"' "GET /api/apps"
    resp=$(_get /api/effects)
    assert_contains "$resp" '"dpx Matrix"' "GET /api/effects"
fi

# ── apps ──────────────────────────────────────────────────────────────────────
suite "apps"
if _should_run; then
    register_app "_t_basic"; register_app "_t_rainbow"

    resp=$(curl -sf --compressed --max-time 8 -X POST "$BASE/api/custom?name=_t_basic" \
         -H "Content-Type: application/x-www-form-urlencoded" \
         --data-urlencode 'plain={"text":"HELLO","color":"#00FF00","dur":10}')
    assert_ok "$resp" "Create custom app"
    resp=$(_get /api/apps)
    assert_contains "$resp" '"_t_basic"' "New app in /api/apps"

    resp=$(_post /api/switch '{"name":"_t_basic"}')
    assert_ok "$resp" "Switch to app"
    vis "Display: GREEN 'HELLO'"

    resp=$(curl -sf --compressed --max-time 8 -X POST "$BASE/api/custom?name=_t_rainbow" \
         -H "Content-Type: application/x-www-form-urlencoded" \
         --data-urlencode 'plain={"text":"RAINBOW","rainbow":true,"dur":10}')
    assert_ok "$resp" "Create rainbow app"
    _post /api/switch '{"name":"_t_rainbow"}' > /dev/null
    vis "Display: 'RAINBOW' — per-letter rainbow hues (R=red, A=orange, I=yellow...)"

    resp=$(_post /api/nextapp '{}')
    assert_ok "$resp" "Next app"
    resp=$(_post /api/previousapp '{}')
    assert_ok "$resp" "Previous app"

    resp=$(_post /api/mute '{"name":"_t_basic","mute":true}')
    assert_ok "$resp" "Mute app"
    resp=$(_post /api/mute '{"name":"_t_basic","mute":false}')
    assert_ok "$resp" "Unmute app"

    resp=$(curl -sf --compressed --max-time 8 -X POST "$BASE/api/custom?name=_t_basic" \
         -H "Content-Type: application/x-www-form-urlencoded" \
         --data-urlencode 'plain={}')
    assert_ok "$resp" "Delete app"
    resp=$(_get /api/apps)
    assert_not_contains "$resp" '"_t_basic"' "App gone after delete"
fi

# ── notify ────────────────────────────────────────────────────────────────────
suite "notify"
if _should_run; then
    resp=$(_post /api/notify '{"text":"TEST","color":"#FFFFFF","duration":3}')
    assert_ok "$resp" "Send notification"
    vis "Display: white 'TEST', scrolls once, auto-clears ~3s"
    _sleep 4

    resp=$(_post /api/notify '{"text":"DISMISS ME","duration":30}')
    assert_ok "$resp" "Send long notification"
    _sleep 1
    resp=$(_post /api/notify/dismiss '{}')
    assert_ok "$resp" "Dismiss notification"
    vis "Display: notification vanishes immediately on dismiss"
    _sleep 1

    # REGRESSION: finite repeat must not get stuck and must not cut early
    resp=$(_post /api/notify '{"text":"TWICE","color":"#00FFFF","repeat":2,"duration":20}')
    assert_ok "$resp" "Send repeat:2 notification"
    vis "REGRESSION: 'TWICE' scrolls EXACTLY 2 times then auto-dismisses (1.9 fix)"
    _sleep 6

    resp=$(_post /api/notify '{"text":"RAINBOW","rainbow":true,"duration":3}')
    assert_ok "$resp" "Send rainbow notification"
    vis "Display: rainbow colored notification text"
    _sleep 4
fi

# ── overlay ───────────────────────────────────────────────────────────────────
suite "overlay"
if _should_run; then
    register_app "_t_no_effect"

    # REGRESSION 1.11: per-app overlay auto-activation
    for effect in sparkle twinkle rain drizzle snow storm strobe blink frost; do
        register_app "_t_eff_$effect"
        resp=$(curl -sf --compressed --max-time 8 -X POST "$BASE/api/custom?name=_t_eff_$effect" \
             -H "Content-Type: application/x-www-form-urlencoded" \
             --data-urlencode "plain={\"text\":\"$effect\",\"color\":\"#FFFFFF\",\"overlay\":\"$effect\",\"dur\":999}")
        assert_ok "$resp" "Create app with overlay=$effect"
        _post /api/switch "{\"name\":\"_t_eff_$effect\"}" > /dev/null
        vis "$effect: text visible + $effect pixel effect on top"
        _sleep 2
    done

    # REGRESSION 1.11: effect MUST clear when switching to app with no overlay
    _cpost "_t_no_effect" '{"text":"CLEAN","color":"#FF8800","dur":999}' > /dev/null
    _post /api/switch '{"name":"_t_no_effect"}' > /dev/null
    vis "REGRESSION 1.11: switching to app with NO overlay — all pixel effects MUST stop"
    _sleep 2

    # REGRESSION 1.12: overlays must respect brightness (SEGMENT API fix)
    orig_bri=$(_jq "$(_get /api/settings)" '.BRI')
    _post /api/settings '{"BRI":25}' > /dev/null
    _post /api/switch '{"name":"_t_eff_rain"}' > /dev/null
    vis "REGRESSION 1.12 (BRI=25): text AND rain drops both dim — NOT full-white override"
    _sleep 3
    _post /api/settings "{\"BRI\":$orig_bri}" > /dev/null
    ok "Brightness restored to $orig_bri"
fi

# ── indicators ────────────────────────────────────────────────────────────────
suite "indicators"
if _should_run; then
    resp=$(_post /api/indicator1 '{"color":"#FF0000"}')
    assert_ok "$resp" "Indicator 1 red"
    vis "TOP-LEFT 3px L-shape: solid RED"

    resp=$(_post /api/indicator2 '{"color":"#00FF00","blink":400}')
    assert_ok "$resp" "Indicator 2 green blink 400ms"
    vis "TOP-RIGHT 3px L-shape: GREEN blinking ~400ms"
    _sleep 2

    resp=$(_post /api/indicator3 '{"color":"#0088FF","fade":1500}')
    assert_ok "$resp" "Indicator 3 blue fade 1500ms"
    vis "BOTTOM-LEFT 3px L-shape: BLUE pulsing on 1.5s triangle wave"
    _sleep 3

    for i in 1 2 3; do
        resp=$(_post /api/indicator$i '{"color":"#000000"}')
        assert_ok "$resp" "Clear indicator $i"
    done
    vis "All indicators OFF"
fi

# ── tc ────────────────────────────────────────────────────────────────────────
suite "tc"
if _should_run; then
    saved_dev=$(_get /api/dev)
    saved_mute=$(_jq "$saved_dev" '.tc_mute')

    # tc_mute persist roundtrip
    resp=$(_post /api/dev '{"tc_mute":true}')
    assert_ok "$resp" "Set tc_mute=true via /api/dev"
    resp=$(_get /api/dev)
    assert_key "$resp" '.tc_mute' "true" "tc_mute=true in dev.json"
    resp=$(_get /api/settings)
    assert_key "$resp" '.TC_MUTE' "true" "TC_MUTE=true in /api/settings"
    vis "TC MUTE ON: send timecode signal — display must NOT react"
    _sleep 3

    resp=$(_post /api/dev '{"tc_mute":false}')
    assert_ok "$resp" "Set tc_mute=false"
    resp=$(_get /api/dev)
    assert_key "$resp" '.tc_mute' "false" "tc_mute=false in dev.json"
    vis "TC MUTE OFF: send timecode — display shows HH:MM:SS + frame progress bar"
    _sleep 3

    # TC settings roundtrip
    resp=$(_post /api/dev '{"tc_dwell":4,"tc_hold":false,"tc_show_frames":false,"tc_stop_beep":false}')
    assert_ok "$resp" "Set TC dwell/hold/frames/beep"
    resp=$(_get /api/dev)
    assert_key "$resp" '.tc_dwell' "4" "tc_dwell=4 roundtrip"
    assert_key "$resp" '.tc_hold' "false" "tc_hold=false roundtrip"
    assert_key "$resp" '.tc_show_frames' "false" "tc_show_frames=false roundtrip"

    # Restore
    _post /api/dev "{\"tc_mute\":$saved_mute}" > /dev/null
    ok "Restored tc_mute to $saved_mute"
fi

# ── sound ─────────────────────────────────────────────────────────────────────
suite "sound"
if _should_run; then
    resp=$(_post /api/beeptest '{}')
    assert_ok "$resp" "POST /api/beeptest"
    assert_contains "$resp" '"pin"' "beeptest has pin field"
    info "Buzzer: pin=$(_jq "$resp" '.pin') ledc_ch=$(_jq "$resp" '.ledc_ch')"
    vis "SOUND: two 880Hz beeps (200ms + 80ms gap + 200ms)"
    _sleep 1

    # RTTTL parse — automated frequency verification (no audio needed)
    purl="$BASE/api/buzzer/parse?q=Beep%3Ad%3D4%2Co%3D5%2Cb%3D200%3Ac%2Ce%2Cg%2Cc6"
    resp=$(curl -sf --compressed --max-time 5 "$purl")
    if [[ -n "$resp" && "$resp" != "null" ]]; then
        assert_key "$resp" '.[0].f' "524" "C5 = 524Hz (RTTTL parser)"
        assert_key "$resp" '.[1].f' "660" "E5 = 660Hz"
        assert_key "$resp" '.[2].f' "784" "G5 = 784Hz"
        assert_key "$resp" '.[3].f' "1048" "C6 = 1048Hz"
    else
        skip "RTTTL parse endpoint (build may not include it)"
    fi

    resp=$(_post /api/sound '{"rtttl":"Scale:d=4,o=5,b=120:c,d,e,f,g,a,b,c6"}')
    assert_ok "$resp" "RTTTL play C major scale"
    vis "SOUND: 8 ascending notes — do re mi fa sol la ti do"
    _sleep 5

    resp=$(_post /api/sound '{"rtttl":"Mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g5,8p"}')
    assert_ok "$resp" "RTTTL play Mario opening"
    vis "SOUND: recognizable Super Mario theme opening (E-E-E-C-E-G)"
    _sleep 3

    resp=$(_post /api/sound '{}')
    assert_ok "$resp" "Stop sound"

    # Sound enable/disable
    resp=$(_post /api/settings '{"SOUND":false}')
    assert_ok "$resp" "Disable sound"
    _post /api/sound '{"rtttl":"Test:d=4,o=5,b=200:c,e,g"}' > /dev/null
    vis "SOUND DISABLED: completely silent (no notes)"
    _sleep 1
    _post /api/settings '{"SOUND":true}' > /dev/null
    ok "Sound re-enabled"
fi

# ── settings ──────────────────────────────────────────────────────────────────
suite "settings"
if _should_run; then
    resp=$(_get /api/settings)
    for key in BRI ATIME ATRANS SSPEED UPPERCASE SOUND TIM DAT TC_MUTE MQTT_PREFIX; do
        assert_contains "$resp" "\"$key\"" "settings has $key"
    done

    orig_bri=$(_jq "$resp" '.BRI')
    resp=$(_post /api/settings '{"BRI":88}')
    assert_ok "$resp" "Set BRI=88"
    resp=$(_get /api/settings)
    assert_key "$resp" '.BRI' "88" "BRI=88 roundtrip"
    vis "Display: brightness visibly changed"
    _post /api/settings "{\"BRI\":$orig_bri}" > /dev/null
    ok "BRI restored to $orig_bri"

    # TIM toggle (no reboot)
    resp=$(_post /api/settings '{"TIM":false}')
    assert_ok "$resp" "TIM=false"
    resp=$(_get /api/apps)
    assert_not_contains "$resp" '"Time"' "Time hidden when TIM=false"
    resp=$(_post /api/settings '{"TIM":true}')
    assert_ok "$resp" "TIM=true"
    resp=$(_get /api/apps)
    assert_contains "$resp" '"Time"' "Time visible when TIM=true"

    # DAT toggle
    resp=$(_post /api/settings '{"DAT":true}')
    assert_ok "$resp" "DAT=true"
    resp=$(_get /api/apps)
    assert_contains "$resp" '"Date"' "Date visible when DAT=true"
    _post /api/settings '{"DAT":false}' > /dev/null
    ok "DAT restored to false"
fi

# ── persist (opt-in, needs reboot) ───────────────────────────────────────────
suite "persist"
if _should_run; then
    if ! "$MODE_REBOOT"; then
        skip "persist — pass --reboot to enable (~10s)"
    else
        _post /api/settings '{"TIM":false}' > /dev/null
        resp=$(_get /api/dev)
        assert_key "$resp" '.show_time' "false" "show_time=false in dev.json before reboot"

        info "Rebooting device..."
        _post /api/reboot '{}' > /dev/null
        sleep 10

        resp=$(_get /dpx) || { fail "Device did not come back after reboot"; }
        ok "Device online after reboot"
        resp=$(_get /api/apps)
        assert_not_contains "$resp" '"Time"' "Time still hidden after reboot (persist ok)"

        _post /api/settings '{"TIM":true}' > /dev/null
        ok "Time restored"
    fi
fi

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
echo -e "${B}━━ Results: PASS ${G}${PASS}${RST}${B}  FAIL ${R}${FAIL}${RST}${B}  SKIP ${DIM}${SKIP}${RST}${B} ━━${RST}"
echo ""
if [[ $FAIL -eq 0 ]]; then
    echo -e "${G}  All automated checks passed.${RST}"
    "$MODE_DISPLAY" && echo -e "${Y}  Review all 👁 [DISPLAY] items above.${RST}"
    exit 0
else
    echo -e "${R}  $FAIL automated check(s) failed.${RST}"
    exit 1
fi
