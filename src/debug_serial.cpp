#include "debug_serial.h"

// Global instance
DebugSerial DebugOut;
LogLevel currentLogLevel = LOG_DEBUG;

void DebugSerial::begin(unsigned long baud) {
  Serial.begin(baud);
  _lineBuffer.reserve(MAX_BUFFER);
}

void DebugSerial::setWebSocket(WebSocketsServer *ws) { _webSocket = ws; }

size_t DebugSerial::write(uint8_t c) {
  // Always write to hardware Serial
  Serial.write(c);

  // Buffer for WebSocket broadcast
  if (c == '\n') {
    // End of line - broadcast the complete line
    broadcastLine(_lineBuffer, _currentMsgLevel);
    _lineBuffer = "";
    _currentMsgLevel = LOG_INFO; // Reset to default level
  } else if (c == '\r') {
    // Ignore carriage returns
  } else {
    // Add character to buffer
    _lineBuffer += (char)c;

    // If buffer is full, flush it
    if (_lineBuffer.length() >= MAX_BUFFER) {
      broadcastLine(_lineBuffer, _currentMsgLevel);
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
    broadcastLine(_lineBuffer, _currentMsgLevel);
    _lineBuffer = "";
  }
}

void DebugSerial::broadcastLine(const String &line, LogLevel level) {
  // Only broadcast if WebSocket is connected and line is not empty
  if (_webSocket == nullptr || line.length() == 0) {
    return;
  }

  // Filter by log level
  if (level < _minLevel) {
    return;
  }

  // Create JSON message
  JsonDocument doc;
  doc["type"] = "debugLog";
  doc["timestamp"] = millis();
  doc["level"] = levelToString(level);
  doc["message"] = line;

  String json;
  serializeJson(doc, json);

  // Broadcast to all connected WebSocket clients
  _webSocket->broadcastTXT((uint8_t *)json.c_str(), json.length());
}

// ===== Log Level Methods =====
void DebugSerial::logWithLevel(LogLevel level, const char *format,
                               va_list args) {
  if (level < _minLevel) {
    return;
  }

  _currentMsgLevel = level;

  // Format the message
  char buffer[MAX_BUFFER];
  vsnprintf(buffer, MAX_BUFFER, format, args);

  // Print with level prefix
  Serial.print(levelToPrefix(level));
  Serial.println(buffer);

  // Broadcast via WebSocket
  String prefixedMsg = String(levelToPrefix(level)) + buffer;
  broadcastLine(prefixedMsg, level);
}

void DebugSerial::debug(const char *format, ...) {
  va_list args;
  va_start(args, format);
  logWithLevel(LOG_DEBUG, format, args);
  va_end(args);
}

void DebugSerial::info(const char *format, ...) {
  va_list args;
  va_start(args, format);
  logWithLevel(LOG_INFO, format, args);
  va_end(args);
}

void DebugSerial::warn(const char *format, ...) {
  va_list args;
  va_start(args, format);
  logWithLevel(LOG_WARN, format, args);
  va_end(args);
}

void DebugSerial::error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  logWithLevel(LOG_ERROR, format, args);
  va_end(args);
}

void DebugSerial::log(LogLevel level, const char *format, ...) {
  va_list args;
  va_start(args, format);
  logWithLevel(level, format, args);
  va_end(args);
}

const char *DebugSerial::levelToString(LogLevel level) {
  switch (level) {
  case LOG_DEBUG:
    return "debug";
  case LOG_INFO:
    return "info";
  case LOG_WARN:
    return "warn";
  case LOG_ERROR:
    return "error";
  default:
    return "info";
  }
}

const char *DebugSerial::levelToPrefix(LogLevel level) {
  switch (level) {
  case LOG_DEBUG:
    return "[D] ";
  case LOG_INFO:
    return "[I] ";
  case LOG_WARN:
    return "[W] ";
  case LOG_ERROR:
    return "[E] ";
  default:
    return "[?] ";
  }
}
