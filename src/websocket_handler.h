#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <Arduino.h>
#include <WebSocketsServer.h>

#define MAX_WS_CLIENTS 16

// ===== Binary WebSocket Message Types =====
#define WS_BIN_WAVEFORM 0x01  // [type:1][adc:1][samples:256] = 258 bytes
#define WS_BIN_SPECTRUM 0x02  // [type:1][adc:1][freq:f32LE][bands:16xf32LE] = 70 bytes

// ===== WebSocket Authentication =====
extern bool wsAuthStatus[MAX_WS_CLIENTS];
extern unsigned long wsAuthTimeout[MAX_WS_CLIENTS];

// ===== WebSocket Event Handler =====
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

// ===== State Broadcasting Functions =====
void sendDisplayState();
void sendFactoryResetProgress(unsigned long secondsHeld, bool resetTriggered);
void sendRebootProgress(unsigned long secondsHeld, bool rebootTriggered);
void sendHardwareStats();
void sendMqttSettingsState();
void sendBuzzerState();
void sendSignalGenState();
void sendAudioGraphState();
void sendDebugState();
#ifdef DSP_ENABLED
void sendDspState();
void sendDspMetrics();
#endif
#ifdef DAC_ENABLED
void sendDacState();
void sendHalDeviceState();
void sendAudioChannelMap();
#endif
#ifdef USB_AUDIO_ENABLED
void sendUsbAudioState();
#endif

// ===== Health Check Broadcast =====
void sendHealthCheckState();

// ===== Diagnostic Event Broadcast =====
void sendDiagEvent();

// ===== Deferred Initial-State Drain =====
void drainPendingInitState();

// ===== Audio Streaming =====
void sendAudioData();

// ===== Authenticated Client Guard =====
// Returns true if at least one WS client has completed authentication.
// Used by broadcast functions to skip JSON serialization when no clients are listening.
bool wsAnyClientAuthenticated();
uint8_t wsAuthenticatedClientCount();

// ===== Forced Disconnect =====
// Disconnect all authenticated clients and clear auth state (called on password change).
void ws_disconnect_all_clients();

// ===== HTTP Page Serving Flag =====
extern volatile bool httpServingPage;

// ===== CPU Utilization Tracking =====
void initCpuUsageMonitoring();
void deinitCpuUsageMonitoring();
void updateCpuUsage();
float getCpuUsageCore0();
float getCpuUsageCore1();

#endif // WEBSOCKET_HANDLER_H
