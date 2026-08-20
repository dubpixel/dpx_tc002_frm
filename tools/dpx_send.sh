#!/usr/bin/env bash
# =============================================================================
# dpx_send.sh — quick one-shot notification sender
# Usage: bash tools/dpx_send.sh "TEXT" [options] [IP]
#
# Options:
#   --scale=N       fontScale (1 default, 2 = large)
#   --repeat=N      scroll exactly N times then auto-dismiss (default: unset,
#                   duration governs instead — use this for text you want to
#                   guarantee finishes scrolling, especially at scale=2)
#   --duration=N    seconds, time-based mode (ignored if --repeat set)
#   --rainbow       rainbow text
#   --color=#RRGGBB text color
#
# Wraps the plain=<urlencoded-json> wire format /api/notify actually requires
# (raw JSON bodies silently no-op) so this doesn't need re-deriving every time.
# =============================================================================
set -euo pipefail

TEXT="${1:?Usage: dpx_send.sh \"TEXT\" [options] [IP]}"; shift || true

SCALE="" REPEAT="" DURATION="" RAINBOW="" COLOR="" IP="192.168.1.144"
for a in "$@"; do case "$a" in
    --scale=*)    SCALE="${a#*=}" ;;
    --repeat=*)   REPEAT="${a#*=}" ;;
    --duration=*) DURATION="${a#*=}" ;;
    --rainbow)    RAINBOW=1 ;;
    --color=*)    COLOR="${a#*=}" ;;
    *)            IP="$a" ;;
esac; done

json=$(python3 -c "
import json, sys
d = {'text': sys.argv[1]}
if sys.argv[2]: d['fontScale'] = int(sys.argv[2])
if sys.argv[3]: d['repeat'] = int(sys.argv[3])
if sys.argv[4]: d['duration'] = int(sys.argv[4])
if sys.argv[5]: d['rainbow'] = True
if sys.argv[6]: d['color'] = sys.argv[6]
print(json.dumps(d))
" "$TEXT" "$SCALE" "$REPEAT" "$DURATION" "$RAINBOW" "$COLOR")

curl -s --max-time 5 -X POST "http://$IP/api/notify" \
     -H "Content-Type: application/x-www-form-urlencoded" \
     --data-urlencode "plain=$json"
echo
