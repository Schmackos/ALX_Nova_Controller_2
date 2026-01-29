#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <Arduino.h>
#include <WebSocketsServer.h>

// ===== WebSocket Event Handler =====
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

// ===== State Broadcasting Functions =====
void sendLEDState();
void sendBlinkingState();
void sendFactoryResetProgress(unsigned long secondsHeld, bool resetTriggered);
void sendRebootProgress(unsigned long secondsHeld, bool rebootTriggered);
void sendHardwareStats();

#endif // WEBSOCKET_HANDLER_H
