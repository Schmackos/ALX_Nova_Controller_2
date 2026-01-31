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
  LOG_ERROR = 3  // Errors
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
};

extern DebugSerial DebugOut;

// Convenience macros for logging
#define LOG_D(fmt, ...) DebugOut.debug(fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) DebugOut.info(fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) DebugOut.warn(fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...) DebugOut.error(fmt, ##__VA_ARGS__)

#endif // DEBUG_SERIAL_H
