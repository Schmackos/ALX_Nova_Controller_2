#pragma once
// Diagnostic journal — hot ring buffer + LittleFS persistence.
//
// Single entry point: diag_emit() writes to the 32-entry PSRAM hot ring,
// sets the EVT_DIAG dirty flag, and emits a [DIAG] JSON line on Serial.
// diag_journal_flush() is called periodically (every 60s) and on shutdown
// to persist WARN+ entries to /diag_journal.bin on LittleFS.
//
// Thread safety: portMUX spinlock — safe from both cores and task context.
// Critical section duration: ~1 µs (memcpy 64 bytes + index arithmetic).
// Do NOT call from ISR context (Serial.printf inside diag_emit is not ISR-safe;
// only the ring-buffer write itself is spinlock-protected).

#include "diag_event.h"
#include "diag_error_codes.h"

// Initialize the journal: allocate the hot ring buffer (PSRAM when available,
// internal heap when not), and load the monotonic seq counter from NVS.
// Call once from setup() after LittleFS.begin() and before any diag_emit().
void diag_journal_init();

// ===== Primary emit functions =====

// Record a structured diagnostic event. Automatically populates:
//   seq (monotonic, NVS-backed), bootId, timestamp (millis()), heapFree.
// Writes to hot ring buffer, sets appState._diagJournalDirty, signals EVT_DIAG.
// On firmware: also prints [DIAG] JSON line on Serial.
// corrId defaults to 0 (standalone event).
void diag_emit(DiagErrorCode code, DiagSeverity severity, uint8_t slot,
               const char* device, const char* msg);

// Overload: same as above but with explicit correlation ID.
// Use corrId to group related events (e.g., a retry sequence).
void diag_emit(DiagErrorCode code, DiagSeverity severity, uint8_t slot,
               const char* device, const char* msg, uint16_t corrId);

// ===== Flush / Persistence =====

// Append WARN+ entries from the hot buffer to /diag_journal.bin on LittleFS.
// Per-entry CRC32 guards against power-loss corruption.
// When the file exceeds DIAG_JOURNAL_MAX_ENTRIES, it is truncated and restarted.
// Safe to call from the main loop (not from the audio task or ISR).
void diag_journal_flush();

// ===== Hot buffer accessors =====

// Read entry from the hot ring buffer by recency index.
//   index 0 = most recently written entry.
//   index 1 = second most recent, etc.
// Returns false when index >= diag_journal_count() or buffer is empty.
bool diag_journal_read(uint8_t index, DiagEvent* out);

// Number of valid entries currently in the hot ring buffer (0..DIAG_JOURNAL_HOT_ENTRIES).
uint8_t diag_journal_count();

// Copy the most recent event into *out. Returns false when buffer is empty.
// Convenience wrapper for WS/MQTT broadcast on dirty-flag wakeup.
bool diag_journal_latest(DiagEvent* out);

// ===== Housekeeping =====

// Erase both the hot ring buffer and the persistent LittleFS file.
void diag_journal_clear();

// Current value of the monotonic sequence counter (next seq to be assigned).
uint32_t diag_journal_seq();

// Set the boot session ID. Call once at startup from crashlog_record() or setup().
// The ID is embedded in every subsequent DiagEvent so events can be grouped by boot.
void diag_journal_set_boot_id(uint32_t id);

// Test-only: reset all internal state (ring buffer, seq, boot ID, mock FS).
#ifdef UNIT_TEST
void diag_journal_reset_for_test();
#endif
