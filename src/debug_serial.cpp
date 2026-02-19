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

#ifndef NATIVE_TEST
  // Async path: enqueue message for main-loop drain (never blocks)
  char buffer[MAX_BUFFER];
  vsnprintf(buffer, MAX_BUFFER, format, args);

  uint8_t nextHead;
  portENTER_CRITICAL_ISR(&_queueMux);
  nextHead = (_queueHead + 1) % LOG_QUEUE_SIZE;
  if (nextHead == _queueTail) {
    // Queue full — overwrite oldest entry by advancing tail
    _queueTail = (_queueTail + 1) % LOG_QUEUE_SIZE;
  }
  // Build prefixed message directly into the queue slot
  snprintf(_queue[_queueHead].msg, sizeof(_queue[_queueHead].msg),
           "%s%s", levelToPrefix(level), buffer);
  _queue[_queueHead].level = level;
  _queueHead = nextHead;
  portEXIT_CRITICAL_ISR(&_queueMux);
#else
  // Synchronous path for native tests: keep existing behavior unchanged
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
#endif
}

#ifndef NATIVE_TEST
// Drain up to LOG_FLUSH_PER_CALL entries from the ring buffer.
// Called from the main loop (Core 0 only) — no consumer-side mutex needed.
void DebugSerial::processQueue() {
  int flushed = 0;
  while (flushed < LOG_FLUSH_PER_CALL) {
    uint8_t tail;
    portENTER_CRITICAL_ISR(&_queueMux);
    if (_queueHead == _queueTail) {
      portEXIT_CRITICAL_ISR(&_queueMux);
      break;  // Queue empty
    }
    tail = _queueTail;
    _queueTail = (_queueTail + 1) % LOG_QUEUE_SIZE;
    portEXIT_CRITICAL_ISR(&_queueMux);

    // Serial output (non-blocking: UART TX buffer is large enough for one line)
    Serial.print(_queue[tail].msg);
    Serial.println();

    // WebSocket broadcast
    broadcastLine(String(_queue[tail].msg), _queue[tail].level);
    flushed++;
  }
}
#endif

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
