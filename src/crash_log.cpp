#include "crash_log.h"
#include "debug_serial.h"
#include <LittleFS.h>

static const char *CRASHLOG_FILE = "/crashlog.bin";
static CrashLogData _crashLog = {};
static bool _loaded = false;

// ===== Internal helpers =====

static void crashlog_load() {
    if (_loaded) return;
    _loaded = true;

    File f = LittleFS.open(CRASHLOG_FILE, "r");
    if (!f || f.size() != sizeof(CrashLogData)) {
        // No file or corrupt — start fresh
        _crashLog = {};
        if (f) f.close();
        return;
    }
    f.read((uint8_t *)&_crashLog, sizeof(CrashLogData));
    f.close();

    // Sanity check
    if (_crashLog.count > CRASH_LOG_MAX_ENTRIES) _crashLog.count = 0;
    if (_crashLog.writeIndex >= CRASH_LOG_MAX_ENTRIES) _crashLog.writeIndex = 0;
}

static void crashlog_save() {
    File f = LittleFS.open(CRASHLOG_FILE, "w");
    if (!f) {
        LOG_E("[CrashLog] Failed to write %s", CRASHLOG_FILE);
        return;
    }
    f.write((const uint8_t *)&_crashLog, sizeof(CrashLogData));
    f.close();
}

// ===== Public API =====

void crashlog_record(const String &reason) {
    crashlog_load();

    CrashLogEntry &entry = _crashLog.entries[_crashLog.writeIndex];
    memset(&entry, 0, sizeof(CrashLogEntry));

    // Copy reason (truncate to fit)
    strncpy(entry.reason, reason.c_str(), sizeof(entry.reason) - 1);
    entry.reason[sizeof(entry.reason) - 1] = '\0';

    // Heap stats at boot
    entry.heapFree = ESP.getFreeHeap();
    entry.heapMinFree = ESP.getMinFreeHeap();

    // Timestamp will be empty until NTP sync
    entry.timestamp[0] = '\0';

    // Advance ring buffer
    _crashLog.writeIndex = (_crashLog.writeIndex + 1) % CRASH_LOG_MAX_ENTRIES;
    if (_crashLog.count < CRASH_LOG_MAX_ENTRIES) _crashLog.count++;

    crashlog_save();

    LOG_I("[CrashLog] Recorded: %s (heap=%lu, minHeap=%lu)",
          entry.reason, (unsigned long)entry.heapFree, (unsigned long)entry.heapMinFree);
}

void crashlog_update_timestamp() {
    crashlog_load();
    if (_crashLog.count == 0) return;

    // Most recent entry is one before writeIndex
    int idx = (_crashLog.writeIndex + CRASH_LOG_MAX_ENTRIES - 1) % CRASH_LOG_MAX_ENTRIES;
    CrashLogEntry &entry = _crashLog.entries[idx];

    // Only backfill if timestamp is empty
    if (entry.timestamp[0] != '\0') return;

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        strftime(entry.timestamp, sizeof(entry.timestamp), "%Y-%m-%dT%H:%M:%S", &timeinfo);
        crashlog_save();
        LOG_I("[CrashLog] Timestamp updated: %s", entry.timestamp);
    }
}

bool crashlog_was_crash(const char *reason) {
    if (!reason) return false;
    // These reset reasons indicate abnormal crashes
    return (strcmp(reason, "exception_panic") == 0 ||
            strcmp(reason, "task_watchdog") == 0 ||
            strcmp(reason, "interrupt_watchdog") == 0 ||
            strcmp(reason, "other_watchdog") == 0 ||
            strcmp(reason, "brownout") == 0);
}

bool crashlog_last_was_crash() {
    crashlog_load();
    if (_crashLog.count == 0) return false;
    int idx = (_crashLog.writeIndex + CRASH_LOG_MAX_ENTRIES - 1) % CRASH_LOG_MAX_ENTRIES;
    return crashlog_was_crash(_crashLog.entries[idx].reason);
}

const CrashLogData &crashlog_get() {
    crashlog_load();
    return _crashLog;
}

const CrashLogEntry *crashlog_get_recent(int index) {
    crashlog_load();
    if (index < 0 || index >= (int)_crashLog.count) return nullptr;
    // index 0 = most recent (one before writeIndex)
    int idx = (_crashLog.writeIndex + CRASH_LOG_MAX_ENTRIES - 1 - index) % CRASH_LOG_MAX_ENTRIES;
    return &_crashLog.entries[idx];
}
