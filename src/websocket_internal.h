#pragma once
// websocket_internal.h — Cross-file accessor functions for WebSocket state
// shared between the split websocket_*.cpp files.
// NOT a public API — only included by websocket_*.cpp files.

#include <Arduino.h>

// ===== Auth State Mutation =====
// Called by websocket_command.cpp on connect/disconnect/auth events
void ws_auth_increment();
void ws_auth_decrement();

// ===== Auth Tracking =====
// Called by websocket_broadcast.cpp for adaptive binary rate
void ws_auth_recalibrate();
uint8_t ws_auth_count();
bool ws_any_auth();

// ===== Audio Subscription Tracking =====
// Set by command handler, read by broadcast
bool ws_is_audio_subscribed(uint8_t clientNum);
void ws_set_audio_subscribed(uint8_t clientNum, bool value);

// ===== Session ID Tracking =====
// Set by command handler during auth, read for validation
const String& ws_get_session_id(uint8_t clientNum);
void ws_set_session_id(uint8_t clientNum, const String& id);
void ws_clear_session_id(uint8_t clientNum);

// ===== HAL Device Lookup Helpers =====
// Used by both command and broadcast code; guarded by DAC_ENABLED.
#ifdef DAC_ENABLED
#include "hal/hal_audio_device.h"
uint8_t ws_hal_slot_for_compatible(const char* compat);
HalAudioDevice* ws_audio_device_for_sink_slot(uint8_t sinkSlot);
#endif
