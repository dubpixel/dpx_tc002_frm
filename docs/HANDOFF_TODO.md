# dpx_tc002_frm → friendster integration reference

Last updated: 2026-08-21

What the firmware provides today for the friendster/cueMaestro server to
build against. Not a roadmap or a TODO list — just what exists right now,
verified against the actual source. If you're looking for firmware-internal
detail (GPIO maps, icon rendering, LED matrix internals), you don't need it
for server-side work; it isn't included here.

---

## Device identity

Every device is identified by the same mac-suffixed string in three places,
set on first boot (`usermods/dpx_matrix/dpx_firstboot.h`):

- `id.name` (WLED's device name)
- `id.mdns` (`dpx-tc002-XXXXXX.local`)
- `ap.ssid` (the AP network name before the device joins WiFi)

`XXXXXX` = last 3 bytes of the device's MAC, lowercase hex. This means the
AP name you join, the mDNS hostname, and the device's reported name are
always the same string — no separate lookup needed. (Previously `id.name`
was hardcoded to the literal string `"dpx_tc002"` for every unit, making
devices indistinguishable — fixed 2026-08-21.)

## MQTT — the primary integration channel

**Every device ships with a default MQTT broker baked in** (`mb.dubpixel.tv:1883`,
no auth), set on first boot. This exists specifically so a factory-fresh
device is claimable out of the box: friendster's device-claim flow is
**MQTT-only** — it publishes the claim PIN via
`mqtt_client.publish(f"{prefix}/dpx/pair", ...)` — there is no HTTP fallback.
A device with no MQTT config is not claimable at all.

Device topics are nested as `wled/<mac>/...` (WLED's own `mqttDeviceTopic`),
not a flat `<name>/...`. A single-level `+/presence` subscription won't
match — subscribe `wled/+/status` (WLED core's own LWT presence, "online"/
"offline") and `wled/+/dpx/info`.

Full topic list and payload shapes are documented in the header comment of
`usermods/dpx_matrix/dpx_mqtt.h` — that file is the source of truth, treat
this list as a pointer to it, not a copy that can drift out of sync:

**Server → device** (subscribe root `{mqttDeviceTopic}/dpx/#`, alias `dpx/#`):
- `dpx/pair` — display a claim PIN full-screen (`{"pin","duration","scale"}`, or `""` to clear)
- `dpx/notify`, `dpx/notify/dismiss` — push/dismiss a notification
- `dpx/switch`, `dpx/nextapp`, `dpx/previousapp` — app loop control
- `dpx/app/<name>` — create/update a custom app
- `dpx/indicator/<1-3>` — set an indicator pixel (color/blink/fade)
- `dpx/power`, `dpx/brightness`, `dpx/rtttl`, `dpx/tc`, `dpx/overlay`, `dpx/effect`, `dpx/mute/<name>`
- `dpx/icon/list` → response on `dpx/icon/list/result`
- `dpx/icon/get/<name>` → response on `dpx/icon/data/<name>` (raw 192-byte RGB888, 8×8)

**Device → server:**
- `dpx/info` — retained, on connect: `{"name","ip","mac","build"}`
- `dpx/appstate` — retained, QoS 1, published on change: `{"app","source","text","color","rainbow","icon","type"}`.
  A lighter, redraw-able echo of whatever's currently on screen (app/notify/history)
  — for a cloud-hosted server that has no LAN path to `GET /api/screen`. Not
  pixel-exact (doesn't reflect overlay effects).

## HTTP — LAN-only, no cloud path

These only work if the server can reach the device directly (same LAN, or a
tunnel) — not usable from a cloud-hosted server on a device sitting on venue
WiFi. Full list in `usermods/dpx_matrix/dpx_api.h`; the ones most relevant to
mirror MQTT functionality:

`/api/pair`, `/api/notify`, `/api/switch`, `/api/nextapp`/`previousapp`,
`/api/power`, `/api/settings` (GET returns `MQTT_PREFIX` — the device's
`mqttDeviceTopic` value, if you need to derive the MQTT topic root from an
HTTP call), `/api/screen` (pixel-exact live view, LAN-only), `/api/list`,
`/api/apps`, `/api/effects`, `/api/transitions`, `/api/stats`.

## Open items on the server side

Not firmware gaps — things the firmware already exposes that friendster may
not be consuming yet, worth checking before assuming something needs to be
built:
- Icon list/get over MQTT (`dpx/icon/list`, `dpx/icon/get/<name>`) — lets a
  cross-network icon picker work without LAN access.
- `dpx/appstate` — if friendster still only reads `dpx/info` for presence
  and doesn't render a live state preview, this is what feeds that.
