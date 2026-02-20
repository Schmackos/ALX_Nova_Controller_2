#ifndef DEBUG_SERIAL_H
#define DEBUG_SERIAL_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include <stdarg.h>


// ===== Log Levels =====
enum LogLevel {
  LOG_DEBUG = 0, // Detailed debugging info
  LOG_INFO = 1,  // General information
  LOG_WARN = 2,  // Warnings
  LOG_ERROR = 3, // Errors
  LOG_NONE = 4   // Suppress all serial output
};

// Current minimum log level (messages below this are filtered)
extern LogLevel currentLogLevel;

class DebugSerial : public Print {
public:
  void begin(unsigned long baud);
  void setWebSocket(WebSocketsServer *ws);
  void setLogLevel(LogLevel level) { _minLevel = level; }
  LogLevel getLogLevel() const { return _minLevel; }

  virtual size_t write(uint8_t c) override;
  virtual size_t write(const uint8_t *buffer, size_t size) override;

  // Flush buffer and send to WebSocket
  void flush();

  // ===== Log Level Methods =====
  void debug(const char *format, ...);
  void info(const char *format, ...);
  void warn(const char *format, ...);
  void error(const char *format, ...);
  void log(LogLevel level, const char *format, ...);

  // Async ring buffer for non-blocking LOG calls (ESP32 only)
#ifndef NATIVE_TEST
  void processQueue();
  bool isQueueEmpty() const { return _queueHead == _queueTail; }
#else
  void processQueue() {}  // no-op for native tests
  bool isQueueEmpty() const { return true; }
#endif

private:
  WebSocketsServer *_webSocket = nullptr;
  String _lineBuffer;
  unsigned long _lastFlush = 0;
  LogLevel _minLevel = LOG_DEBUG;       // Default: show all
  LogLevel _currentMsgLevel = LOG_INFO; // Level of current message being built
  static const size_t MAX_BUFFER = 256;

  void broadcastLine(const String &line, LogLevel level = LOG_INFO);
  void logWithLevel(LogLevel level, const char *format, va_list args);
  const char *levelToString(LogLevel level);
  const char *levelToPrefix(LogLevel level);

#ifndef NATIVE_TEST
  static const uint8_t LOG_QUEUE_SIZE = 16;
  static const int LOG_FLUSH_PER_CALL = 4;

  struct LogEntry {
    char msg[256];
    LogLevel level;
  };

  LogEntry *_queue = nullptr;
  volatile uint8_t _queueHead = 0;  // write index (producer)
  volatile uint8_t _queueTail = 0;  // read index (consumer, main loop only)

  portMUX_TYPE _queueMux = portMUX_INITIALIZER_UNLOCKED;
#endif
};

extern DebugSerial DebugOut;

// Apply debug serial level from AppState debug toggles
inline void applyDebugSerialLevel(bool masterEnabled, int level) {
  if (!masterEnabled) {
    DebugOut.setLogLevel(LOG_ERROR); // Master off = errors only
    return;
  }
  switch (level) {
    case 0: DebugOut.setLogLevel(LOG_NONE); break;
    case 1: DebugOut.setLogLevel(LOG_ERROR); break;
    case 2: DebugOut.setLogLevel(LOG_INFO); break;
    case 3: DebugOut.setLogLevel(LOG_DEBUG); break;
    default: DebugOut.setLogLevel(LOG_INFO); break;
  }
}

// Convenience macros for logging
#define LOG_D(fmt, ...) DebugOut.debug(fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) DebugOut.info(fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) DebugOut.warn(fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...) DebugOut.error(fmt, ##__VA_ARGS__)

#endif // DEBUG_SERIAL_H
