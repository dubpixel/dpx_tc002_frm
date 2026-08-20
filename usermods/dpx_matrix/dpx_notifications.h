// ================================================================================
// dpx_notifications.h — Notification Queue
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_notifications.h
// Purpose: One-shot notifications that interrupt the app loop, display once,
//          then vanish (or hold until dismissed if hold=true).
//
// ================================================================================

#pragma once
#include <deque>
#include "dpx_apps.h"

struct DpxNotification {
    DpxCustomApp data;
    bool         hold = false;    // true = hold until explicit dismiss
    bool         stack = true;    // true = queue, false = replace current
    unsigned long id   = 0;       // stable id for debug UI (GH #72) — NOT array
                                   // index, which shifts as items are consumed
};

static std::vector<DpxNotification> dpxNotifQueue;
static bool dpxNotifActive = false;
static DpxNotification dpxCurrentNotif;
static unsigned long dpxNotifStartMs = 0;
static unsigned long dpxNotifNextId  = 1; // 0 reserved for "no active notification"

// ── History ring buffer (GH #75) ────────────────────────────────────────────
// Small RAM-only backlog of recently-*shown* notifications so the outer
// buttons can replay something you missed. Archived only when a notification
// stops being current (natural completion or manual dismiss) — never on
// push, so a rapid-fire burst doesn't just show the newest item twice.
// front() = most recently shown.
static const size_t DPX_NOTIF_HISTORY_MAX = 8;
static std::deque<DpxNotification> dpxNotifHistory;

static void dpxArchiveCurrentNotif() {
    if (!dpxNotifActive) return;
    dpxNotifHistory.push_front(dpxCurrentNotif);
    if (dpxNotifHistory.size() > DPX_NOTIF_HISTORY_MAX) dpxNotifHistory.pop_back();
}

// Parse and enqueue a notification from JSON body (SPEC.md §5 POST /api/notify)
static bool dpxPushNotification(const char* json) {
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, json)) return false;

    DpxNotification notif;
    notif.data = dpxParseApp(json);
    if (!notif.data.valid) {
        // Minimal: just a text string
        notif.data.valid = true;
        notif.data.text = doc["text"].as<String>();
    }
    notif.hold  = doc.containsKey("hold")  ? doc["hold"].as<bool>()  : false;
    notif.stack = doc.containsKey("stack") ? doc["stack"].as<bool>() : true;
    notif.id    = dpxNotifNextId++;

    if (!notif.stack) {
        // Replace: clear queue and active
        dpxNotifQueue.clear();
        dpxNotifActive = false;
    }
    dpxNotifQueue.push_back(notif);
    dpxActivateEffect();  // ensure dpx Matrix effect is showing
    return true;
}

// Debug/recovery (GH #72): JSON snapshot of the active notification (if any)
// plus everything still queued, for a /ctrl panel to render.
static String dpxNotifQueueJson() {
    DynamicJsonDocument doc(2048);
    if (dpxNotifActive) {
        JsonObject a = doc.createNestedObject("active");
        a["id"]     = dpxCurrentNotif.id;
        a["text"]   = dpxCurrentNotif.data.text;
        a["hold"]   = dpxCurrentNotif.hold;
        a["repeat"] = dpxCurrentNotif.data.repeat;
    } else {
        doc["active"] = nullptr;
    }
    JsonArray q = doc.createNestedArray("queue");
    for (auto& n : dpxNotifQueue) {
        JsonObject o = q.createNestedObject();
        o["id"]     = n.id;
        o["text"]   = n.data.text;
        o["hold"]   = n.hold;
        o["repeat"] = n.data.repeat;
    }
    String s; serializeJson(doc, s); return s;
}

// Debug/recovery (GH #72): remove one specific queued item by id, leaving the
// rest of the queue and the currently-active notification untouched. Returns
// false if no queued item has that id (already consumed, or never existed).
static bool dpxNotifQueueDelete(unsigned long id) {
    for (auto it = dpxNotifQueue.begin(); it != dpxNotifQueue.end(); ++it) {
        if (it->id == id) { dpxNotifQueue.erase(it); return true; }
    }
    return false;
}

// Dismiss the currently active notification and advance to the next queued one.
// Does NOT clear the rest of the queue — use /api/notify/clear for that.
static void dpxDismissNotification() {
    dpxArchiveCurrentNotif();
    dpxNotifActive = false;
    dpxScroll.stop();  // clear scroll state so it doesn't bleed into next app/notif
}

// Notification tick — call from loop() after app tick
// Returns true while a notification is being displayed.
static bool dpxNotifTick() {
    if (!dpxNotifActive) {
        if (dpxNotifQueue.empty()) return false;
        dpxCurrentNotif = dpxNotifQueue.front();
        dpxNotifQueue.erase(dpxNotifQueue.begin());
        dpxNotifActive = true;
        dpxNotifStartMs = millis();
        dpxScroll.stop();
    }

    // Check duration expiry (hold notifications never auto-dismiss).
    if (!dpxCurrentNotif.hold) {
        unsigned long dur;
        if (dpxCurrentNotif.data.repeat >= 0) {
            // Scroll-count mode: repeat/dpxRenderNotification() is the real
            // completion signal (ends things early once the requested number
            // of passes finishes). `duration` here is NOT "how long to show
            // it" — it's purely a safety net against dpxRenderApp() never
            // reporting completion (e.g. text short enough to fit on screen
            // without scrolling at all, which the renderer treats as a
            // static app that never completes — GH #70). That safety net
            // must stay generous and ignore the caller's `duration` value
            // entirely: a caller-supplied short duration (or the 5s default)
            // would otherwise race ahead of legitimate multi-pass scrolling
            // and cut it off early (GH #71) — the whole point of repeat mode
            // is that scroll-count governs, not a clock.
            dur = 5UL * 60UL * 1000UL; // 5 min
        } else {
            dur = dpxCurrentNotif.data.durationMs();
            if (dur == 0) dur = 5000; // default 5s, time-based mode
        }
        if (millis() - dpxNotifStartMs >= dur) {
            dpxArchiveCurrentNotif();
            dpxNotifActive = false;
            return false;
        }
    }
    return true;
}

// Render the current notification frame.
// Also ends the notification early when finite-repeat scroll completes,
// so the display doesn't wait for the full duration after text has scrolled off.
static void dpxRenderNotification() {
    bool done = !dpxRenderApp(dpxCurrentNotif.data);
    if (done && dpxCurrentNotif.data.repeat >= 0 && !dpxCurrentNotif.hold) {
        dpxArchiveCurrentNotif();
        dpxNotifActive = false;  // scroll cycle complete — end early
    }
}

// ── History browsing (GH #75) ───────────────────────────────────────────────
// LEFT long-press with nothing currently active steps back through
// dpxNotifHistory (see dpx_matrix.h handleButton() — that slot is otherwise
// a no-op, since "dismiss notification" has nothing to do when idle).
// Auto-exits after DPX_HISTORY_ITEM_MS so you never get stuck showing an old
// message; RIGHT short-press also exits explicitly while browsing.
static bool dpxHistoryBrowsing = false;
static int  dpxHistoryIndex    = -1;
static unsigned long dpxHistoryShownMs = 0;
static const unsigned long DPX_HISTORY_ITEM_MS = 8000;

static void dpxHistoryExit() {
    dpxHistoryBrowsing = false;
    dpxHistoryIndex = -1;
    dpxScroll.stop();
}

// Enter browsing (show most recent) or step to the next-older item, wrapping
// around. No-op if there's nothing to show.
static bool dpxHistoryNext() {
    if (dpxNotifHistory.empty()) return false;
    if (!dpxHistoryBrowsing) {
        dpxHistoryBrowsing = true;
        dpxHistoryIndex = 0;
    } else {
        dpxHistoryIndex = (dpxHistoryIndex + 1) % (int)dpxNotifHistory.size();
    }
    dpxHistoryShownMs = millis();
    dpxScroll.stop();
    return true;
}

// Call once per frame alongside dpxNotifTick(). Returns true while an item
// should be shown. A real notification arriving always wins — dpx_matrix.h's
// mode_dpx_matrix() calls dpxHistoryExit() when notifActive becomes true.
static bool dpxHistoryTick() {
    if (!dpxHistoryBrowsing) return false;
    if (millis() - dpxHistoryShownMs >= DPX_HISTORY_ITEM_MS) { dpxHistoryExit(); return false; }
    return true;
}

static void dpxRenderHistoryItem() {
    if (dpxHistoryIndex < 0 || dpxHistoryIndex >= (int)dpxNotifHistory.size()) { dpxHistoryExit(); return; }
    dpxRenderApp(dpxNotifHistory[dpxHistoryIndex].data);
}

// Debug/verification (GH #75): JSON snapshot of the history ring buffer,
// same pattern as dpxNotifQueueJson() (GH #72). front-most = most recent.
static String dpxNotifHistoryJson() {
    DynamicJsonDocument doc(2048);
    doc["browsing"] = dpxHistoryBrowsing;
    doc["index"]    = dpxHistoryIndex;
    JsonArray h = doc.createNestedArray("history");
    for (auto& n : dpxNotifHistory) {
        JsonObject o = h.createNestedObject();
        o["id"]   = n.id;
        o["text"] = n.data.text;
    }
    String s; serializeJson(doc, s); return s;
}
