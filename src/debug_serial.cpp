#include "debug_serial.h"

// Global instance
DebugSerial DebugOut;

void DebugSerial::begin(unsigned long baud) {
    Serial.begin(baud);
    _lineBuffer.reserve(MAX_BUFFER);
}

void DebugSerial::setWebSocket(WebSocketsServer* ws) {
    _webSocket = ws;
}

size_t DebugSerial::write(uint8_t c) {
    // Always write to hardware Serial
    Serial.write(c);
    
    // Buffer for WebSocket broadcast
    if (c == '\n') {
        // End of line - broadcast the complete line
        broadcastLine(_lineBuffer);
        _lineBuffer = "";
    } else if (c == '\r') {
        // Ignore carriage returns
    } else {
        // Add character to buffer
        _lineBuffer += (char)c;
        
        // If buffer is full, flush it
        if (_lineBuffer.length() >= MAX_BUFFER) {
            broadcastLine(_lineBuffer);
            _lineBuffer = "";
        }
    }
    
    return 1;
}

size_t DebugSerial::write(const uint8_t *buffer, size_t size) {
    size_t written = 0;
    for (size_t i = 0; i < size; i++) {
        written += write(buffer[i]);
    }
    return written;
}

void DebugSerial::flush() {
    Serial.flush();
    
    // If there's buffered content, send it
    if (_lineBuffer.length() > 0) {
        broadcastLine(_lineBuffer);
        _lineBuffer = "";
    }
}

void DebugSerial::broadcastLine(const String& line) {
    // Only broadcast if WebSocket is connected and line is not empty
    if (_webSocket == nullptr || line.length() == 0) {
        return;
    }
    
    // Create JSON message
    JsonDocument doc;
    doc["type"] = "debugLog";
    doc["timestamp"] = millis();
    doc["message"] = line;
    
    String json;
    serializeJson(doc, json);
    
    // Broadcast to all connected WebSocket clients
    _webSocket->broadcastTXT((uint8_t*)json.c_str(), json.length());
}
