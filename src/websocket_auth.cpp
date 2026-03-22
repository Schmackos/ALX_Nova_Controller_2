// websocket_auth.cpp — WebSocket authentication state and tracking
// Extracted from websocket_handler.cpp for modularity.
// Public functions declared in websocket_handler.h.
// Cross-file accessors declared in websocket_internal.h.

#include "websocket_handler.h"
#include "websocket_internal.h"
#include "config.h"

// ===== WebSocket Authentication Tracking =====
bool wsAuthStatus[MAX_WS_CLIENTS] = {false};
unsigned long wsAuthTimeout[MAX_WS_CLIENTS] = {0};
static String wsSessionId[MAX_WS_CLIENTS];

// ===== HTTP Page Serving Flag =====
volatile bool httpServingPage = false;

// ===== Per-client Audio Streaming Subscription =====
static bool _audioSubscribed[MAX_WS_CLIENTS] = {};

// ===== Authenticated Client Counter =====
// Tracked via webSocketEvent() connect/disconnect callbacks.
// Used by broadcast functions to skip JSON serialization when no clients are listening.
static uint8_t _wsAuthCount = 0;

bool wsAnyClientAuthenticated() { return _wsAuthCount > 0; }
uint8_t wsAuthenticatedClientCount() { return _wsAuthCount; }

// Periodic recalibration of _wsAuthCount to handle stale counts from unclean disconnects
static unsigned long _lastAuthRecount = 0;
static void _recalibrateAuthCount() {
    unsigned long now = millis();
    if (now - _lastAuthRecount < WS_AUTH_RECOUNT_INTERVAL_MS) return;
    _lastAuthRecount = now;
    uint8_t count = 0;
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (wsAuthStatus[i]) count++;
    }
    _wsAuthCount = count;
}

// ===== Cross-file Accessor Implementations (websocket_internal.h) =====

void ws_auth_increment() { _wsAuthCount++; }
void ws_auth_decrement() { if (_wsAuthCount > 0) _wsAuthCount--; }
void ws_auth_recalibrate() { _recalibrateAuthCount(); }
uint8_t ws_auth_count() { return _wsAuthCount; }
bool ws_any_auth() { return _wsAuthCount > 0; }

bool ws_is_audio_subscribed(uint8_t clientNum) {
    if (clientNum >= MAX_WS_CLIENTS) return false;
    return _audioSubscribed[clientNum];
}

void ws_set_audio_subscribed(uint8_t clientNum, bool value) {
    if (clientNum >= MAX_WS_CLIENTS) return;
    _audioSubscribed[clientNum] = value;
}

const String& ws_get_session_id(uint8_t clientNum) {
    static String empty;
    if (clientNum >= MAX_WS_CLIENTS) return empty;
    return wsSessionId[clientNum];
}

void ws_set_session_id(uint8_t clientNum, const String& id) {
    if (clientNum >= MAX_WS_CLIENTS) return;
    wsSessionId[clientNum] = id;
}

void ws_clear_session_id(uint8_t clientNum) {
    if (clientNum >= MAX_WS_CLIENTS) return;
    wsSessionId[clientNum] = "";
}
