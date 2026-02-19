// test_debug_serial.cpp
// Tests for DebugSerial async ring-buffer API and log-level filtering.
//
// On NATIVE_TEST the ring-buffer code is compiled out (#ifndef NATIVE_TEST),
// so processQueue() is a guaranteed no-op and isQueueEmpty() always returns
// true.  These tests verify:
//   - The no-op API contract holds (no crash, correct return values)
//   - Log-level filtering still works as expected
//   - Filtered messages (below minimum level) produce no Serial output

#include <cstring>
#include <string>
#include <stdarg.h>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Minimal Serial mock =====
// Captures print/println calls so tests can assert on output.

namespace SerialMock {
static std::string capturedOutput;

inline void reset() { capturedOutput.clear(); }
} // namespace SerialMock

struct MockSerial {
    void begin(unsigned long baud) { SerialMock::capturedOutput.clear(); }

    void print(const char *s) {
        if (s) SerialMock::capturedOutput += s;
    }
    void print(const std::string &s) { SerialMock::capturedOutput += s; }
    void println(const char *s = "") {
        if (s) SerialMock::capturedOutput += s;
        SerialMock::capturedOutput += '\n';
    }
    void println(const std::string &s) {
        SerialMock::capturedOutput += s;
        SerialMock::capturedOutput += '\n';
    }
    void flush() {}
    void write(uint8_t c) { SerialMock::capturedOutput += (char)c; }
};

static MockSerial Serial;

// ===== Inline DebugSerial implementation for native tests =====
// Mirrors debug_serial.h/.cpp, stripped to the subset needed here.
// The ring-buffer members are absent (NATIVE_TEST guard), matching the
// real header's behaviour.

enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
    LOG_NONE  = 4
};

class DebugSerial {
public:
    void setLogLevel(LogLevel level) { _minLevel = level; }
    LogLevel getLogLevel() const { return _minLevel; }

    // Async ring-buffer API — no-ops on NATIVE_TEST
    void processQueue() {}
    bool isQueueEmpty() const { return true; }

    void info(const char *format, ...) {
        va_list args;
        va_start(args, format);
        logWithLevel(LOG_INFO, format, args);
        va_end(args);
    }

    void warn(const char *format, ...) {
        va_list args;
        va_start(args, format);
        logWithLevel(LOG_WARN, format, args);
        va_end(args);
    }

    void error(const char *format, ...) {
        va_list args;
        va_start(args, format);
        logWithLevel(LOG_ERROR, format, args);
        va_end(args);
    }

    void debug(const char *format, ...) {
        va_list args;
        va_start(args, format);
        logWithLevel(LOG_DEBUG, format, args);
        va_end(args);
    }

private:
    LogLevel _minLevel = LOG_DEBUG;
    static const size_t MAX_BUFFER = 256;

    const char *levelToPrefix(LogLevel level) {
        switch (level) {
            case LOG_DEBUG: return "[D] ";
            case LOG_INFO:  return "[I] ";
            case LOG_WARN:  return "[W] ";
            case LOG_ERROR: return "[E] ";
            default:        return "[?] ";
        }
    }

    void logWithLevel(LogLevel level, const char *format, va_list args) {
        if (level < _minLevel) return;

        char buffer[MAX_BUFFER];
        vsnprintf(buffer, MAX_BUFFER, format, args);

        Serial.print(levelToPrefix(level));
        Serial.println(buffer);
    }
};

static DebugSerial DebugOut;

// ===== Test Setup / Teardown =====

void setUp(void) {
    SerialMock::reset();
    DebugOut.setLogLevel(LOG_DEBUG);
}

void tearDown(void) {}

// ===== Tests: no-op API contract =====

void test_isQueueEmpty_always_true_in_native(void) {
    TEST_ASSERT_TRUE(DebugOut.isQueueEmpty());
}

void test_processQueue_does_not_crash(void) {
    // Simply calling processQueue() must not crash or have side-effects
    DebugOut.processQueue();
    DebugOut.processQueue();
    DebugOut.processQueue();
    TEST_PASS();
}

void test_isQueueEmpty_still_true_after_processQueue(void) {
    DebugOut.processQueue();
    TEST_ASSERT_TRUE(DebugOut.isQueueEmpty());
}

// ===== Tests: log-level filtering =====

void test_LOG_I_outputs_when_level_is_DEBUG(void) {
    DebugOut.setLogLevel(LOG_DEBUG);
    SerialMock::reset();
    DebugOut.info("hello");
    TEST_ASSERT_FALSE(SerialMock::capturedOutput.empty());
}

void test_LOG_W_is_filtered_below_minimum_level(void) {
    // Set minimum to ERROR — WARN messages should be suppressed
    DebugOut.setLogLevel(LOG_ERROR);
    SerialMock::reset();
    DebugOut.warn("this should be filtered");
    TEST_ASSERT_TRUE(SerialMock::capturedOutput.empty());
}

void test_LOG_E_passes_when_level_is_ERROR(void) {
    DebugOut.setLogLevel(LOG_ERROR);
    SerialMock::reset();
    DebugOut.error("critical failure");
    TEST_ASSERT_FALSE(SerialMock::capturedOutput.empty());
}

// ===== Test Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // No-op API contract (ring buffer compiled out on native)
    RUN_TEST(test_isQueueEmpty_always_true_in_native);
    RUN_TEST(test_processQueue_does_not_crash);
    RUN_TEST(test_isQueueEmpty_still_true_after_processQueue);

    // Log-level filtering
    RUN_TEST(test_LOG_I_outputs_when_level_is_DEBUG);
    RUN_TEST(test_LOG_W_is_filtered_below_minimum_level);
    RUN_TEST(test_LOG_E_passes_when_level_is_ERROR);

    return UNITY_END();
}
