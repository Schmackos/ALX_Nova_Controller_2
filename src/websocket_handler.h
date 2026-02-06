#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <Arduino.h>
#include <WebSocketsServer.h>

#define MAX_WS_CLIENTS 10

// ===== WebSocket Authentication =====
extern bool wsAuthStatus[MAX_WS_CLIENTS];
extern unsigned long wsAuthTimeout[MAX_WS_CLIENTS];

// ===== WebSocket Event Handler =====
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

// ===== State Broadcasting Functions =====
void sendLEDState();
void sendBlinkingState();
void sendDisplayState();
void sendFactoryResetProgress(unsigned long secondsHeld, bool resetTriggered);
void sendRebootProgress(unsigned long secondsHeld, bool rebootTriggered);
void sendHardwareStats();
void sendMqttSettingsState();
void sendBuzzerState();

// ===== Audio Streaming =====
void sendAudioData();

// ===== CPU Utilization Tracking =====
void initCpuUsageMonitoring();
void updateCpuUsage();
float getCpuUsageCore0();
float getCpuUsageCore1();

#endif // WEBSOCKET_HANDLER_H
