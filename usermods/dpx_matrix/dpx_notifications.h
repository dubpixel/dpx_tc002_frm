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
#include "dpx_apps.h"

struct DpxNotification {
    DpxCustomApp data;
    bool         hold = false;    // true = hold until explicit dismiss
    bool         stack = true;    // true = queue, false = replace current
};

static std::vector<DpxNotification> dpxNotifQueue;
static bool dpxNotifActive = false;
static DpxNotification dpxCurrentNotif;
static unsigned long dpxNotifStartMs = 0;

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

    if (!notif.stack) {
        // Replace: clear queue and active
        dpxNotifQueue.clear();
        dpxNotifActive = false;
    }
    dpxNotifQueue.push_back(notif);
    dpxActivateEffect();  // ensure dpx Matrix effect is showing
    return true;
}

// Dismiss the currently active notification and advance to the next queued one.
// Does NOT clear the rest of the queue — use /api/notify/clear for that.
static void dpxDismissNotification() {
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
        dpxNotifActive = false;  // scroll cycle complete — end early
    }
}
