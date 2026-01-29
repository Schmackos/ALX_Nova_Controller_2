#ifndef DEBUG_SERIAL_H
#define DEBUG_SERIAL_H

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

class DebugSerial : public Print {
public:
    void begin(unsigned long baud);
    void setWebSocket(WebSocketsServer* ws);
    
    virtual size_t write(uint8_t c) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;
    
    // Flush buffer and send to WebSocket
    void flush();
    
private:
    WebSocketsServer* _webSocket = nullptr;
    String _lineBuffer;
    unsigned long _lastFlush = 0;
    static const size_t MAX_BUFFER = 256;
    
    void broadcastLine(const String& line);
};

extern DebugSerial DebugOut;

#endif // DEBUG_SERIAL_H
