// ================================================================================
// dpx_html.h — PROGMEM Web UI Pages
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// Pages: /api-ref, /browse, /ctrl, /screen, /fullscreen
// Ported from dpx_tc001/src/htmls.h (original work, not AWTRIX-derived).
// EXCLUDED: custom_html, screen_html, backup_html (contain AWTRIX references).
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================

#pragma once

static const char apiref_html[] PROGMEM = R"APIREF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>API Reference</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0e0e18;color:#ccc;font-family:monospace;font-size:12px;padding:14px;line-height:1.5}
h1{color:#4af;font-size:17px;margin-bottom:6px}
h2{color:#8cf;font-size:13px;margin:18px 0 6px;border-bottom:1px solid #2a2a4a;padding-bottom:4px}
h3{color:#adf;font-size:12px;margin:12px 0 4px}
nav a{color:#4af;text-decoration:none;margin-right:12px}
nav{margin-bottom:12px}
table{width:100%;border-collapse:collapse;margin-bottom:8px;font-size:11px}
th{background:#1a1a2e;color:#8cf;text-align:left;padding:4px 7px;border:1px solid #2a2a4a}
td{padding:3px 7px;border:1px solid #1e1e30;vertical-align:top}
tr:nth-child(even){background:#111120}
code,.snip{background:#1a1a2e;color:#8cf;padding:1px 4px;border-radius:3px;font-family:monospace}
.block{position:relative;background:#131325;border:1px solid #2a2a4a;border-radius:4px;padding:8px 36px 8px 10px;margin:4px 0;white-space:pre-wrap;font-size:11px;color:#aaa}
.cp{position:absolute;top:6px;right:6px;background:#2a5298;color:#fff;border:none;border-radius:3px;padding:2px 7px;cursor:pointer;font-size:10px}
.cp:hover{background:#3a63b8}
.pill{display:inline-block;padding:1px 6px;border-radius:3px;font-size:10px;margin-right:3px}
.get{background:#1a4a1a;color:#6f6}
.post{background:#3a2a10;color:#fa6}
.tag{color:#555;font-size:10px}
.toc{columns:2;margin-bottom:10px}
.toc a{color:#4af;text-decoration:none;display:block;line-height:1.8;font-size:11px}
.toast{position:fixed;bottom:12px;right:12px;background:#285;color:#fff;padding:6px 14px;border-radius:5px;display:none;font-size:11px;z-index:999}
</style></head><body>
<h1>&#128196; API Reference</h1>
<nav><a href="/ctrl">&#9664; Control</a><a href="/browse">&#9782; Browse</a></nav>

<div class="toc">
<a href="#endpoints">HTTP Endpoints</a>
<a href="#notify">Notify / Custom App JSON</a>
<a href="#settings">Settings Keys</a>
<a href="#effects">Effects &amp; Overlays</a>
<a href="#draw">Draw Commands</a>
<a href="#osc">OSC Addresses</a>
<a href="#mqtt">MQTT Topics</a>
</div>

<!-- ── ENDPOINTS ── -->
<h2 id="endpoints">HTTP Endpoints</h2>
<table>
<tr><th>Method</th><th>URL</th><th>Body / Notes</th></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/api/stats</code></td><td>Device stats (RAM, uptime, battery…)</td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/api/screen</code></td><td>Current matrix as 256× 24-bit int array</td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/api/effects</code></td><td>Array of effect name strings</td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/api/transitions</code></td><td>Array of transition name strings</td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/api/loop</code></td><td>Ordered app list</td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/api/apps</code></td><td>App list with icons</td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/api/settings</code></td><td>Current settings JSON</td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/notify</code></td><td>One-shot notification — see JSON keys below</td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/notify/dismiss</code></td><td>Dismiss held notification (empty body)</td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/custom?name=<i>appname</i></code></td><td>Create/update persistent custom app</td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/switch</code></td><td><code>{"name":"Time"}</code> — jump to app</td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/nextapp</code></td><td>empty body</td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/previousapp</code></td><td>empty body</td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/power</code></td><td><code>{"power":true}</code></td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/sleep</code></td><td><code>{"sleep":60}</code> — seconds</td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/settings</code></td><td>Settings JSON — see keys below</td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/moodlight</code></td><td><code>{"color":[255,80,0],"brightness":170}</code></td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/indicator1</code> <code>/2</code> <code>/3</code></td><td><code>{"color":[255,0,0],"blink":500}</code></td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/rtttl</code></td><td>Raw RTTTL string as plain body</td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/sound</code></td><td><code>{"sound":"alarm"}</code> or <code>{"rtttl":"..."}</code> — loads <code>/MELODIES/&lt;name&gt;.txt</code></td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/api/time</code></td><td>Current device time: <code>{"local":"2026-07-14T15:30:00","utc":1752506200}</code></td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/time</code></td><td><code>{"utc":1752506200}</code> — set clock via settimeofday()</td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/syncntp</code></td><td>Re-trigger NTP sync with current server + timezone settings</td></tr>
<tr><td><span class="pill post">POST</span></td><td><code>/api/rename</code></td><td><code>{"from":"/ICONS/old.jpg","to":"/ICONS/new.jpg"}</code></td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/api/reboot</code></td><td>Reboot device</td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/screen</code></td><td>Live view page</td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/ctrl</code></td><td>Control panel</td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/browse</code></td><td>Community browser</td></tr>
</table>

<h3>Quick curl examples</h3>
<div class="block">curl -X POST http://[IP]/api/notify \
  -H "Content-Type: application/json" \
  -d '{"text":"Hello!","color":[255,200,0],"icon":"87","duration":8}'<button class="cp" onclick="cp(this)">copy</button></div>
<div class="block">curl -X POST http://[IP]/api/custom?name=myapp \
  -H "Content-Type: application/json" \
  -d '{"text":"Always on","rainbow":true,"scrollSpeed":80}'<button class="cp" onclick="cp(this)">copy</button></div>

<!-- ── NOTIFY JSON ── -->
<h2 id="notify">Notify &amp; Custom App — JSON Keys</h2>
<p style="color:#666;margin-bottom:6px">All keys optional. <span class="tag">N=notify only &nbsp; C=custom app only &nbsp; B=both</span></p>
<table>
<tr><th>Key</th><th>Type</th><th>Description</th><th>Default</th><th></th></tr>
<tr><td><code>text</code></td><td>string/array</td><td>Text to display. Array = colored fragments: <code>[{"t":"Hi","c":"FF0000"}]</code></td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>textCase</code></td><td>int</td><td>0=global, 1=UPPER, 2=as-sent</td><td>0</td><td class="tag">B</td></tr>
<tr><td><code>topText</code></td><td>bool</td><td>Draw text on top row</td><td>false</td><td class="tag">B</td></tr>
<tr><td><code>textOffset</code></td><td>int</td><td>X-offset for text start</td><td>0</td><td class="tag">B</td></tr>
<tr><td><code>center</code></td><td>bool</td><td>Center short non-scrolling text</td><td>true</td><td class="tag">B</td></tr>
<tr><td><code>color</code></td><td>str/[r,g,b]</td><td>Text/bar/line color</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>gradient</code></td><td>[c1,c2]</td><td>Two-color text gradient</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>background</code></td><td>str/[r,g,b]</td><td>Background fill color</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>rainbow</code></td><td>bool</td><td>RGB rainbow per letter</td><td>false</td><td class="tag">B</td></tr>
<tr><td><code>blinkText</code></td><td>int</td><td>Blink text every N ms</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>fadeText</code></td><td>int</td><td>Fade text in/out every N ms</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>icon</code></td><td>string</td><td>Icon filename (no ext) or LaMetric ID, or 8×8 JPG as base64</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>pushIcon</code></td><td>int</td><td>0=fixed, 1=scroll+gone, 2=scroll+loop</td><td>0</td><td class="tag">B</td></tr>
<tr><td><code>noScroll</code></td><td>bool</td><td>Disable text scrolling</td><td>false</td><td class="tag">B</td></tr>
<tr><td><code>scrollSpeed</code></td><td>int</td><td>Scroll speed % of default</td><td>100</td><td class="tag">B</td></tr>
<tr><td><code>duration</code></td><td>int</td><td>Display time in seconds</td><td>5</td><td class="tag">B</td></tr>
<tr><td><code>repeat</code></td><td>int</td><td>Times to scroll before ending (-1=forever)</td><td>-1</td><td class="tag">B</td></tr>
<tr><td><code>hold</code></td><td>bool</td><td>Hold notification until dismissed</td><td>false</td><td class="tag">N</td></tr>
<tr><td><code>stack</code></td><td>bool</td><td>Stack notification; false=replace current</td><td>true</td><td class="tag">N</td></tr>
<tr><td><code>wakeup</code></td><td>bool</td><td>Wake matrix from sleep for notification</td><td>false</td><td class="tag">N</td></tr>
<tr><td><code>sound</code></td><td>string</td><td>RTTTL filename (no ext) or DFPlayer 4-digit number</td><td>—</td><td class="tag">N</td></tr>
<tr><td><code>rtttl</code></td><td>string</td><td>Inline RTTTL string</td><td>—</td><td class="tag">N</td></tr>
<tr><td><code>loopSound</code></td><td>bool</td><td>Loop sound for notification duration</td><td>false</td><td class="tag">N</td></tr>
<tr><td><code>effect</code></td><td>string</td><td>Background effect name (see Effects)</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>effectSettings</code></td><td>object</td><td><code>{"speed":3,"palette":"Rainbow","blend":true}</code></td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>overlay</code></td><td>string</td><td>Per-app overlay: snow/rain/drizzle/storm/thunder/frost/clear</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>bar</code></td><td>int[]</td><td>Bar chart data (max 16 values, 11 with icon)</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>line</code></td><td>int[]</td><td>Line chart data (same limits)</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>autoscale</code></td><td>bool</td><td>Autoscale bar/line chart</td><td>true</td><td class="tag">B</td></tr>
<tr><td><code>barBC</code></td><td>str/[r,g,b]</td><td>Bar background color</td><td>0</td><td class="tag">B</td></tr>
<tr><td><code>progress</code></td><td>int</td><td>Progress bar 0–100 (-1=off)</td><td>-1</td><td class="tag">B</td></tr>
<tr><td><code>progressC</code></td><td>str/[r,g,b]</td><td>Progress bar fill color</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>progressBC</code></td><td>str/[r,g,b]</td><td>Progress bar background color</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>draw</code></td><td>object[]</td><td>Drawing instructions array — see Draw Commands</td><td>—</td><td class="tag">B</td></tr>
<tr><td><code>lifetime</code></td><td>int</td><td>Remove custom app if no update after N seconds</td><td>0</td><td class="tag">C</td></tr>
<tr><td><code>pos</code></td><td>int</td><td>Loop position (0-based, set on first push)</td><td>—</td><td class="tag">C</td></tr>
<tr><td><code>save</code></td><td>bool</td><td>Persist custom app across reboots (avoid for high-freq updates)</td><td>false</td><td class="tag">C</td></tr>
<tr><td><code>clients</code></td><td>string[]</td><td>Forward notification to other devices (IP or MQTT prefix)</td><td>—</td><td class="tag">N</td></tr>
</table>

<!-- ── SETTINGS ── -->
<h2 id="settings">Settings Keys  <span class="tag">— POST /api/settings or GET to read</span></h2>
<table>
<tr><th>Key</th><th>Type</th><th>Description</th><th>Range/Default</th></tr>
<tr><td><code>BRI</code></td><td>int</td><td>Brightness — applied to WLED main brightness</td><td>0–255</td></tr>
<tr><td><code>ATIME</code></td><td>int</td><td>App display duration per rotation slot (seconds)</td><td>&gt;0</td></tr>
<tr><td><code>ATRANS</code></td><td>bool</td><td>Auto-advance through app rotation</td><td>true/false</td></tr>
<tr><td><code>SSPEED</code></td><td>int</td><td>Global scroll speed (% of default)</td><td>1–200</td></tr>
<tr><td><code>UPPERCASE</code></td><td>bool</td><td>Force all text to uppercase globally</td><td>true/false</td></tr>
<tr><td><code>Timezone</code></td><td>string</td><td>POSIX timezone string e.g. <code>PST8PDT,M3.2.0,M11.1.0</code></td><td>—</td></tr>
<tr><td><code>MQTT_PREFIX</code></td><td>string</td><td>Read-only — MQTT device topic prefix</td><td>—</td></tr>
<tr><td><code>SOUND</code></td><td>bool</td><td>Buzzer/piezo enabled</td><td>true</td></tr>
<tr><td><code>VOL</code></td><td>int</td><td>Volume (passive piezo — no-op)</td><td>0–30</td></tr>
<tr><td><code>TIM</code></td><td>bool</td><td>Include Time app in rotation</td><td>true</td></tr>
<tr><td><code>DAT</code></td><td>bool</td><td>Include Date app in rotation</td><td>false</td></tr>
</table>
<p style="color:#555;font-size:10px;margin-top:4px">Phase 1.8: <code>ABRI</code>, <code>CEL</code>, <code>TEMP</code>, <code>HUM</code>, <code>BAT</code> — pending SHT3x / battery ADC / LDR firmware support.</p>

<!-- ── WLED JSON API ── -->
<h2 id="wled">WLED JSON API  <span class="tag">— native WLED endpoints, always available</span></h2>
<p style="color:#666;font-size:11px;margin-bottom:6px">WLED's built-in REST API. Use these for brightness, effects, segments, and colours. Full docs at <a href="https://kno.wled.ge/interfaces/json-api/" target="_blank" style="color:#4af">kno.wled.ge/interfaces/json-api</a>.</p>
<table>
<tr><th>Method</th><th>URL</th><th>Body / Notes</th></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/json</code></td><td>Full state + info + effect/palette lists combined</td></tr>
<tr><td><span class="pill get">GET</span><span class="pill post">POST</span></td><td><code>/json/state</code></td><td>WLED state: <code>{"on":true,"bri":128,"seg":[{"fx":0,"col":[[255,0,0]]}]}</code></td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/json/info</code></td><td>Device info: firmware version, LED count, WiFi, etc.</td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/json/eff</code></td><td>Effect name array (also available as <code>GET /api/effects</code>)</td></tr>
<tr><td><span class="pill get">GET</span></td><td><code>/json/palettes</code></td><td>Palette name array</td></tr>
</table>

<!-- ── EFFECTS ── -->
<h2 id="effects">Effects &amp; Overlays  <span class="tag">— fetched live from device</span></h2>
<div class="row" style="display:flex;gap:16px;flex-wrap:wrap">
<div style="flex:1;min-width:180px">
  <h3>Background Effects</h3>
  <div id="fx_list" style="color:#666">Loading...</div>
</div>
<div style="flex:1;min-width:180px">
  <h3>Overlay Effects</h3>
  <div style="color:#8cf">clear · snow · rain · drizzle · storm · thunder · frost</div>
</div>
<div style="flex:1;min-width:180px">
  <h3>App Transitions</h3>
  <div id="tr_list" style="color:#666">Loading...</div>
</div>
</div>

<!-- ── DRAW ── -->
<h2 id="draw">Draw Commands  <span class="tag">— "draw":[...] in notify/custom</span></h2>
<table>
<tr><th>Command</th><th>Values</th><th>Description</th></tr>
<tr><td><code>dp</code></td><td><code>[x,y,color]</code></td><td>Draw pixel</td></tr>
<tr><td><code>dl</code></td><td><code>[x0,y0,x1,y1,color]</code></td><td>Draw line</td></tr>
<tr><td><code>dr</code></td><td><code>[x,y,w,h,color]</code></td><td>Rectangle outline</td></tr>
<tr><td><code>df</code></td><td><code>[x,y,w,h,color]</code></td><td>Filled rectangle</td></tr>
<tr><td><code>dc</code></td><td><code>[x,y,r,color]</code></td><td>Circle outline</td></tr>
<tr><td><code>dfc</code></td><td><code>[x,y,r,color]</code></td><td>Filled circle</td></tr>
<tr><td><code>dt</code></td><td><code>[x,y,"text",color]</code></td><td>Draw text</td></tr>
<tr><td><code>db</code></td><td><code>[x,y,w,h,[rgb888array]]</code></td><td>Bitmap array</td></tr>
</table>
<div class="block">{"draw":[{"df":[0,0,8,8,"#1a1aff"]},{"dt":[9,1,"Hi","#ffffff"]}]}<button class="cp" onclick="cp(this)">copy</button></div>

<!-- ── OSC ── -->
<h2 id="osc">OSC (UDP port 4210)</h2>
<p style="color:#666;margin-bottom:6px">Send OSC UDP packets to port <b style="color:#fff">4210</b>. Both bare addresses and <code>/dpx_tc002/</code> or <code>/awtrix/</code> namespace prefixes are accepted.</p>
<table>
<tr><th>Address</th><th>Args</th><th>Action</th></tr>
<tr><td><code>/notify</code> or <code>/text</code></td><td>(s) text</td><td>One-shot notification</td></tr>
<tr><td><code>/custom/&lt;name&gt;</code></td><td>(s) text</td><td>Update/create persistent app</td></tr>
<tr><td><code>/switch</code></td><td>(s) appname</td><td>Switch to named app</td></tr>
<tr><td><code>/nextapp</code></td><td>none</td><td>Next app</td></tr>
<tr><td><code>/previousapp</code></td><td>none</td><td>Previous app</td></tr>
<tr><td><code>/power</code></td><td>(i|f) 0/1</td><td>Power off/on</td></tr>
<tr><td><code>/brightness</code></td><td>(i|f) 0–255</td><td>Set brightness</td></tr>
<tr><td><code>/indicator/1</code> <code>/2</code> <code>/3</code></td><td>(i i i) r g b</td><td>Set indicator color</td></tr>
</table>
<div class="block"># python-osc example
from pythonosc import udp_client
c = udp_client.SimpleUDPClient("192.168.x.x", 4210)
c.send_message("/notify", "01:23:45:12")
c.send_message("/dpx_tc002/custom/tc", "00:59:59:24")<button class="cp" onclick="cp(this)">copy</button></div>

<!-- ── MQTT ── -->
<h2 id="mqtt">MQTT  <span class="tag">— broker configured in /setup · prefix set per device</span></h2>
<p style="color:#666;margin-bottom:6px">All topics use your configured prefix (default: device hostname, e.g. <code>awtrix_a1b2c3</code>). Format: <code>[PREFIX]/topic</code>.</p>

<h3>Inbound — device subscribes, you publish</h3>
<table>
<tr><th>Topic</th><th>Payload</th><th>Action</th></tr>
<tr><td><code>[PREFIX]/notify</code></td><td>JSON</td><td>One-shot notification — same JSON keys as HTTP notify</td></tr>
<tr><td><code>[PREFIX]/notify/dismiss</code></td><td>empty</td><td>Dismiss held notification</td></tr>
<tr><td><code>[PREFIX]/custom/[appname]</code></td><td>JSON or empty</td><td>Create/update custom app; empty payload = delete</td></tr>
<tr><td><code>[PREFIX]/settings</code></td><td>JSON</td><td>Update settings — same keys as HTTP settings</td></tr>
<tr><td><code>[PREFIX]/switch</code></td><td><code>{"name":"Time"}</code></td><td>Switch to named app</td></tr>
<tr><td><code>[PREFIX]/nextapp</code></td><td>any</td><td>Next app</td></tr>
<tr><td><code>[PREFIX]/previousapp</code></td><td>any</td><td>Previous app</td></tr>
<tr><td><code>[PREFIX]/power</code></td><td><code>{"power":true}</code></td><td>Power on/off</td></tr>
<tr><td><code>[PREFIX]/sleep</code></td><td><code>{"sleep":60}</code></td><td>Deep sleep for N seconds</td></tr>
<tr><td><code>[PREFIX]/moodlight</code></td><td>JSON or empty</td><td>Set moodlight; empty = disable</td></tr>
<tr><td><code>[PREFIX]/indicator1</code></td><td><code>{"color":[r,g,b],"blink":500}</code></td><td>Indicator 1</td></tr>
<tr><td><code>[PREFIX]/indicator2</code></td><td>same</td><td>Indicator 2</td></tr>
<tr><td><code>[PREFIX]/indicator3</code></td><td>same</td><td>Indicator 3</td></tr>
<tr><td><code>[PREFIX]/sound</code></td><td><code>{"sound":"alarm"}</code></td><td>Play sound file from /MELODIES/</td></tr>
<tr><td><code>[PREFIX]/rtttl</code></td><td>raw RTTTL string</td><td>Play inline RTTTL melody</td></tr>
<tr><td><code>[PREFIX]/apps</code></td><td>JSON array</td><td>Update app loop order</td></tr>
<tr><td><code>[PREFIX]/sendscreen</code></td><td>any</td><td>Request device to publish current screen state</td></tr>
<tr><td><code>[PREFIX]/doupdate</code></td><td>any</td><td>Trigger OTA firmware update</td></tr>
<tr><td><code>[PREFIX]/reboot</code></td><td>any</td><td>Reboot device</td></tr>
<tr><td><code>[PREFIX]/r2d2</code></td><td>any</td><td>Play R2D2 sound effect</td></tr>
</table>

<h3>Outbound — device publishes, you subscribe</h3>
<table>
<tr><th>Topic</th><th>Payload</th><th>When</th></tr>
<tr><td><code>[PREFIX]/stats</code></td><td>JSON</td><td>Periodic stats: battery, RAM, uptime, temp, humidity, lux, RSSI, IP, version</td></tr>
<tr><td><code>[PREFIX]/stats/currentApp</code></td><td>string</td><td>App name each time display switches</td></tr>
<tr><td><code>[PREFIX]/stats/effects</code></td><td>JSON array</td><td>Published on connect — list of effect names</td></tr>
<tr><td><code>[PREFIX]/stats/transitions</code></td><td>JSON array</td><td>Published on connect — list of transition names</td></tr>
<tr><td><code>[PREFIX]/stats/device</code></td><td><code>online</code></td><td>Published on connect</td></tr>
<tr><td><code>[PREFIX]/screen</code></td><td>JSON array (256× 24-bit int)</td><td>Response to <code>/sendscreen</code></td></tr>
<tr><td><code>[PREFIX]/buttonLeft</code></td><td><code>true</code>/<code>false</code></td><td>Physical button press/release</td></tr>
<tr><td><code>[PREFIX]/buttonRight</code></td><td><code>true</code>/<code>false</code></td><td>Physical button press/release</td></tr>
<tr><td><code>[PREFIX]/buttonSelect</code></td><td><code>true</code>/<code>false</code></td><td>Middle button press/release</td></tr>
</table>

<div class="block"># mosquitto example — send notification
mosquitto_pub -h 192.168.x.x -t "awtrix_a1b2c3/notify" \
  -m '{"text":"hello","rainbow":true,"duration":8}'

# create a persistent app
mosquitto_pub -h 192.168.x.x -t "awtrix_a1b2c3/custom/myticker" \
  -m '{"text":"LIVE","color":[255,0,0],"scrollSpeed":80}'

# subscribe to button presses
mosquitto_sub -h 192.168.x.x -t "awtrix_a1b2c3/button+"<button class="cp" onclick="cp(this)">copy</button></div>

<div class="toast" id="toast"></div>
<script>
function cp(btn){
  var txt=btn.parentElement.textContent.replace(/copy$/,"").trim();
  navigator.clipboard.writeText(txt).then(function(){
    var t=document.getElementById("toast");
    t.textContent="Copied!";t.style.display="block";
    clearTimeout(t._t);t._t=setTimeout(function(){t.style.display="none";},1800);
  });
}
fetch("/api/effects").then(function(r){return r.json();}).then(function(fx){
  document.getElementById("fx_list").innerHTML=fx.map(function(e){return '<span style="color:#8cf">'+e+'</span>';}).join(" &middot; ");
}).catch(function(){document.getElementById("fx_list").textContent="(connect device to see)";});
fetch("/api/transitions").then(function(r){return r.json();}).then(function(tr){
  document.getElementById("tr_list").innerHTML=tr.map(function(e){return '<span style="color:#8cf">'+e+'</span>';}).join(" &middot; ");
}).catch(function(){document.getElementById("tr_list").textContent="(connect device to see)";});
</script></body></html>
)APIREF";

// ── Community Browser page (/browse) ─────────────────────────────────────────
// Three tabs:
//  1. LaMetric Icons  – paginated grid of thumbnails loaded via <img> (no CORS),
//     one-click install to /ICONS/ on the device.  Clicking any installed icon
//     copies the usage snippet to the clipboard.
//  2. Bigtime GIFs   – lists files from Blueforcer/awtrix3 on GitHub, one-click
//     install to the device root (used by TMODE=5).
//  3. On-Device Files – lists /ICONS/, /MELODIES/, root; allows delete.
static const char browse_html[] PROGMEM = R"BROWSE(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Browse</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#111;color:#ddd;font-family:monospace;font-size:12px;padding:12px}
h1{color:#4af;font-size:16px;margin-bottom:8px}
nav a{color:#4af;text-decoration:none;margin-right:12px}
nav{margin-bottom:10px}
.tabs{display:flex;gap:0;border-bottom:1px solid #333;margin-bottom:10px}
.tab{padding:6px 14px;cursor:pointer;color:#888;border-bottom:2px solid transparent}
.tab.on{color:#fff;border-bottom-color:#4af}
.pnl{display:none}.pnl.on{display:block}
.row{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin-bottom:8px}
input[type=text],input[type=number]{background:#222;color:#eee;border:1px solid #444;border-radius:4px;padding:4px 7px;font-family:monospace;font-size:12px}
button{background:#2a5298;color:#fff;border:none;border-radius:4px;padding:5px 11px;cursor:pointer;font:12px monospace}
button:hover{background:#3a63b8}
button.red{background:#822}button.red:hover{background:#a33}
button.sm{padding:3px 8px;font-size:11px}
.grid{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}
.ic{background:#1a1a2e;border:1px solid #2a2a4a;border-radius:4px;padding:5px;text-align:center;width:72px;cursor:default}
.ic:hover{border-color:#4af}
.ic img{width:36px;height:36px;image-rendering:pixelated;background:#000;display:block;margin:0 auto 3px}
.ic .lbl{font-size:10px;color:#888;display:block;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.ic .btns{display:flex;gap:3px;margin-top:4px;justify-content:center}
.fitem{display:flex;align-items:center;justify-content:space-between;background:#1a1a2e;border:1px solid #2a2a4a;border-radius:4px;padding:5px 10px;margin-bottom:3px}
.gifcard{display:flex;align-items:center;gap:10px;background:#1a1a2e;border:1px solid #2a2a4a;border-radius:4px;padding:8px;margin-bottom:5px}
.toast{position:fixed;bottom:14px;right:14px;padding:8px 16px;border-radius:6px;font-size:12px;display:none;z-index:999;color:#fff}
h3{color:#8cf;font-size:12px;margin:10px 0 5px}
</style></head><body>
<h1>&#9782; Browse &amp; Install</h1>
<nav><a href="/ctrl">&#9664; Control</a><a href="/setup">&#9881; Settings</a><a href="/screen" target="_blank">&#9654; Live</a></nav>

<div class="tabs">
  <div class="tab on" onclick="tab('icons',this)">LaMetric Icons</div>
  <div class="tab" onclick="tab('gifs',this)">Bigtime GIFs</div>
  <div class="tab" onclick="tab('files',this)">On-Device Files</div>
</div>

<!-- ── ICONS ── -->
<div class="pnl on" id="pnl_icons">
  <p style="color:#666;margin-bottom:8px">Click <b style="color:#fff">Get</b> to install an icon to the device. Once installed, click <b style="color:#fff">Use</b> to copy its ID — paste it as the <code style="color:#8cf">icon</code> value in any notify/custom-app call.</p>
  <div class="row">
    <input type="number" id="ic_id" placeholder="Icon ID" style="width:90px" onkeydown="if(event.key==='Enter')searchId()">
    <button onclick="searchId()">&#128269; Search ID</button>
    <span style="color:#555">|</span>
    <button onclick="pg(-1)">&#9664;</button>
    <span style="color:#999">page</span>
    <input type="number" id="pg_n" value="0" min="0" style="width:48px" onchange="loadPage(+this.value)">
    <button onclick="pg(1)">&#9654;</button>
    <span style="color:#555;font-size:11px">(50/page)</span>
  </div>
  <div class="grid" id="ic_grid"></div>
</div>

<!-- ── GIFS ── -->
<div class="pnl" id="pnl_gifs">
  <p style="color:#666;margin-bottom:8px">Official Bigtime GIFs — install to device root, then set <code style="color:#8cf">TMODE=5</code> and name the file <code style="color:#8cf">bigtime.gif</code>.</p>
  <div id="gif_list"><span style="color:#666">Loading from GitHub...</span></div>
</div>

<!-- ── FILES ── -->
<div class="pnl" id="pnl_files">
  <div class="row"><button onclick="loadFiles()">&#8635; Refresh</button></div>
  <h3>ICONS/</h3><div id="f_icons"><span style="color:#666">Loading...</span></div>
  <h3>MELODIES/</h3><div id="f_mel"><span style="color:#666">Loading...</span></div>
  <h3>Root</h3><div id="f_root"><span style="color:#666">Loading...</span></div>
</div>

<div class="toast" id="toast"></div>
<script>
// Popular icon IDs curated from the LaMetric gallery
var POPULAR=[87,230,4584,1782,2867,6881,3061,61,82,3793,66,37,653,3253,5588,
5319,8520,3741,413,7756,5610,4688,178,4007,76,102,3637,469,6219,96,555,4297,
397,34,1820,5965,8277,1134,4033,386,2285,3049,1852,1624,59,4075,5252,315,73,
2325,7660,4741,6215,7922,77,8660,5266,95,5066,794,1549,18854,7896,3665,2772,
294,71,5838,4789,4738,4373,5446,128,28817,5751,25861,2545,7020,332,74];

var installed={};
var curPage=-1; // -1 = popular

function toast(m,ok){
  var t=document.getElementById("toast");
  t.textContent=m;t.style.background=ok===false?"#822":"#285";
  t.style.display="block";clearTimeout(t._tid);
  t._tid=setTimeout(function(){t.style.display="none"},2800);
}

function tab(id,el){
  document.querySelectorAll(".tab").forEach(function(t){t.classList.remove("on");});
  document.querySelectorAll(".pnl").forEach(function(p){p.classList.remove("on");});
  el.classList.add("on");
  document.getElementById("pnl_"+id).classList.add("on");
  if(id==="gifs")loadGifs();
  if(id==="files")loadFiles();
}

// ── icon card ─────────────────────────────────────────────────────────────
function thumbUrl(id){return "https://developer.lametric.com/content/apps/icon_thumbs/"+id+"_icon_thumb.png";}

function makeCard(id){
  var d=document.createElement("div");d.className="ic";d.id="ic_"+id;
  var img=document.createElement("img");
  img.src=thumbUrl(id);img.alt=id;
  img.onerror=function(){d.style.display="none";};
  var lbl=document.createElement("span");lbl.className="lbl";lbl.textContent="#"+id;
  var btns=document.createElement("div");btns.className="btns";
  var bGet=document.createElement("button");bGet.className="sm";bGet.textContent="Get";
  bGet.title="Install icon #"+id+" to /ICONS/ on the device";
  bGet.onclick=function(){installIcon(id,bGet);};
  var bUse=document.createElement("button");bUse.className="sm";bUse.textContent="Use";
  bUse.title='Copy "icon":"'+id+'" to clipboard';
  bUse.style.background="#285";
  bUse.onclick=function(){
    navigator.clipboard.writeText(String(id)).then(function(){
      toast('Copied ID '+id+' — paste as "icon":"'+id+'" in your call');
    });
  };
  btns.appendChild(bGet);btns.appendChild(bUse);
  d.appendChild(img);d.appendChild(lbl);d.appendChild(btns);
  return d;
}

function renderGrid(ids){
  var g=document.getElementById("ic_grid");g.innerHTML="";
  ids.forEach(function(id){g.appendChild(makeCard(id));});
}

function loadPage(p){
  curPage=p;
  document.getElementById("pg_n").value=p;
  if(p<0){renderGrid(POPULAR);return;}
  var start=p*50+1,ids=[];
  for(var i=start;i<start+50;i++)ids.push(i);
  renderGrid(ids);
}

function pg(d){
  var n=(curPage<0?0:curPage)+d;
  if(n<-1)n=-1;
  loadPage(n<0?-1:n);
  document.getElementById("pg_n").value=n<0?0:n;
}

function searchId(){
  var v=parseInt(document.getElementById("ic_id").value);
  if(!v){toast("Enter a valid ID",false);return;}
  renderGrid([v]);
}

// ── install ───────────────────────────────────────────────────────────────
function installIcon(id,btn){
  var saveName=prompt("Save icon #"+id+" as (no extension):",String(id));
  if(saveName===null)return; // cancelled
  saveName=saveName.trim().replace(/[^a-zA-Z0-9_\-]/g,"_")||String(id);
  var orig=btn?btn.textContent:"";
  if(btn){btn.textContent="...";btn.disabled=true;}
  var url="https://developer.lametric.com/content/apps/icon_thumbs/"+id;
  fetch(url).then(function(r){
    var ct=r.headers.get("content-type")||"";
    return r.blob().then(function(blob){return {blob:blob,ct:ct};});
  }).then(function(res){
    var blob=res.blob,ct=res.ct;
    var finish=function(b,ext){
      var fd=new FormData();
      fd.append("image",b,"ICONS/"+saveName+ext);
      fetch("/edit",{method:"POST",body:fd,mode:"no-cors"})
        .then(function(){
          toast("Saved as \""+saveName+"\" \u2192 use icon:\""+saveName+"\"");
          installed[id]=ext;
          if(btn){btn.textContent="\u2713";btn.style.background="#285";}
          btn.title="Saved as: "+saveName;
        }).catch(function(e){toast("Save error: "+e,false);if(btn){btn.textContent=orig;btn.disabled=false;}});
    };
    if(ct.indexOf("gif")>=0){
      finish(blob,".gif");
    } else {
      var ou=URL.createObjectURL(blob);
      var im=new Image();
      im.onload=function(){
        var cv=document.createElement("canvas");
        cv.width=im.width;cv.height=im.height;
        cv.getContext("2d").drawImage(im,0,0);
        cv.toBlob(function(b){finish(b,".jpg");},"image/jpeg",1);
        URL.revokeObjectURL(ou);
      };
      im.onerror=function(){finish(blob,".png");};
      im.src=ou;
    }
  }).catch(function(e){
    toast("Download failed: "+e,false);
    if(btn){btn.textContent=orig;btn.disabled=false;}
  });
}

// ── gifs ──────────────────────────────────────────────────────────────────
var gifsLoaded=false;
function loadGifs(){
  if(gifsLoaded)return;gifsLoaded=true;
  var el=document.getElementById("gif_list");
  fetch("https://api.github.com/repos/Blueforcer/awtrix3/contents/Bigtime_GIFs")
    .then(function(r){return r.json();})
    .then(function(files){
      el.innerHTML="";
      var gifs=files.filter(function(f){return /\.gif$/i.test(f.name);});
      if(!gifs.length){el.textContent="None found.";return;}
      gifs.forEach(function(f){
        var card=document.createElement("div");card.className="gifcard";
        var img=document.createElement("img");
        img.src=f.download_url;
        img.style.cssText="width:96px;height:24px;image-rendering:pixelated;background:#000;flex-shrink:0";
        img.onerror=function(){img.style.display="none";};
        var info=document.createElement("div");info.style.flex="1";
        info.innerHTML='<div style="color:#8cf">'+f.name+'</div>'
          +'<div style="color:#666;font-size:10px">'+Math.round(f.size/1024)+' KB &mdash; install as <code style="color:#aaa">'+f.name+'</code> in root</div>';
        var btn=document.createElement("button");btn.textContent="&#8659; Install";
        btn.onclick=function(){
          btn.textContent="...";btn.disabled=true;
          fetch(f.download_url).then(function(r){return r.blob();}).then(function(b){
            var fd=new FormData();fd.append("image",b,f.name);
            fetch("/edit",{method:"POST",body:fd,mode:"no-cors"})
              .then(function(){toast("Installed "+f.name);btn.textContent="✓";btn.style.background="#285";})
              .catch(function(e){toast("Save error: "+e,false);btn.textContent="&#8659; Install";btn.disabled=false;});
          }).catch(function(e){toast("Fetch error: "+e,false);btn.textContent="&#8659; Install";btn.disabled=false;});
        };
        card.appendChild(img);card.appendChild(info);card.appendChild(btn);
        el.appendChild(card);
      });
    }).catch(function(e){el.textContent="GitHub error: "+e;});
}

// ── files ─────────────────────────────────────────────────────────────────
function renderDir(data,elId,prefix){
  var el=document.getElementById(elId);
  if(!data||!data.length){el.innerHTML='<span style="color:#555">Empty</span>';return;}
  el.innerHTML="";
  data.forEach(function(f){
    if(f.type!=="file")return;
    var row=document.createElement("div");row.className="fitem";
    var base=f.name.replace(/\.[^.]+$/,"");
    var ext=f.name.indexOf(".")>=0?f.name.slice(f.name.lastIndexOf(".")):"";
    var name=document.createElement("span");
    name.innerHTML='<b style="color:#8cf">'+f.name+'</b>'
      +' <span style="color:#555">'+(Math.round(f.size/102.4)/10)+' KB</span>'
      +(prefix==="/ICONS/"?'<span style="color:#444;margin-left:6px">\u2192 icon:"'+base+'"</span>':'');
    var btns=document.createElement("div");btns.style.cssText="display:flex;gap:4px";
    var ren=document.createElement("button");ren.textContent="Rename";ren.className="sm";ren.style.marginTop="0";
    ren.onclick=(function(fname,fbase,fext){return function(){
      var nb=prompt("Rename \""+fname+"\" to (no extension):",fbase);
      if(!nb||nb===fbase)return;
      nb=nb.trim().replace(/[^a-zA-Z0-9_\-]/g,"_");if(!nb)return;
      fetch("/api/rename",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},
        body:"plain="+encodeURIComponent(JSON.stringify({from:prefix+fname,to:prefix+nb+fext}))})
      .then(function(r){
        if(r.ok){toast("Renamed \u2192 "+nb+fext);
          name.innerHTML='<b style="color:#8cf">'+nb+fext+'</b> <span style="color:#555">'+(Math.round(f.size/102.4)/10)+' KB</span>'+(prefix==="/ICONS/"?'<span style="color:#444;margin-left:6px">\u2192 icon:"'+nb+'"</span>':'');
          f.name=nb+fext;}
        else r.text().then(function(t){toast("Failed: "+t,false);});
      });
    };})(f.name,base,ext);
    var del=document.createElement("button");del.textContent="Delete";del.className="red sm";del.style.marginTop="0";
    del.onclick=(function(fname){return function(){
      if(!confirm("Delete "+prefix+fname+"?"))return;
      fetch("/edit?path="+encodeURIComponent(prefix+fname),{method:"DELETE"})
        .then(function(){toast("Deleted "+fname);row.remove();})
        .catch(function(){toast("Delete failed",false);});
    };})(f.name);
    btns.appendChild(ren);btns.appendChild(del);
    row.appendChild(name);row.appendChild(btns);el.appendChild(row);
  });
}

function loadFiles(){
  fetch("/list?dir=/ICONS/").then(function(r){return r.json();}).then(function(d){renderDir(d,"f_icons","/ICONS/");}).catch(function(){document.getElementById("f_icons").textContent="Could not list.";});
  fetch("/list?dir=/MELODIES/").then(function(r){return r.json();}).then(function(d){renderDir(d,"f_mel","/MELODIES/");}).catch(function(){document.getElementById("f_mel").textContent="Could not list.";});
  fetch("/list?dir=/").then(function(r){return r.json();}).then(function(d){renderDir(d,"f_root","/");}).catch(function(){document.getElementById("f_root").textContent="Could not list.";});
}

// init
loadPage(-1);
loadFiles();
</script></body></html>
)BROWSE";

// Small snippet injected into /setup to link to the control panel
static const char ctrl_nav_html[] PROGMEM = R"EOF(
  <div style="margin:8px 0">
    <a href="/ctrl" target="_blank" style="color:#4af;font-family:monospace;font-size:13px">&#9654; Open Control Panel</a>
    &nbsp;&nbsp;
    <a href="/screen" target="_blank" style="color:#4af;font-family:monospace;font-size:13px">&#9654; Live View</a>
  </div>
)EOF";

static const char ctrl_html[] PROGMEM = R"EOF(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>dpx_tc002</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#111;color:#ddd;font-family:monospace;font-size:13px;padding:14px}
h1{color:#4af;margin-bottom:12px;font-size:18px}
h2{color:#8cf;margin-bottom:8px;font-size:13px;text-transform:uppercase;letter-spacing:1px}
h3{color:#8cf;font-size:11px;margin-bottom:6px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(320px,1fr));gap:10px}
.card{background:#1a1a2e;border:1px solid #2a2a4a;border-radius:8px;padding:12px}
label{display:block;margin:5px 0 2px;color:#999;font-size:11px}
input[type=text],input[type=number],select{width:100%;background:#222;color:#eee;border:1px solid #444;border-radius:4px;padding:4px 7px;font-family:monospace;font-size:12px}
input[type=color]{width:40px;height:26px;border:none;background:none;cursor:pointer;padding:0}
input[type=range]{flex:1;accent-color:#4af}
.row{display:flex;align-items:center;gap:6px;flex-wrap:wrap;margin-top:4px}
.chk{display:flex;align-items:center;gap:4px;white-space:nowrap}
.chk input{width:14px;height:14px}
.chk label{margin:0;color:#bbb;font-size:11px}
button{background:#2a5298;color:#fff;border:none;border-radius:4px;padding:6px 13px;cursor:pointer;font-family:monospace;font-size:12px;margin-top:8px}
button:hover{background:#3a63b8}
button.red{background:#822}
button.red:hover{background:#a33}
button.sm{padding:4px 9px;margin-top:0}
.ibox{flex:1;background:#111;border:1px solid #2a2a4a;border-radius:6px;padding:8px;text-align:center}
.ibox b{color:#8cf;font-size:12px}
hr{border:none;border-top:1px solid #2a2a4a;margin:8px 0}
.toast{position:fixed;bottom:16px;right:16px;background:#2a5;color:#fff;padding:8px 16px;border-radius:6px;display:none;z-index:999;font-size:12px}
nav a{color:#4af;text-decoration:none;margin-right:12px;font-size:12px}
nav{margin-bottom:12px}
select option{background:#222}
</style></head><body>
<h1>&#9632; dpx_tc002</h1>
<nav><a href="/setup">&#9881; Settings</a><a href="/browse" target="_blank">&#9782; Browse Icons</a><a href="/screen" target="_blank">&#9654; Live View</a><a href="/api-ref" target="_blank">&#128196; API Ref</a></nav>
<div id="status_bar" style="background:#0a0a1a;border:1px solid #2a2a4a;border-radius:4px;padding:5px 10px;margin-bottom:10px;font-size:11px;color:#666;display:flex;gap:14px;flex-wrap:wrap;align-items:center">Loading...</div>
<div class="grid">

<div class="card">
<h2>Notification</h2>
<label>Text</label><input type="text" id="n_text" value="Hello!">
<div class="row">
  <div><label>Color</label><input type="color" id="n_color" value="#ffffff"></div>
  <div><label>Background</label><input type="color" id="n_bg" value="#000000"></div>
  <div class="chk" style="margin-top:14px"><input type="checkbox" id="n_rainbow"><label for="n_rainbow">Rainbow</label></div>
</div>
<label>Icon</label>
<div class="row">
  <select id="n_icon_sel" style="flex:1" onchange="document.getElementById('n_icon').value=this.value"><option value="">&#8212; installed icons &#8212;</option></select>
  <input type="text" id="n_icon" placeholder="name or ID" style="flex:1">
  <select id="n_pushicon" style="width:auto"><option value="0">Fixed</option><option value="1">Scroll</option><option value="2">Loop</option></select>
</div>
<div class="row">
  <div style="flex:1"><label>Effect</label><select id="n_effect"><option value="">None</option></select></div>
  <div style="flex:1"><label>Overlay</label><select id="n_overlay"><option value="">None</option><option value="drizzle">Drizzle</option><option value="rain">Rain</option><option value="snow">Snow</option><option value="storm">Storm</option><option value="thunder">Thunder</option><option value="frost">Frost</option></select></div>
</div>
<div class="row">
  <div style="flex:1"><label>Speed (0-100)</label><input type="number" id="n_speed" value="100" min="0" max="100"></div>
  <div style="flex:1"><label>Duration (s)</label><input type="number" id="n_dur" value="5" min="1"></div>
  <div style="flex:1"><label>Repeat (-1=&#8734;)</label><input type="number" id="n_rep" value="1" min="-1"></div>
</div>
<div class="row" style="margin-top:6px">
  <div class="chk"><input type="checkbox" id="n_center"><label for="n_center">Center</label></div>
  <div class="chk"><input type="checkbox" id="n_noscroll"><label for="n_noscroll">No Scroll</label></div>
  <div class="chk"><input type="checkbox" id="n_bounce"><label for="n_bounce">Bounce</label></div>
  <div class="chk"><input type="checkbox" id="n_toptext" checked><label for="n_toptext">Top Text</label></div>
</div>
<div class="row">
  <div style="flex:1"><label>Fade ms</label><input type="number" id="n_fade" value="0" min="0"></div>
  <div style="flex:1"><label>Blink ms</label><input type="number" id="n_blink" value="0" min="0"></div>
  <div style="flex:1"><label>Text Case</label><select id="n_tcase"><option value="0">Global</option><option value="1">Upper</option><option value="2">Lower</option></select></div>
</div>
<label>Progress (-1=off)</label>
<div class="row">
  <input type="number" id="n_prog" value="-1" min="-1" max="100" style="flex:1">
  <div><label style="margin:0;font-size:10px">Bar</label><input type="color" id="n_pc" value="#00ff00"></div>
  <div><label style="margin:0;font-size:10px">BG</label><input type="color" id="n_pbc" value="#333333"></div>
</div>
<div class="row">
  <button onclick="sendNotify()">&#9993; Send</button>
  <button class="red sm" style="margin-top:8px;margin-left:4px" onclick="apiPost('/api/notify/dismiss',{})">&#10006; Dismiss current</button>
</div>
<div class="row" style="margin-top:4px">
  <button class="red" style="width:100%;background:#3a1a1a" onclick="apiPost('/api/notify/clear',{}).then(loadStatus);toast('Notification queue cleared')">&#10006;&#10006; Clear all queued <span id="n_queue_count" style="font-size:11px;opacity:0.7"></span></button>
</div>
</div>

<div class="card">
<h2>Configure Channel</h2>
<div class="row" style="margin-bottom:8px">
  <select id="ch_type" style="flex:0.6" onchange="updateChType(this.value)">
    <option value="text">Text</option>
    <option value="time">Time</option>
    <option value="date">Date</option>
    <option value="timecode">Timecode</option>
    <option value="osc">OSC</option>
  </select>
</div>
<div id="ch_fields_native" style="display:none">
  <p id="ch_native_hint" style="color:#8cf;font-size:12px;margin:8px 0 12px">Select a type above.</p>
  <button onclick="addChannel()">+ Add to Rotation</button>
</div>
<div id="ch_fields_text">
<div class="row">
  <div style="flex:1"><label>Channel name</label>
    <div class="row" style="margin-top:0">
      <select id="ca_name_sel" style="flex:1" onchange="if(this.value)document.getElementById('ca_name').value=this.value"><option value="">&#8212; existing &#8212;</option></select>
      <input type="text" id="ca_name" value="myapp" placeholder="or new name" style="flex:1">
    </div>
  </div>
</div>
<label>Text</label><input type="text" id="ca_text" placeholder="App text content">
<div class="row">
  <div><label>Color</label><input type="color" id="ca_color" value="#ffffff"></div>
  <div><label>Background</label><input type="color" id="ca_bg" value="#000000"></div>
  <div class="chk" style="margin-top:14px"><input type="checkbox" id="ca_rainbow"><label for="ca_rainbow">Rainbow</label></div>
</div>
<label>Icon</label>
<div class="row">
  <select id="ca_icon_sel" style="flex:1" onchange="document.getElementById('ca_icon').value=this.value"><option value="">&#8212; installed icons &#8212;</option></select>
  <input type="text" id="ca_icon" placeholder="name or ID" style="flex:1">
  <select id="ca_pushicon" style="width:auto"><option value="0">Fixed</option><option value="1">Scroll</option><option value="2">Loop</option></select>
</div>
<div class="row">
  <div style="flex:1"><label>Effect</label><select id="ca_effect"><option value="">None</option></select></div>
  <div style="flex:1"><label>Overlay</label><select id="ca_overlay"><option value="">None</option><option value="drizzle">Drizzle</option><option value="rain">Rain</option><option value="snow">Snow</option><option value="storm">Storm</option><option value="thunder">Thunder</option><option value="frost">Frost</option></select></div>
</div>
<div class="row">
  <div style="flex:1"><label>Speed</label><input type="number" id="ca_speed" value="100" min="0" max="100"></div>
  <div style="flex:1"><label>Duration (s, 0=perm)</label><input type="number" id="ca_dur" value="0" min="0"></div>
</div>
<div class="row" style="margin-top:6px">
  <div class="chk"><input type="checkbox" id="ca_center"><label for="ca_center">Center</label></div>
  <div class="chk"><input type="checkbox" id="ca_noscroll"><label for="ca_noscroll">No Scroll</label></div>
  <div class="chk"><input type="checkbox" id="ca_bounce"><label for="ca_bounce">Bounce</label></div>
</div>
<div class="row">
  <div style="flex:1"><label>Fade ms</label><input type="number" id="ca_fade" value="0" min="0"></div>
  <div style="flex:1"><label>Blink ms</label><input type="number" id="ca_blink" value="0" min="0"></div>
</div>
<label>Progress (-1=off)</label>
<div class="row">
  <input type="number" id="ca_prog" value="-1" min="-1" max="100" style="flex:1">
  <div><label style="margin:0;font-size:10px">Bar</label><input type="color" id="ca_pc" value="#00ff00"></div>
  <div><label style="margin:0;font-size:10px">BG</label><input type="color" id="ca_pbc" value="#333333"></div>
</div>
</div><!-- end ch_fields_text -->
<div class="row">
  <button onclick="sendCustomApp()">&#9654; Push</button>
  <button class="sm" style="margin-top:8px;margin-left:4px;background:#285" onclick="loadCustomApp()">&#8595; Load</button>
  <button class="red sm" style="margin-top:8px;margin-left:4px" onclick="deleteCustomApp()">Remove</button>
</div>
</div>

<div class="card">
<h2>D3 / OSC Channels</h2>
<p style="color:#666;font-size:11px;margin-bottom:8px">Map OSC paths to display channels. Each path auto-pushes its value as text to the named channel.</p>
<div class="row" style="flex-wrap:wrap;gap:6px;margin-bottom:8px">
  <div style="flex:2;min-width:160px">
    <label>OSC Path</label>
    <select id="osc_path_txt" style="width:100%" onchange="document.getElementById('osc_label_txt').value=this.options[this.selectedIndex].getAttribute('data-lbl')||'';document.getElementById('osc_ch_txt').value=this.options[this.selectedIndex].getAttribute('data-ch')||'';document.getElementById('osc_path_manual').value=''">
      <option value="" data-lbl="" data-ch="">&#8212; quick-fill presets &#8212;</option>
      <option value="/d3/showcontrol/timecodeposition"   data-lbl="Timecode"        data-ch="tc"     >/d3/showcontrol/timecodeposition</option>
      <option value="/d3/showcontrol/trackposition"      data-lbl="Track Position"  data-ch="d3_pos" >/d3/showcontrol/trackposition</option>
      <option value="/d3/showcontrol/trackname"          data-lbl="Track Name"      data-ch="d3_name">/d3/showcontrol/trackname</option>
      <option value="/d3/showcontrol/trackid"            data-lbl="Track ID"        data-ch="d3_id"  >/d3/showcontrol/trackid</option>
      <option value="/d3/showcontrol/playmode"           data-lbl="Play Mode"       data-ch="d3_mode">/d3/showcontrol/playmode</option>
      <option value="/d3/showcontrol/currentsectionname" data-lbl="Current Section" data-ch="d3_sec" >/d3/showcontrol/currentsectionname</option>
      <option value="/d3/showcontrol/nextsectionname"    data-lbl="Next Section"    data-ch="d3_nsec">/d3/showcontrol/nextsectionname</option>
      <option value="/d3/showcontrol/sectionhint"        data-lbl="Section Hint"    data-ch="d3_hint">/d3/showcontrol/sectionhint</option>
      <option value="/d3/showcontrol/volume"             data-lbl="Volume"          data-ch="d3_vol" >/d3/showcontrol/volume</option>
      <option value="/d3/showcontrol/brightness"         data-lbl="Brightness"      data-ch="d3_bri" >/d3/showcontrol/brightness</option>
      <option value="/d3/showcontrol/bpm"                data-lbl="BPM"             data-ch="d3_bpm" >/d3/showcontrol/bpm</option>
      <option value="/d3/showcontrol/heartbeat"          data-lbl="Heartbeat"       data-ch="d3_hb"  >/d3/showcontrol/heartbeat</option>
    </select>
    <input type="text" id="osc_path_manual" placeholder="/custom/path" style="margin-top:4px">
  </div>
  <div style="flex:1;min-width:100px">
    <label>Label</label>
    <input type="text" id="osc_label_txt" placeholder="My Channel">
  </div>
  <div style="flex:1;min-width:100px">
    <label>Channel</label>
    <input type="text" id="osc_ch_txt" placeholder="my_ch">
  </div>
</div>
<div class="row">
  <button class="sm" style="margin-top:0" onclick="addListener()">+ Add Listener</button>
</div>
<div id="listener_list" style="margin-top:8px;color:#555">Loading...</div>
</div>

<div class="card">
<h2>App Channels <button class="sm" style="margin-top:0;margin-left:6px" onclick="loadLoop()">&#8635;</button></h2>
<div id="mqtt_prefix_note" style="background:#111;border:1px solid #2a2a4a;border-radius:4px;padding:6px 10px;margin-bottom:8px;font-size:11px;color:#888">MQTT prefix loading...</div>
<div id="loop_list" style="color:#555">Loading...</div>
<hr>
<h3>Go to Channel</h3>
<div class="row"><select id="sw_app_sel" style="flex:1" onchange="document.getElementById('sw_app').value=this.value"><option value="">&#8212; from loop &#8212;</option></select><input type="text" id="sw_app" placeholder="or type name" style="flex:1"><button class="sm" style="margin-top:0;margin-left:4px" onclick="switchApp()">Go</button></div>
<div class="row" style="margin-top:6px">
  <button onclick="apiPost('/api/previousapp',{})">&#9664; Prev</button>
  <button style="margin-left:4px" onclick="apiPost('/api/nextapp',{})">Next &#9654;</button>
</div>
</div>

<div class="card">
<h2>Display &amp; Indicators</h2>
<label>Brightness</label>
<div class="row"><input type="range" id="bri" min="0" max="255" value="128"><span id="bri_v" style="min-width:28px;text-align:right">128</span></div>
<button onclick="sendBri()" style="margin-top:6px">Set Brightness</button>
<hr>
<div class="row" style="margin-top:4px">
  <button onclick="apiPost('/api/power',{power:true})">&#9675; Power On</button>
  <button class="red sm" style="margin-top:8px;margin-left:4px" onclick="apiPost('/api/power',{power:false})">Power Off</button>
</div>
<hr>
<div class="row" style="align-items:stretch">
<div class="ibox">
  <b>1</b>
  <div><label>Color</label><input type="color" id="i1c" value="#ff0000"></div>
  <div><label>Blink ms</label><input type="number" id="i1b" value="0" min="0"></div>
  <div><label>Fade ms</label><input type="number" id="i1f" value="0" min="0"></div>
  <button style="width:100%;margin-top:6px" onclick="sendInd(1)">Set</button>
  <button class="red" style="width:100%;margin-top:3px" onclick="apiPost('/api/indicator1',{color:[0,0,0]})">Off</button>
</div>
<div class="ibox">
  <b>2</b>
  <div><label>Color</label><input type="color" id="i2c" value="#00ff00"></div>
  <div><label>Blink ms</label><input type="number" id="i2b" value="0" min="0"></div>
  <div><label>Fade ms</label><input type="number" id="i2f" value="0" min="0"></div>
  <button style="width:100%;margin-top:6px" onclick="sendInd(2)">Set</button>
  <button class="red" style="width:100%;margin-top:3px" onclick="apiPost('/api/indicator2',{color:[0,0,0]})">Off</button>
</div>
<div class="ibox">
  <b>3</b>
  <div><label>Color</label><input type="color" id="i3c" value="#0000ff"></div>
  <div><label>Blink ms</label><input type="number" id="i3b" value="0" min="0"></div>
  <div><label>Fade ms</label><input type="number" id="i3f" value="0" min="0"></div>
  <button style="width:100%;margin-top:6px" onclick="sendInd(3)">Set</button>
  <button class="red" style="width:100%;margin-top:3px" onclick="apiPost('/api/indicator3',{color:[0,0,0]})">Off</button>
</div>
</div>
</div>

<div class="card">
<h2>TC Settings</h2>
<p style="color:#666;font-size:11px;margin-bottom:10px">Controls how the timecode display behaves when LTC frames arrive via OSC.</p>
<div class="row" style="margin-bottom:8px;align-items:center">
  <div class="chk"><input type="checkbox" id="tc_hold"><label for="tc_hold">Hold display forever after TC stops (never auto-dismiss)</label></div>
</div>
<div class="row" style="margin-bottom:10px;align-items:center">
  <label style="min-width:140px">Dwell after stop <span style="color:#8cf;font-size:10px" id="tc_dwell_v">2 s</span></label>
  <input type="range" id="tc_dwell" min="0" max="30" step="1" value="2" style="flex:1;accent-color:#4af" oninput="document.getElementById('tc_dwell_v').textContent=this.value+' s'">
</div>
<div class="row" style="margin-bottom:10px;align-items:center">
  <div class="chk"><input type="checkbox" id="tc_show_frames"><label for="tc_show_frames">Show frames: display <code>MM:SS.FF</code> instead of <code>HH:MM:SS</code></label></div>
</div>
<div class="row" style="margin-bottom:10px;align-items:center">
  <div class="chk"><input type="checkbox" id="tc_stop_beep"><label for="tc_stop_beep">2&times; beep when TC stops</label></div>
</div>
<div class="row" style="margin-bottom:10px;align-items:center">
  <div class="chk"><input type="checkbox" id="tc_mute"><label for="tc_mute">Mute TC &mdash; ignore timecode signal entirely</label></div>
</div>
<button onclick="saveTCSettings()">Save TC Settings</button>
</div>

<div class="card">
<h2>Sound <span id="snd_status" style="font-size:10px;color:#666"></span></h2>
<div class="row" style="margin-bottom:6px">
  <div class="chk"><input type="checkbox" id="snd_en"><label for="snd_en">Sound enabled</label></div>
  <div style="flex:1;margin-left:10px;color:#555;font-size:11px">Volume: n/a &#8212; passive piezo</div>
  <button class="sm" style="margin-top:0" onclick="saveSndSettings()">Save</button>
  <button class="sm" style="margin-top:0;margin-left:4px;background:#285" onclick="fetch('/api/beeptest',{method:'POST'}).then(function(r){return r.json();}).then(function(d){toast('pin:'+d.pin+' ch:'+d.ledc_ch+' snd:'+d.sound_enabled);}).catch(function(e){toast('Error: '+e,false);})">&#9834; Test</button>
</div>
<hr>
<label>RTTTL &#8212; inline melody (format: <code style="color:#8cf">Name:d=4,o=5,b=120:c,e,g</code>)</label>
<div class="row" style="margin-bottom:4px">
  <select id="rtttl_preset" style="flex:1" onchange="document.getElementById('rtttl').value=this.value">
    <option value="">&#8212; presets &#8212;</option>
    <option value="Beep:d=4,o=5,b=200:c,e,g,c6">Simple beep up</option>
    <option value="Alert:d=8,o=5,b=180:c6,p,c6,p,c6">Alert triple</option>
    <option value="Mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g5,8p">Mario</option>
    <option value="StarWars:d=4,o=5,b=45:32p,32f,32f,32f,8a#.,8f,16a#,16g#,16g,16f,8a#.,8f,16a#,16g#,16g,16f">Star Wars</option>
    <option value="Axel-F:d=4,o=5,b=160:32p,16f#,32p,16f#,32p,f#,16p,16d#,16p,f#,32p,16a#,32p,16f#,32p,16a,16f#">Axel-F</option>
    <option value="Countdown:d=4,o=5,b=80:2e6,2d6,2c6">Countdown</option>
  </select>
</div>
<input type="text" id="rtttl" placeholder="Name:d=4,o=5,b=120:c,e,g,c6" style="margin-bottom:4px">
<div class="row">
  <button onclick="sendRtttl()">&#9834; Play RTTTL</button>
  <button class="red sm" style="margin-top:8px;margin-left:4px" onclick="apiPost('/api/sound',{})">&#9646;&#9646; Stop</button>
</div>
<hr>
<label>Sound File &#8212; plays <code style="color:#8cf">/MELODIES/name.txt</code> (upload via Browse &rarr; Files)</label>
<div class="row">
  <select id="snd_file_sel" style="flex:1" onchange="document.getElementById('snd_file').value=this.value"><option value="">&#8212; installed melodies &#8212;</option></select>
  <input type="text" id="snd_file" placeholder="filename (no ext)" style="flex:1">
  <button class="sm" style="margin-top:0" onclick="sendSound()">&#9654;</button>
</div>
</div>

</div>
<div class="toast" id="toast"></div>
<script>
function h2r(h){return[parseInt(h.slice(1,3),16),parseInt(h.slice(3,5),16),parseInt(h.slice(5,7),16)];}
function toast(m,ok){var t=document.getElementById("toast");t.textContent=m;t.style.background=ok===false?"#822":"#2a5";t.style.display="block";setTimeout(function(){t.style.display="none";},2500);}
function apiPost(url,data){return fetch(url,{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"plain="+encodeURIComponent(JSON.stringify(data))}).then(function(r){r.ok?toast(url.split("/").pop()+" OK"):toast("Error "+r.status,false);}).catch(function(e){toast(String(e),false);});}
function strip(d){Object.keys(d).forEach(function(k){if(d[k]===undefined||d[k]==="")delete d[k];});return d;}
fetch("/api/effects").then(function(r){return r.json();}).then(function(fx){["n_effect","ca_effect"].forEach(function(id){var s=document.getElementById(id);fx.forEach(function(e){var o=document.createElement("option");o.value=e;o.textContent=e;s.appendChild(o);});});}).catch(function(){});
function loadIcons(){fetch("/list?dir=/ICONS/").then(function(r){return r.json();}).then(function(files){var names=files.filter(function(f){return f.type==="file";}).map(function(f){return f.name.replace(/\.[^.]+$/,"");});["n_icon_sel","ca_icon_sel"].forEach(function(id){var s=document.getElementById(id);s.innerHTML="<option value=''>&#8212; installed icons &#8212;</option>";names.forEach(function(n){var o=document.createElement("option");o.value=n;o.textContent=n;s.appendChild(o);});});}).catch(function(){});}
loadIcons();
var mqttPrefix="[prefix]";
function loadLoop(){
  var el=document.getElementById("loop_list");
  fetch("/api/apps").then(function(r){return r.json();}).then(function(apps){
    el.innerHTML="";
    var names=apps.map(function(a){return a.name;});
    var swSel=document.getElementById("sw_app_sel");
    var caSel=document.getElementById("ca_name_sel");
    if(swSel){swSel.innerHTML="<option value=''>&#8212; from loop &#8212;</option>";names.forEach(function(n){var o=document.createElement("option");o.value=n;o.textContent=n;swSel.appendChild(o);});}
    if(caSel){caSel.innerHTML="<option value=''>&#8212; existing &#8212;</option>";apps.filter(function(a){return !a.native;}).forEach(function(a){var o=document.createElement("option");o.value=a.name;o.textContent=a.name;caSel.appendChild(o);});}
    apps.forEach(function(app){
      var row=document.createElement("div");
      row.style.cssText="background:#111120;border:1px solid #2a2a4a;border-radius:4px;padding:6px 10px;margin-bottom:4px"+(app.muted?";opacity:0.5":"");
      var hdr=document.createElement("div");hdr.style.cssText="display:flex;align-items:center;justify-content:space-between";
      var nameEl=document.createElement("span");nameEl.style.cssText="color:#8cf;font-weight:bold";
      nameEl.textContent=app.name+(app.native?" \u2605":"")+(app.muted?" [muted]":"");
      var btns=document.createElement("div");btns.style.cssText="display:flex;gap:4px";
      var bGo=document.createElement("button");bGo.textContent="Go";bGo.className="sm";bGo.style.marginTop="0";
      bGo.onclick=(function(n){return function(){apiPost("/api/switch",{name:n});};})(app.name);
      btns.appendChild(bGo);
      var bMute=document.createElement("button");bMute.textContent=app.muted?"Unmute":"Mute";bMute.className="sm";bMute.style.cssText="margin-top:0;background:#554422";
      bMute.onclick=(function(a){return function(){apiPost("/api/mute",{name:a.name,mute:!a.muted}).then(function(){setTimeout(loadLoop,300);});};})(app);
      btns.appendChild(bMute);
      if(!app.native){
        var bDel=document.createElement("button");bDel.textContent="Remove";bDel.className="red sm";bDel.style.marginTop="0";
        bDel.onclick=(function(n){return function(){fetch("/api/custom?name="+encodeURIComponent(n),{method:"POST"}).then(function(){toast("Removed "+n);setTimeout(loadLoop,600);});};})(app.name);
        btns.appendChild(bDel);
      } else {
        var bDel=document.createElement("button");bDel.textContent="Remove";bDel.className="red sm";bDel.style.marginTop="0";
        var key=app.name==="Time"?"TIM":"DAT";
        bDel.onclick=(function(k,n){return function(){apiPost("/api/settings",function(){var d={};d[k]=false;return d;}()).then(function(){toast(n+" removed");setTimeout(loadLoop,400);});};})(key,app.name);
        btns.appendChild(bDel);
      }
      hdr.appendChild(nameEl);hdr.appendChild(btns);row.appendChild(hdr);
      if(!app.native){
        var info=document.createElement("div");info.style.cssText="font-size:10px;color:#555;margin-top:3px;line-height:1.7";
        info.innerHTML='OSC <code style="color:#4af;font-size:10px">/dpx_tc002/custom/'+app.name+'</code><br>MQTT <code style="color:#4af;font-size:10px">'+mqttPrefix+'/custom/'+app.name+'</code>';
        row.appendChild(info);}
      el.appendChild(row);
    });
    if(!apps.length)el.textContent="No apps.";
    // Show "Add back" buttons for hidden native channels
    var names=apps.map(function(a){return a.name;});
    var addArea=document.getElementById("native_add_area");
    if(addArea){
      addArea.innerHTML="";
      [["Time","TIM"],["Date","DAT"]].forEach(function(pair){
        if(names.indexOf(pair[0])<0){
          var b=document.createElement("button");b.className="sm";b.style.marginTop="0";
          b.textContent="+ "+pair[0];
          b.onclick=(function(k,n){return function(){apiPost("/api/settings",(function(){var d={};d[k]=true;return d;})()).then(function(){setTimeout(loadLoop,400);});};})(pair[1],pair[0]);
          addArea.appendChild(b);
        }
      });
    }
  }).catch(function(){el.textContent="Could not load.";});
}
  var text=document.getElementById("new_ch_text").value||name;
  fetch("/api/custom?name="+encodeURIComponent(name),{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"plain="+encodeURIComponent(JSON.stringify({text:text,scrollSpeed:80}))}).then(function(r){r.ok?toast("Channel '"+name+"' created"):toast("Error",false);setTimeout(loadLoop,400);});
  document.getElementById("new_ch_name").value="";document.getElementById("new_ch_text").value="";
}
loadLoop();
document.getElementById("bri").addEventListener("input",function(){document.getElementById("bri_v").textContent=this.value;});
function sendNotify(){
  var d={text:document.getElementById("n_text").value,color:h2r(document.getElementById("n_color").value),background:h2r(document.getElementById("n_bg").value),rainbow:document.getElementById("n_rainbow").checked,icon:document.getElementById("n_icon").value||undefined,pushIcon:+document.getElementById("n_pushicon").value,scrollSpeed:+document.getElementById("n_speed").value,noScrolling:document.getElementById("n_noscroll").checked,center:document.getElementById("n_center").checked,bounce:document.getElementById("n_bounce").checked,topText:document.getElementById("n_toptext").checked,fade:+document.getElementById("n_fade").value,blink:+document.getElementById("n_blink").value,textCase:+document.getElementById("n_tcase").value,duration:+document.getElementById("n_dur").value,repeat:+document.getElementById("n_rep").value};
  var e=document.getElementById("n_effect").value;if(e)d.effect=e;
  d.overlay=document.getElementById("n_overlay").value;
  var p=+document.getElementById("n_prog").value;if(p>=0){d.progress=p;d.progressC=h2r(document.getElementById("n_pc").value);d.progressBC=h2r(document.getElementById("n_pbc").value);}
  apiPost("/api/notify",strip(d));
}
function sendCustomApp(){
  var name=document.getElementById("ca_name").value;if(!name){toast("Name required",false);return;}
  var d={text:document.getElementById("ca_text").value,color:h2r(document.getElementById("ca_color").value),background:h2r(document.getElementById("ca_bg").value),rainbow:document.getElementById("ca_rainbow").checked,icon:document.getElementById("ca_icon").value||undefined,pushIcon:+document.getElementById("ca_pushicon").value,scrollSpeed:+document.getElementById("ca_speed").value,noScrolling:document.getElementById("ca_noscroll").checked,center:document.getElementById("ca_center").checked,bounce:document.getElementById("ca_bounce").checked,fade:+document.getElementById("ca_fade").value,blink:+document.getElementById("ca_blink").value};
  var e=document.getElementById("ca_effect").value;if(e)d.effect=e;
  d.overlay=document.getElementById("ca_overlay").value;
  var dur=+document.getElementById("ca_dur").value;if(dur>0)d.duration=dur;
  var p=+document.getElementById("ca_prog").value;if(p>=0){d.progress=p;d.progressC=h2r(document.getElementById("ca_pc").value);d.progressBC=h2r(document.getElementById("ca_pbc").value);}
  fetch("/api/custom?name="+encodeURIComponent(name),{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"plain="+encodeURIComponent(JSON.stringify(strip(d)))}).then(function(r){r.ok?toast("App pushed"):toast("Error "+r.status,false);setTimeout(loadLoop,500);}).catch(function(e){toast(String(e),false);});
}
function loadCustomApp(){
  var name=document.getElementById("ca_name").value;if(!name){toast("Pick an app name first",false);return;}
  fetch("/api/custom?name="+encodeURIComponent(name)).then(function(r){return r.json();}).then(function(d){
    if(d.text!==undefined)document.getElementById("ca_text").value=d.text;
    if(d.rainbow!==undefined)document.getElementById("ca_rainbow").checked=d.rainbow;
    if(d.center!==undefined)document.getElementById("ca_center").checked=d.center;
    if(d.noScrolling!==undefined)document.getElementById("ca_noscroll").checked=d.noScrolling;
    if(d.scrollSpeed!==undefined)document.getElementById("ca_speed").value=d.scrollSpeed;
    if(d.fade!==undefined)document.getElementById("ca_fade").value=d.fade;
    if(d.blink!==undefined)document.getElementById("ca_blink").value=d.blink;
    if(d.bounce!==undefined)document.getElementById("ca_bounce").checked=d.bounce;
    if(d.pushIcon!==undefined)document.getElementById("ca_pushicon").value=d.pushIcon;
    if(d.icon)document.getElementById("ca_icon").value=d.icon;
    if(d.progress!==undefined&&d.progress>=0)document.getElementById("ca_prog").value=d.progress;
    toast("Loaded '"+name+"'");
  }).catch(function(e){toast("Load failed: "+e,false);});
}
function deleteCustomApp(){
  var name=document.getElementById("ca_name").value;if(!name){toast("Name required",false);return;}
  fetch("/api/custom?name="+encodeURIComponent(name),{method:"POST"}).then(function(){toast("App removed");setTimeout(loadLoop,600);});
}
function sendInd(n){apiPost("/api/indicator"+n,{color:h2r(document.getElementById("i"+n+"c").value),blink:+document.getElementById("i"+n+"b").value,fade:+document.getElementById("i"+n+"f").value});}
function sendBri(){apiPost("/api/settings",{BRI:+document.getElementById("bri").value});}
function switchApp(){var n=document.getElementById("sw_app").value;if(!n){toast("Enter app name",false);return;}apiPost("/api/switch",{name:n});}
function loadListeners(){
  var el=document.getElementById("listener_list");if(!el)return;
  fetch("/api/osc/listeners").then(function(r){return r.json();}).then(function(list){
    el.innerHTML="";
    if(!list||!list.length){el.innerHTML="<span style='color:#444'>No listeners registered.</span>";return;}
    list.forEach(function(lsr){
      var row=document.createElement("div");row.style.cssText="background:#111120;border:1px solid #2a2a4a;border-radius:4px;padding:5px 10px;margin-bottom:4px;display:flex;align-items:center;justify-content:space-between";
      var info=document.createElement("div");
      info.innerHTML='<span style="color:#8cf;font-size:11px">'+lsr.label+'</span><br><code style="color:#4af;font-size:10px">'+lsr.path+'</code> <span style="color:#555">\u2192</span> <code style="color:#fa6;font-size:10px">'+lsr.channel+'</code>';
      var bDel=document.createElement("button");bDel.textContent="\u2715";bDel.className="sm red";bDel.style.cssText="margin:0;padding:2px 8px";
      bDel.onclick=(function(p){return function(){fetch("/api/osc/listeners?path="+encodeURIComponent(p),{method:"DELETE"}).then(function(r){r.ok?toast("Removed"):toast("Error",false);loadListeners();});};})(lsr.path);
      row.appendChild(info);row.appendChild(bDel);el.appendChild(row);
    });
  }).catch(function(){el.innerHTML="<span style='color:#f66'>Error loading</span>";});
}
function addListener(){
  var selEl=document.getElementById("osc_path_txt");
  var manEl=document.getElementById("osc_path_manual");
  var path=manEl&&manEl.value.trim()?manEl.value.trim():(selEl?selEl.value:"");
  var channel=document.getElementById("osc_ch_txt").value.trim();
  var label=document.getElementById("osc_label_txt").value.trim()||channel;
  if(!path||!channel){toast("Path and channel required",false);return;}
  fetch("/api/osc/listeners",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"plain="+encodeURIComponent(JSON.stringify({path:path,channel:channel,label:label}))}).then(function(r){if(r.ok){toast("Listener added \u2192 "+channel);loadListeners();document.getElementById("osc_path_manual").value="";document.getElementById("osc_ch_txt").value="";document.getElementById("osc_label_txt").value="";}else{toast("Error",false);}});
}
loadListeners();
function saveTCSettings(){
  var hold=document.getElementById("tc_hold").checked;
  var dwell=parseInt(document.getElementById("tc_dwell").value);
  var frames=document.getElementById("tc_show_frames").checked;
  var beep=document.getElementById("tc_stop_beep").checked;
  var mute=document.getElementById("tc_mute").checked;
  apiPost("/api/dev",{tc_hold:hold,tc_dwell:dwell,tc_show_frames:frames,tc_stop_beep:beep,tc_mute:mute});
  apiPost("/api/settings",{TC_MUTE:mute});
}
fetch("/api/dev").then(function(r){return r.json();}).then(function(d){
  if(d.tc_hold!==undefined)document.getElementById("tc_hold").checked=d.tc_hold;
  if(d.tc_dwell!==undefined){document.getElementById("tc_dwell").value=d.tc_dwell;document.getElementById("tc_dwell_v").textContent=d.tc_dwell+" s";}
  if(d.tc_show_frames!==undefined)document.getElementById("tc_show_frames").checked=d.tc_show_frames;
  if(d.tc_stop_beep!==undefined)document.getElementById("tc_stop_beep").checked=d.tc_stop_beep;
  if(d.tc_mute!==undefined)document.getElementById("tc_mute").checked=d.tc_mute;
}).catch(function(){});
function saveSndSettings(){apiPost("/api/settings",{SOUND:document.getElementById("snd_en").checked});}
var _CH_NATIVE_HINTS={
  time:"Adds the Time clock to the channel rotation.",
  date:"Adds the Date display to the channel rotation.",
  timecode:"The Timecode channel activates automatically when LTC/OSC TC signals arrive — no manual add needed.",
  osc:"Create a named channel driven by OSC. Use the channel name when adding an OSC Listener."
};
function updateChType(t){
  var tf=document.getElementById("ch_fields_text"),nf=document.getElementById("ch_fields_native");
  var textTypes=t==="text"||t==="osc";
  if(tf)tf.style.display=textTypes?"":"none";
  if(nf)nf.style.display=textTypes?"none":"";
  var h=document.getElementById("ch_native_hint");
  if(h)h.textContent=_CH_NATIVE_HINTS[t]||"";
}
function addChannel(){
  var t=document.getElementById("ch_type").value;
  if(t==="time"){apiPost("/api/settings",{TIM:true}).then(function(){setTimeout(loadLoop,400);});return;}
  if(t==="date"){apiPost("/api/settings",{DAT:true}).then(function(){setTimeout(loadLoop,400);});return;}
  if(t==="timecode"){toast("TC activates on incoming signal — no action needed");return;}
  // text or osc — use the Configure Channel form fields
  var name=document.getElementById("ca_name").value.trim().replace(/\s+/g,"_");
  if(!name){toast("Enter a channel name",false);return;}
  var text=document.getElementById("ca_text").value||name;
  fetch("/api/custom?name="+encodeURIComponent(name),{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"plain="+encodeURIComponent(JSON.stringify({text:text,scrollSpeed:80}))}).then(function(r){r.ok?toast("Channel '"+name+"' created"):toast("Error",false);setTimeout(loadLoop,400);});
}
function addChannel(){
  var t=document.getElementById("ch_type").value;
  if(t==="time"){apiPost("/api/settings",{TIM:true}).then(function(){setTimeout(loadLoop,400);});return;}
  if(t==="date"){apiPost("/api/settings",{DAT:true}).then(function(){setTimeout(loadLoop,400);});return;}
  if(t==="timecode"){toast("TC channel activates automatically on incoming timecode signal");return;}
  var name=document.getElementById("new_ch_name").value.trim().replace(/\s+/g,"_");
  if(!name){toast("Enter a channel name",false);return;}
  var text=document.getElementById("new_ch_text").value||name;
  fetch("/api/custom?name="+encodeURIComponent(name),{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"plain="+encodeURIComponent(JSON.stringify({text:text,scrollSpeed:80}))}).then(function(r){r.ok?toast("Channel '"+name+"' created"):toast("Error",false);setTimeout(loadLoop,400);});
  document.getElementById("new_ch_name").value="";document.getElementById("new_ch_text").value="";
}
;data[key]=next;
  apiPost("/api/settings",data).then(function(){
    btn.textContent=(next?'\u25cf ':'\u25cb ')+name;
    btn.style.opacity=next?"1":"0.5";
    setTimeout(loadLoop,400);
  });
}
fetch("/api/settings").then(function(r){return r.json();}).then(function(s){
  if(s.MQTT_PREFIX){mqttPrefix=s.MQTT_PREFIX;var note=document.getElementById("mqtt_prefix_note");if(note)note.innerHTML='MQTT prefix: <code style="color:#4af">'+s.MQTT_PREFIX+'</code> &nbsp;&nbsp; OSC namespace: <code style="color:#4af">/dpx_tc002</code>';}
var en=document.getElementById("snd_en");var st=document.getElementById("snd_status");
  if(s.SOUND!==undefined){en.checked=s.SOUND;}else{en.checked=true;}
  if(st){st.textContent=en.checked?"(on)":"(DISABLED \u2014 check this box and Save)";st.style.color=en.checked?"#2a5":"#f66";}
  en.onchange=function(){if(st){st.textContent=en.checked?"(on)":"(DISABLED)";st.style.color=en.checked?"#2a5":"#f66";}};
}).catch(function(){});
  var v=document.getElementById("rtttl").value;if(!v){toast("Enter RTTTL string",false);return;}
  apiPost("/api/sound",{rtttl:v});
}
function sendSound(){var v=document.getElementById("snd_file").value;if(!v){toast("Enter filename",false);return;}apiPost("/api/sound",{sound:v});}
fetch("/api/settings").then(function(r){return r.json();}).then(function(s){
  if(s.MQTT_PREFIX){mqttPrefix=s.MQTT_PREFIX;var note=document.getElementById("mqtt_prefix_note");if(note)note.innerHTML='MQTT prefix: <code style="color:#4af">'+s.MQTT_PREFIX+'</code> &nbsp;&nbsp; OSC namespace: <code style="color:#4af">/dpx_tc002</code>';}
  var en=document.getElementById("snd_en");var st=document.getElementById("snd_status");
  if(s.SOUND!==undefined){en.checked=s.SOUND;}else{en.checked=true;}
  if(st){st.textContent=en.checked?"(on)":"(DISABLED \u2014 check this box and Save)";st.style.color=en.checked?"#2a5":"#f66";}
  en.onchange=function(){if(st){st.textContent=en.checked?"(on)":"(DISABLED)";st.style.color=en.checked?"#2a5":"#f66";}};
}).catch(function(){});
fetch("/list?dir=/MELODIES/").then(function(r){return r.json();}).then(function(files){var s=document.getElementById("snd_file_sel");files.filter(function(f){return f.type==="file";}).forEach(function(f){var n=f.name.replace(/\.txt$/i,"");var o=document.createElement("option");o.value=n;o.textContent=n;s.appendChild(o);});}).catch(function(){});
function loadStatus(){
  fetch("/dpx").then(function(r){return r.json();}).then(function(d){
    var el=document.getElementById("status_bar");if(el)
      el.innerHTML='<b style="color:#4af">'+(d.hostname||'dpx_tc002')+'</b> <span style="color:#333">&#9654;</span> <span>'+d.ip+'</span> <span style="color:#2a2a4a">|</span> <span>RSSI: '+d.rssi+' dBm</span> <span style="color:#2a2a4a">|</span> <span>heap: '+Math.round(d.ram/1024)+'KB</span> <span style="color:#2a2a4a">|</span> <span>up '+d.uptime+'s</span> <span style="color:#2a2a4a">|</span> app: <b style="color:#8cf">'+d.app+'</b><span style="color:#333;margin-left:auto;font-size:10px">build: '+d.build+'</span>';
    var qc=document.getElementById("n_queue_count");
    if(qc){var n=d.notif||0;qc.textContent=n>0?'('+n+' queued)':'';}
  }).catch(function(){});
}
loadStatus();
setInterval(loadStatus,15000);
</script></body></html>
)EOF";

static const char screenfull_html[] PROGMEM = R"EOF(
<!doctype html><html> <head> <title>LiveView</title> <style>body{display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; overflow: hidden; background: #000;}canvas{display: block; width: 100vw; background: #000; z-index: 1;}</style> </head> <body><canvas id=c></canvas></body> <script>const c=document.getElementById("c"), d=c.getContext("2d");const urlParams=new URLSearchParams(window.location.search);const queriedFPS=parseInt(urlParams.get('fps'));let fps=queriedFPS||10;function scd(){const t=window.innerWidth; c.width=t, c.height=t / 4;}function j(){fetch("/api/screen").then(t=> t.json()).then(t=>{d.clearRect(0, 0, c.width, c.height); d.fillStyle="#000"; for (let e=0; e < 8; e++) for (let n=0; n < 32; n++){const i=t[32 * e + n], o=(16711680 & i) >> 16, s=(65280 & i) >> 8, h=255 & i; d.fillStyle=`rgb(${o},${s},${h})`; d.fillRect(n * (c.width / 32), e * (c.height / 8), c.width / 32 - 4, c.height / 8 - 4);}setTimeout(j, 1000 / fps);});}scd();document.addEventListener("DOMContentLoaded", j);window.addEventListener("resize", scd); </script></html>
)EOF";