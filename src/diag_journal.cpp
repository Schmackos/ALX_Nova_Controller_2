// diag_journal.cpp — Diagnostic journal implementation.
//
// Design constraints:
//   - Hot ring buffer in PSRAM (2 KB). Falls back to heap on UNIT_TEST / no PSRAM.
//   - Spinlock (portMUX) guards ring-buffer writes — safe from both cores, task context.
//     NOT safe from ISRs (Serial.println inside diag_emit is not ISR-safe).
//   - Sequence counter is NVS-backed (Preferences) so it survives reboots.
//   - Flush writes only WARN+ entries to LittleFS, with per-entry CRC32.
//   - File format: 16-byte header + N × 68-byte records (64B DiagEvent + 4B CRC32).
//   - Under UNIT_TEST: no NVS, no Serial, no portMUX (all no-ops).

#include "diag_journal.h"
#include "app_state.h"
#include "config.h"

// ===== Platform includes =====
#ifdef NATIVE_TEST
#include "../test/test_mocks/Arduino.h"
#include "../test/test_mocks/LittleFS.h"
#include "../test/test_mocks/Preferences.h"

#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)

#define DIAG_ENTER_CRITICAL()
#define DIAG_EXIT_CRITICAL()

#else // ---- Firmware build ----

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "debug_serial.h"
#include "psram_alloc.h"

static portMUX_TYPE _diagMux = portMUX_INITIALIZER_UNLOCKED;
#define DIAG_ENTER_CRITICAL()   portENTER_CRITICAL(&_diagMux)
#define DIAG_EXIT_CRITICAL()    portEXIT_CRITICAL(&_diagMux)

#endif // NATIVE_TEST

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ===== CRC32 — IEEE 802.3, reflected polynomial =====

static uint32_t _crc32_table[256];
static bool     _crc32_ready = false;

static void crc32_init_table() {
    if (_crc32_ready) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        _crc32_table[i] = c;
    }
    _crc32_ready = true;
}

static uint32_t crc32_compute(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; i++)
        crc = _crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFUL;
}

// ===== File format constants =====

static const uint32_t JOURNAL_MAGIC   = 0x44494147UL; // "DIAG" LE
static const uint8_t  JOURNAL_VERSION = 1;
static const size_t   HEADER_SIZE     = 16;

struct JournalHeader {
    uint32_t magic;
    uint8_t  version;
    uint8_t  _pad[3];
    uint32_t entryCount;
    uint32_t headIndex;
};
static_assert(sizeof(JournalHeader) == HEADER_SIZE, "JournalHeader must be 16 bytes");

// ===== Hot ring buffer state =====

static DiagEvent* _hotRing  = nullptr;
static uint8_t    _hotHead  = 0;  // Index of oldest entry
static uint8_t    _hotCount = 0;  // Valid entries (0..HOT_ENTRIES)

// ===== Sequence counter + boot ID =====

static uint32_t _seq    = 0;
static uint32_t _bootId = 0;

// NVS wear reduction: save seq every N emits.
static const uint8_t SEQ_SAVE_INTERVAL = 16;
static uint8_t _seqSaveCounter = 0;

static void nvs_load_seq() {
#ifndef UNIT_TEST
    Preferences prefs;
    if (prefs.begin("diag", true)) {
        _seq = (uint32_t)prefs.getInt("diag_seq", 0);
        prefs.end();
    }
#endif
}

static void nvs_save_seq() {
#ifndef UNIT_TEST
    Preferences prefs;
    if (prefs.begin("diag", false)) {
        prefs.putInt("diag_seq", (int)_seq);
        prefs.end();
    }
#endif
}

// ===== Serial JSON output =====

static void diag_serial_emit(const DiagEvent& ev) {
#ifndef UNIT_TEST
    char buf[160];
    snprintf(buf, sizeof(buf),
        "[DIAG] {\"seq\":%lu,\"c\":\"0x%04X\",\"sub\":\"%s\","
        "\"dev\":\"%s\",\"sev\":\"%s\",\"msg\":\"%s\"}",
        (unsigned long)ev.seq, (unsigned int)ev.code,
        diag_subsystem_name(diag_subsystem_from_code((DiagErrorCode)ev.code)),
        ev.device, diag_severity_char((DiagSeverity)ev.severity), ev.message);
    Serial.println(buf);
#else
    (void)ev;
#endif
}

// ===== Public API =====

void diag_journal_init() {
    crc32_init_table();
    nvs_load_seq();

#ifdef UNIT_TEST
    if (_hotRing) free(_hotRing);
    _hotRing = static_cast<DiagEvent*>(calloc(DIAG_JOURNAL_HOT_ENTRIES, sizeof(DiagEvent)));
#else
    if (_hotRing) {
        psram_free(_hotRing, "diag_ring");
        _hotRing = nullptr;
    }
    _hotRing = static_cast<DiagEvent*>(
        psram_alloc(DIAG_JOURNAL_HOT_ENTRIES, sizeof(DiagEvent), "diag_ring"));
#endif

    if (!_hotRing) {
        LOG_E("[DIAG] Ring alloc failed — journal disabled");
        return;
    }

    _hotHead  = 0;
    _hotCount = 0;
    LOG_I("[DIAG] Journal init (seq=%lu, boot=%lu)",
          (unsigned long)_seq, (unsigned long)_bootId);
}

void diag_journal_set_boot_id(uint32_t id) { _bootId = id; }
uint32_t diag_journal_seq() { return _seq; }

// Internal emit — both overloads funnel here.
static void diag_emit_internal(DiagErrorCode code, DiagSeverity severity,
                                uint8_t slot, const char* device,
                                const char* msg, uint16_t corrId) {
    if (!_hotRing) return;

    DiagEvent ev = diag_event_create(code, severity, slot, device, msg);
    ev.bootId   = _bootId;
    ev.timestamp = (uint32_t)millis();
    ev.heapFree  = (uint32_t)ESP.getFreeHeap();
    ev.corrId    = corrId;

    DIAG_ENTER_CRITICAL();

    ev.seq = _seq++;
    uint8_t writeIdx = (_hotHead + _hotCount) % DIAG_JOURNAL_HOT_ENTRIES;
    memcpy(&_hotRing[writeIdx], &ev, sizeof(DiagEvent));
    if (_hotCount < DIAG_JOURNAL_HOT_ENTRIES)
        _hotCount++;
    else
        _hotHead = (_hotHead + 1) % DIAG_JOURNAL_HOT_ENTRIES;

    DIAG_EXIT_CRITICAL();

    if (++_seqSaveCounter >= SEQ_SAVE_INTERVAL) {
        _seqSaveCounter = 0;
        nvs_save_seq();
    }

    appState.markDiagJournalDirty();
    diag_serial_emit(ev);
}

void diag_emit(DiagErrorCode code, DiagSeverity severity, uint8_t slot,
               const char* device, const char* msg) {
    diag_emit_internal(code, severity, slot, device, msg, 0);
}

void diag_emit(DiagErrorCode code, DiagSeverity severity, uint8_t slot,
               const char* device, const char* msg, uint16_t corrId) {
    diag_emit_internal(code, severity, slot, device, msg, corrId);
}

uint8_t diag_journal_count() { return _hotCount; }

bool diag_journal_read(uint8_t index, DiagEvent* out) {
    if (!_hotRing || !out) return false;

    DIAG_ENTER_CRITICAL();
    uint8_t count = _hotCount;
    uint8_t head  = _hotHead;
    if (index >= count) { DIAG_EXIT_CRITICAL(); return false; }

    uint8_t ringIdx = (uint8_t)((head + count - 1u - index) % DIAG_JOURNAL_HOT_ENTRIES);
    memcpy(out, &_hotRing[ringIdx], sizeof(DiagEvent));
    DIAG_EXIT_CRITICAL();
    return true;
}

bool diag_journal_latest(DiagEvent* out) {
    return diag_journal_read(0, out);
}

// ===== Flush to LittleFS =====
// Appends WARN+ entries with per-entry CRC32. Truncates when full.

void diag_journal_flush() {
    if (!_hotRing) return;

    uint8_t count, head;
    DIAG_ENTER_CRITICAL();
    count = _hotCount;
    head  = _hotHead;
    DIAG_EXIT_CRITICAL();
    if (count == 0) return;

    // Read existing header
    JournalHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    bool fileValid = false;

    if (LittleFS.exists(DIAG_JOURNAL_FILE)) {
        File rf = LittleFS.open(DIAG_JOURNAL_FILE, FILE_READ);
        if (rf && rf.size() >= (int)HEADER_SIZE) {
            rf.read(reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr));
            rf.close();
            fileValid = (hdr.magic == JOURNAL_MAGIC && hdr.version == JOURNAL_VERSION);
        }
    }

    if (!fileValid) {
        hdr.magic = JOURNAL_MAGIC;
        hdr.version = JOURNAL_VERSION;
        hdr.entryCount = 0;
        hdr.headIndex = 0;
    }

    if (hdr.entryCount >= (uint32_t)DIAG_JOURNAL_MAX_ENTRIES) {
        LOG_W("[DIAG] Journal full — truncating");
        LittleFS.remove(DIAG_JOURNAL_FILE);
        hdr.entryCount = 0;
        hdr.headIndex = 0;
        fileValid = false;
    }

    // Collect WARN+ entries (oldest first)
    DiagEvent batch[DIAG_JOURNAL_HOT_ENTRIES];
    uint8_t batchCount = 0;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t ringIdx = (head + i) % DIAG_JOURNAL_HOT_ENTRIES;
        if (_hotRing[ringIdx].severity >= (uint8_t)DIAG_SEV_WARN) {
            if (hdr.entryCount + batchCount >= (uint32_t)DIAG_JOURNAL_MAX_ENTRIES) break;
            batch[batchCount++] = _hotRing[ringIdx];
        }
    }
    if (batchCount == 0) return;

    // Write records
    File wf = LittleFS.open(DIAG_JOURNAL_FILE, fileValid ? FILE_APPEND : FILE_WRITE);
    if (!wf) { LOG_E("[DIAG] Cannot open journal"); return; }

    if (!fileValid) {
        wf.write(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr));
    }
    for (uint8_t i = 0; i < batchCount; i++) {
        wf.write(reinterpret_cast<const uint8_t*>(&batch[i]), sizeof(DiagEvent));
        uint32_t crc = crc32_compute(
            reinterpret_cast<const uint8_t*>(&batch[i]), sizeof(DiagEvent));
        wf.write(reinterpret_cast<const uint8_t*>(&crc), sizeof(crc));
        hdr.entryCount++;
    }
    wf.close();

    // Rewrite header: read full file, patch first 16 bytes, write back
    File rf2 = LittleFS.open(DIAG_JOURNAL_FILE, FILE_READ);
    if (rf2) {
        size_t totalSize = rf2.size();
        uint8_t* tmp = static_cast<uint8_t*>(malloc(totalSize));
        if (tmp) {
            rf2.read(tmp, totalSize);
            rf2.close();
            memcpy(tmp, &hdr, sizeof(hdr));
            File wf2 = LittleFS.open(DIAG_JOURNAL_FILE, FILE_WRITE);
            if (wf2) { wf2.write(tmp, totalSize); wf2.close(); }
            free(tmp);
        } else {
            rf2.close();
        }
    }

    nvs_save_seq();
    _seqSaveCounter = 0;
    LOG_I("[DIAG] Flushed %u entries (total: %lu)",
          (unsigned)batchCount, (unsigned long)hdr.entryCount);
}

void diag_journal_clear() {
    DIAG_ENTER_CRITICAL();
    if (_hotRing) memset(_hotRing, 0, DIAG_JOURNAL_HOT_ENTRIES * sizeof(DiagEvent));
    _hotHead = 0;
    _hotCount = 0;
    DIAG_EXIT_CRITICAL();

    LittleFS.remove(DIAG_JOURNAL_FILE);
    LOG_I("[DIAG] Journal cleared");
}

// ===== Test-only reset =====
#ifdef UNIT_TEST
void diag_journal_reset_for_test() {
    if (_hotRing) { free(_hotRing); _hotRing = nullptr; }
    _hotHead = 0;
    _hotCount = 0;
    _seq = 0;
    _bootId = 0;
    _seqSaveCounter = 0;
    _crc32_ready = false;
    MockFS::reset();
    LittleFS.begin();
    Preferences::reset();
}
#endif
