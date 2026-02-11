#ifndef CRASH_LOG_H
#define CRASH_LOG_H

#include <Arduino.h>

// ===== Crash Log Ring Buffer =====
// Persisted as binary in /crashlog.bin on LittleFS
// Stores last 10 boot entries with reset reason, heap stats, and timestamp

#define CRASH_LOG_MAX_ENTRIES 10

struct CrashLogEntry {
    char reason[24];      // Reset reason string (e.g., "task_watchdog")
    uint32_t heapFree;    // Free heap at boot (bytes)
    uint32_t heapMinFree; // Min free heap (lifetime, bytes)
    char timestamp[24];   // ISO 8601 timestamp (backfilled after NTP sync)
};

struct CrashLogData {
    uint8_t count;        // Number of valid entries (0..CRASH_LOG_MAX_ENTRIES)
    uint8_t writeIndex;   // Next write position (ring buffer)
    CrashLogEntry entries[CRASH_LOG_MAX_ENTRIES];
};

// Record current boot's reset reason and heap stats to the ring buffer.
// Call once in setup() after LittleFS.begin().
void crashlog_record(const String &reason);

// Backfill the latest entry's timestamp after NTP sync succeeds.
// Call once from syncTimeWithNTP().
void crashlog_update_timestamp();

// Returns true if the given reset reason indicates an abnormal crash
// (panic, watchdog, brownout) rather than a normal boot (power_on, software_reset).
bool crashlog_was_crash(const char *reason);

// Returns true if the most recent boot was a crash.
bool crashlog_last_was_crash();

// Read-only access to the crash log ring buffer.
const CrashLogData &crashlog_get();

// Get the Nth most recent entry (0 = most recent).
// Returns nullptr if index >= count.
const CrashLogEntry *crashlog_get_recent(int index);

#endif // CRASH_LOG_H
