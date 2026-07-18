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

// Dismiss the currently active held notification
static void dpxDismissNotification() {
    dpxNotifActive = false;
    dpxNotifQueue.erase(dpxNotifQueue.begin(), dpxNotifQueue.end());
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

    // Check duration expiry (hold notifications never auto-dismiss)
    if (!dpxCurrentNotif.hold) {
        unsigned long dur = dpxCurrentNotif.data.durationMs();
        if (dur == 0) dur = 5000; // default 5s
        if (millis() - dpxNotifStartMs >= dur) {
            dpxNotifActive = false;
            return false;
        }
    }
    return true;
}

// Render the current notification (call from handleOverlayDraw() when notifTick() = true)
static void dpxRenderNotification() {
    dpxRenderApp(dpxCurrentNotif.data);
}
