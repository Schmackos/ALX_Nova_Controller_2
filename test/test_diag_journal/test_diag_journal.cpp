/**
 * test_diag_journal.cpp
 *
 * Comprehensive tests for the diagnostic journal module (src/diag_journal.h/.cpp).
 * Covers: hot ring buffer operations, event field population, sequence counter,
 * LittleFS flush/persistence, clear, dirty flags, and struct packing.
 *
 * Technique: inline-includes diag_journal.cpp directly. The .cpp has its own
 * #ifdef NATIVE_TEST guards that include the same mocks — since we include them
 * first, the guards in the .cpp become no-ops. This is the standard project
 * pattern (see test_hal_bridge, test_sink_slot_api, etc.).
 */

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/LittleFS.h"
#include "../test_mocks/Preferences.h"
#else
#include <Arduino.h>
#endif

// Inline the implementation under test
#include "../../src/diag_journal.cpp"

// ===== Struct packing static asserts =====
// These fire at compile time — if they fail, the build fails.
static_assert(sizeof(DiagEvent) == 64, "DiagEvent must be exactly 64 bytes");
static_assert(sizeof(JournalHeader) == 16, "JournalHeader must be exactly 16 bytes");

// ===== setUp / tearDown =====

void setUp() {
    ArduinoMock::reset();
    diag_journal_reset_for_test();
    diag_journal_init();
}

void tearDown() {}

// ===== Helper =====

static void emit_n(uint8_t n, DiagSeverity sev = DIAG_SEV_INFO) {
    for (uint8_t i = 0; i < n; i++) {
        diag_emit(DIAG_HAL_INIT_FAILED, sev, i, "TestDev", "msg");
    }
}

// =========================================================================
// Ring Buffer Tests
// =========================================================================

void test_single_emit_read_back() {
    // Arrange
    ArduinoMock::mockMillis = 5000;
    diag_journal_set_boot_id(42);

    // Act
    diag_emit(DIAG_HAL_INIT_FAILED, DIAG_SEV_ERROR, 3, "PCM5102A", "init fail");

    // Assert
    TEST_ASSERT_EQUAL_UINT8(1, diag_journal_count());
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT32(0, ev.seq);
    TEST_ASSERT_EQUAL_UINT32(42, ev.bootId);
    TEST_ASSERT_EQUAL_UINT32(5000, ev.timestamp);
    TEST_ASSERT_EQUAL_UINT32(200000, ev.heapFree);  // EspClass mock default
    TEST_ASSERT_EQUAL_UINT16(DIAG_HAL_INIT_FAILED, ev.code);
    TEST_ASSERT_EQUAL_UINT8(DIAG_SEV_ERROR, ev.severity);
    TEST_ASSERT_EQUAL_UINT8(3, ev.slot);
    TEST_ASSERT_EQUAL_UINT16(0, ev.corrId);
    TEST_ASSERT_EQUAL_STRING("PCM5102A", ev.device);
    TEST_ASSERT_EQUAL_STRING("init fail", ev.message);
}

void test_count_increments() {
    // Arrange & Act
    TEST_ASSERT_EQUAL_UINT8(0, diag_journal_count());
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "A", "m1");
    TEST_ASSERT_EQUAL_UINT8(1, diag_journal_count());
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "B", "m2");
    TEST_ASSERT_EQUAL_UINT8(2, diag_journal_count());
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "C", "m3");
    TEST_ASSERT_EQUAL_UINT8(3, diag_journal_count());
}

void test_read_index_ordering() {
    // Arrange — emit 3 events with identifiable device names
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "First", "m");
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "Second", "m");
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "Third", "m");

    // Act & Assert — index 0 = newest
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_STRING("Third", ev.device);

    TEST_ASSERT_TRUE(diag_journal_read(1, &ev));
    TEST_ASSERT_EQUAL_STRING("Second", ev.device);

    TEST_ASSERT_TRUE(diag_journal_read(2, &ev));
    TEST_ASSERT_EQUAL_STRING("First", ev.device);
}

void test_exact_capacity_fill() {
    // Arrange — fill to exactly DIAG_JOURNAL_HOT_ENTRIES (32)
    for (uint8_t i = 0; i < DIAG_JOURNAL_HOT_ENTRIES; i++) {
        char dev[16];
        snprintf(dev, sizeof(dev), "Dev%u", (unsigned)i);
        diag_emit(DIAG_OK, DIAG_SEV_INFO, i, dev, "m");
    }

    // Assert
    TEST_ASSERT_EQUAL_UINT8(DIAG_JOURNAL_HOT_ENTRIES, diag_journal_count());

    // Newest = seq 31, oldest = seq 0
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT32(DIAG_JOURNAL_HOT_ENTRIES - 1, ev.seq);

    TEST_ASSERT_TRUE(diag_journal_read(DIAG_JOURNAL_HOT_ENTRIES - 1, &ev));
    TEST_ASSERT_EQUAL_UINT32(0, ev.seq);
}

void test_wrap_evicts_oldest() {
    // Arrange — write 33 entries (one past capacity)
    for (uint8_t i = 0; i < 33; i++) {
        diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");
    }

    // Assert — count stays at 32, oldest evicted
    TEST_ASSERT_EQUAL_UINT8(DIAG_JOURNAL_HOT_ENTRIES, diag_journal_count());

    DiagEvent ev;
    // Newest = seq 32
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT32(32, ev.seq);

    // Oldest = seq 1 (seq 0 was evicted)
    TEST_ASSERT_TRUE(diag_journal_read(DIAG_JOURNAL_HOT_ENTRIES - 1, &ev));
    TEST_ASSERT_EQUAL_UINT32(1, ev.seq);
}

void test_wrap_two_and_half_times() {
    // Arrange — write 80 entries (2.5x capacity of 32)
    for (uint8_t i = 0; i < 80; i++) {
        diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");
    }

    // Assert
    TEST_ASSERT_EQUAL_UINT8(DIAG_JOURNAL_HOT_ENTRIES, diag_journal_count());

    DiagEvent ev;
    // Newest = seq 79
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT32(79, ev.seq);

    // Oldest = seq 48 (80 - 32)
    TEST_ASSERT_TRUE(diag_journal_read(DIAG_JOURNAL_HOT_ENTRIES - 1, &ev));
    TEST_ASSERT_EQUAL_UINT32(48, ev.seq);
}

void test_read_out_of_bounds() {
    // Arrange — emit 5 entries
    emit_n(5);

    // Act & Assert
    DiagEvent ev;
    TEST_ASSERT_FALSE(diag_journal_read(5, &ev));    // index == count
    TEST_ASSERT_FALSE(diag_journal_read(255, &ev));   // far out of range
    TEST_ASSERT_FALSE(diag_journal_read(100, &ev));
}

void test_empty_buffer_reads() {
    // Arrange — nothing emitted (setUp already called init)

    // Assert
    TEST_ASSERT_EQUAL_UINT8(0, diag_journal_count());

    DiagEvent ev;
    TEST_ASSERT_FALSE(diag_journal_read(0, &ev));
    TEST_ASSERT_FALSE(diag_journal_latest(&ev));
}

// =========================================================================
// Event Field Tests
// =========================================================================

void test_device_name_truncation() {
    // Arrange — 20-char name, field is char[16] (15 usable + null)
    const char* longName = "ThisIsA20CharDevName";

    // Act
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, longName, "m");

    // Assert — truncated to 15 chars + null terminator
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT8(15, strlen(ev.device));
    TEST_ASSERT_EQUAL_STRING("ThisIsA20CharDe", ev.device);
    TEST_ASSERT_EQUAL_UINT8('\0', ev.device[15]);
}

void test_message_truncation() {
    // Arrange — 30-char message, field is char[24] (23 usable + null)
    const char* longMsg = "This message is exactly 30 ch";  // 29 chars
    // Actually, use exactly 30:
    const char* longMsg30 = "AbcDefGhiJklMnoPqrStUvWxYz1234";  // 30 chars

    // Act
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", longMsg30);

    // Assert — truncated to 23 chars + null terminator
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT8(23, strlen(ev.message));
    TEST_ASSERT_EQUAL_STRING("AbcDefGhiJklMnoPqrStUvW", ev.message);
    TEST_ASSERT_EQUAL_UINT8('\0', ev.message[23]);
}

void test_null_device_and_message() {
    // Act — should not crash
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, nullptr, nullptr);

    // Assert
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_STRING("", ev.device);
    TEST_ASSERT_EQUAL_STRING("", ev.message);
}

void test_severity_stored_correctly() {
    // Act — emit one of each severity
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "info");
    diag_emit(DIAG_OK, DIAG_SEV_WARN, 0, "D", "warn");
    diag_emit(DIAG_OK, DIAG_SEV_ERROR, 0, "D", "error");
    diag_emit(DIAG_OK, DIAG_SEV_CRIT, 0, "D", "crit");

    // Assert — index 0=newest(CRIT), 3=oldest(INFO)
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT8(DIAG_SEV_CRIT, ev.severity);

    TEST_ASSERT_TRUE(diag_journal_read(1, &ev));
    TEST_ASSERT_EQUAL_UINT8(DIAG_SEV_ERROR, ev.severity);

    TEST_ASSERT_TRUE(diag_journal_read(2, &ev));
    TEST_ASSERT_EQUAL_UINT8(DIAG_SEV_WARN, ev.severity);

    TEST_ASSERT_TRUE(diag_journal_read(3, &ev));
    TEST_ASSERT_EQUAL_UINT8(DIAG_SEV_INFO, ev.severity);
}

void test_slot_stored_correctly() {
    // Act
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 15, "D", "m");
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0xFF, "D", "m");

    // Assert — index 0 = newest (slot 0xFF)
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT8(0xFF, ev.slot);

    TEST_ASSERT_TRUE(diag_journal_read(1, &ev));
    TEST_ASSERT_EQUAL_UINT8(15, ev.slot);

    TEST_ASSERT_TRUE(diag_journal_read(2, &ev));
    TEST_ASSERT_EQUAL_UINT8(0, ev.slot);
}

void test_correlation_id_stored() {
    // Act — emit without corrId (default 0) and with explicit corrId
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "no-corr");
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "with-corr", (uint16_t)0xBEEF);

    // Assert
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));  // newest = with-corr
    TEST_ASSERT_EQUAL_UINT16(0xBEEF, ev.corrId);

    TEST_ASSERT_TRUE(diag_journal_read(1, &ev));  // oldest = no-corr
    TEST_ASSERT_EQUAL_UINT16(0, ev.corrId);
}

void test_boot_id_propagated() {
    // Arrange
    diag_journal_set_boot_id(9999);

    // Act
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");

    // Assert
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT32(9999, ev.bootId);
}

void test_timestamp_from_millis_mock() {
    // Arrange
    ArduinoMock::mockMillis = 12345;

    // Act
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");

    // Assert
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT32(12345, ev.timestamp);
}

void test_heap_free_from_esp_mock() {
    // The EspClass mock returns 200000 by default for getFreeHeap()
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");

    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT32(200000, ev.heapFree);
}

// =========================================================================
// Sequence Counter Tests
// =========================================================================

void test_seq_starts_at_zero_increments() {
    // Assert — seq should be 0 before any emit
    TEST_ASSERT_EQUAL_UINT32(0, diag_journal_seq());

    // Act
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");
    TEST_ASSERT_EQUAL_UINT32(1, diag_journal_seq());

    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");
    TEST_ASSERT_EQUAL_UINT32(2, diag_journal_seq());

    // Verify event seq values
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT32(1, ev.seq);  // newest
    TEST_ASSERT_TRUE(diag_journal_read(1, &ev));
    TEST_ASSERT_EQUAL_UINT32(0, ev.seq);  // oldest
}

void test_seq_unique_after_100_emits() {
    // Act
    for (int i = 0; i < 100; i++) {
        diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");
    }

    // Assert — check all 32 entries in the ring have unique seq values
    uint32_t seqs[DIAG_JOURNAL_HOT_ENTRIES];
    for (uint8_t i = 0; i < DIAG_JOURNAL_HOT_ENTRIES; i++) {
        DiagEvent ev;
        TEST_ASSERT_TRUE(diag_journal_read(i, &ev));
        seqs[i] = ev.seq;
    }
    // Verify all are unique (consecutive: 99, 98, ..., 68)
    for (uint8_t i = 0; i < DIAG_JOURNAL_HOT_ENTRIES; i++) {
        for (uint8_t j = i + 1; j < DIAG_JOURNAL_HOT_ENTRIES; j++) {
            TEST_ASSERT_NOT_EQUAL(seqs[i], seqs[j]);
        }
    }
}

void test_seq_returns_next_to_assign() {
    // Arrange — emit 5
    emit_n(5);

    // Assert — seq() returns the value the NEXT emit will get
    TEST_ASSERT_EQUAL_UINT32(5, diag_journal_seq());

    // Act — emit one more
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");

    // Assert — now 6
    TEST_ASSERT_EQUAL_UINT32(6, diag_journal_seq());

    // Verify the last event got seq 5
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT32(5, ev.seq);
}

// =========================================================================
// Clear Tests
// =========================================================================

void test_clear_resets_ring() {
    // Arrange
    emit_n(10);
    TEST_ASSERT_EQUAL_UINT8(10, diag_journal_count());

    // Act
    diag_journal_clear();

    // Assert
    TEST_ASSERT_EQUAL_UINT8(0, diag_journal_count());
    DiagEvent ev;
    TEST_ASSERT_FALSE(diag_journal_read(0, &ev));
}

void test_clear_does_not_reset_seq() {
    // Arrange
    emit_n(5);
    uint32_t seqBefore = diag_journal_seq();
    TEST_ASSERT_EQUAL_UINT32(5, seqBefore);

    // Act
    diag_journal_clear();

    // Assert — seq continues from where it was
    TEST_ASSERT_EQUAL_UINT32(5, diag_journal_seq());

    // New emit gets seq 5 (continues, not reset to 0)
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "after-clear");
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT32(5, ev.seq);
    TEST_ASSERT_EQUAL_UINT32(6, diag_journal_seq());
}

// =========================================================================
// Latest Tests
// =========================================================================

void test_latest_returns_newest() {
    // Arrange
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "Old", "m");
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "New", "m");

    // Act
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_latest(&ev));

    // Assert
    TEST_ASSERT_EQUAL_STRING("New", ev.device);
}

void test_latest_empty_returns_false() {
    DiagEvent ev;
    TEST_ASSERT_FALSE(diag_journal_latest(&ev));
}

// =========================================================================
// Flush Tests (LittleFS persistence)
// =========================================================================

void test_flush_info_only_no_file_created() {
    // Arrange — emit only INFO severity entries
    emit_n(5, DIAG_SEV_INFO);

    // Act
    diag_journal_flush();

    // Assert — no file should be created (INFO is below WARN threshold)
    TEST_ASSERT_FALSE(LittleFS.exists(DIAG_JOURNAL_FILE));
}

void test_flush_warn_creates_file_with_header_and_records() {
    // Arrange
    diag_emit(DIAG_HAL_INIT_FAILED, DIAG_SEV_WARN, 1, "ES8311", "init retry");
    diag_emit(DIAG_HAL_HEALTH_FAIL, DIAG_SEV_ERROR, 2, "PCM5102A", "health bad");

    // Act
    diag_journal_flush();

    // Assert — file exists and has correct structure
    TEST_ASSERT_TRUE(LittleFS.exists(DIAG_JOURNAL_FILE));

    std::string raw = MockFS::getFile(DIAG_JOURNAL_FILE);
    // Expected size: 16-byte header + 2 * (64-byte event + 4-byte CRC32) = 16 + 136 = 152
    size_t expectedSize = sizeof(JournalHeader) + 2 * (sizeof(DiagEvent) + sizeof(uint32_t));
    TEST_ASSERT_EQUAL(expectedSize, raw.size());

    // Verify header
    JournalHeader hdr;
    memcpy(&hdr, raw.data(), sizeof(hdr));
    TEST_ASSERT_EQUAL_UINT32(0x44494147UL, hdr.magic);  // "DIAG" LE
    TEST_ASSERT_EQUAL_UINT8(1, hdr.version);
    TEST_ASSERT_EQUAL_UINT32(2, hdr.entryCount);

    // Verify first record (oldest WARN+ entry = seq 0, ES8311)
    DiagEvent persisted;
    memcpy(&persisted, raw.data() + sizeof(JournalHeader), sizeof(DiagEvent));
    TEST_ASSERT_EQUAL_STRING("ES8311", persisted.device);
    TEST_ASSERT_EQUAL_UINT8(DIAG_SEV_WARN, persisted.severity);

    // Verify CRC32 of first record
    uint32_t storedCrc;
    memcpy(&storedCrc, raw.data() + sizeof(JournalHeader) + sizeof(DiagEvent), sizeof(uint32_t));
    uint32_t computedCrc = crc32_compute(
        reinterpret_cast<const uint8_t*>(&persisted), sizeof(DiagEvent));
    TEST_ASSERT_EQUAL_UINT32(computedCrc, storedCrc);
}

void test_flush_mixed_severity_only_warn_plus_persisted() {
    // Arrange — mix of INFO and WARN+ entries
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "InfoDev", "info msg");
    diag_emit(DIAG_HAL_INIT_FAILED, DIAG_SEV_WARN, 1, "WarnDev", "warn msg");
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "InfoDev2", "info msg2");
    diag_emit(DIAG_HAL_HEALTH_FAIL, DIAG_SEV_ERROR, 2, "ErrDev", "err msg");
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "InfoDev3", "info msg3");

    // Act
    diag_journal_flush();

    // Assert — only 2 entries persisted (WARN + ERROR, not the 3 INFO entries)
    TEST_ASSERT_TRUE(LittleFS.exists(DIAG_JOURNAL_FILE));

    std::string raw = MockFS::getFile(DIAG_JOURNAL_FILE);
    JournalHeader hdr;
    memcpy(&hdr, raw.data(), sizeof(hdr));
    TEST_ASSERT_EQUAL_UINT32(2, hdr.entryCount);

    // Verify the two persisted events are WarnDev and ErrDev (oldest first)
    size_t recordSize = sizeof(DiagEvent) + sizeof(uint32_t);
    DiagEvent ev1, ev2;
    memcpy(&ev1, raw.data() + sizeof(JournalHeader), sizeof(DiagEvent));
    memcpy(&ev2, raw.data() + sizeof(JournalHeader) + recordSize, sizeof(DiagEvent));
    TEST_ASSERT_EQUAL_STRING("WarnDev", ev1.device);
    TEST_ASSERT_EQUAL_STRING("ErrDev", ev2.device);
}

void test_clear_removes_persistent_file() {
    // Arrange — flush to create a file
    diag_emit(DIAG_HAL_INIT_FAILED, DIAG_SEV_WARN, 0, "D", "m");
    diag_journal_flush();
    TEST_ASSERT_TRUE(LittleFS.exists(DIAG_JOURNAL_FILE));

    // Act
    diag_journal_clear();

    // Assert — file removed
    TEST_ASSERT_FALSE(LittleFS.exists(DIAG_JOURNAL_FILE));
}

// =========================================================================
// Dirty Flag Tests
// =========================================================================

void test_emit_sets_dirty_flag() {
    // Arrange — clear any residual state
    appState.clearDiagJournalDirty();
    TEST_ASSERT_FALSE(appState.isDiagJournalDirty());

    // Act
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");

    // Assert
    TEST_ASSERT_TRUE(appState.isDiagJournalDirty());
}

void test_clear_dirty_flag() {
    // Arrange
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");
    TEST_ASSERT_TRUE(appState.isDiagJournalDirty());

    // Act
    appState.clearDiagJournalDirty();

    // Assert
    TEST_ASSERT_FALSE(appState.isDiagJournalDirty());
}

// =========================================================================
// Error Code Tests
// =========================================================================

void test_error_code_stored_correctly() {
    // Act — emit events with different error codes
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "ok");
    diag_emit(DIAG_HAL_INIT_FAILED, DIAG_SEV_ERROR, 0, "D", "hal");
    diag_emit(DIAG_AUDIO_I2S_READ_ERROR, DIAG_SEV_ERROR, 0, "D", "audio");

    // Assert
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT16(DIAG_AUDIO_I2S_READ_ERROR, ev.code);

    TEST_ASSERT_TRUE(diag_journal_read(1, &ev));
    TEST_ASSERT_EQUAL_UINT16(DIAG_HAL_INIT_FAILED, ev.code);

    TEST_ASSERT_TRUE(diag_journal_read(2, &ev));
    TEST_ASSERT_EQUAL_UINT16(DIAG_OK, ev.code);
}

// =========================================================================
// Multiple Flush / Append Tests
// =========================================================================

void test_flush_appends_to_existing_file() {
    // Arrange — first flush with one WARN entry
    diag_emit(DIAG_HAL_INIT_FAILED, DIAG_SEV_WARN, 0, "Dev1", "first");
    diag_journal_flush();

    // Verify file has 1 entry after first flush
    std::string raw1 = MockFS::getFile(DIAG_JOURNAL_FILE);
    JournalHeader hdr1;
    memcpy(&hdr1, raw1.data(), sizeof(hdr1));
    TEST_ASSERT_EQUAL_UINT32(1, hdr1.entryCount);

    // Act — emit another WARN entry (ring now has 2 WARN entries total)
    // and flush again. The second flush re-reads the full ring, so it sees
    // both the original and the new entry. Both are appended to the file.
    diag_emit(DIAG_HAL_HEALTH_FAIL, DIAG_SEV_ERROR, 1, "Dev2", "second");
    diag_journal_flush();

    // Assert — file now has 1 (first flush) + 2 (second flush re-reads ring) = 3 entries
    std::string raw2 = MockFS::getFile(DIAG_JOURNAL_FILE);
    JournalHeader hdr2;
    memcpy(&hdr2, raw2.data(), sizeof(hdr2));
    TEST_ASSERT_EQUAL_UINT32(3, hdr2.entryCount);

    size_t recordSize = sizeof(DiagEvent) + sizeof(uint32_t);
    size_t expectedSize = sizeof(JournalHeader) + 3 * recordSize;
    TEST_ASSERT_EQUAL(expectedSize, raw2.size());

    // Verify the appended records are readable (third entry = Dev2)
    DiagEvent ev3;
    memcpy(&ev3, raw2.data() + sizeof(JournalHeader) + 2 * recordSize, sizeof(DiagEvent));
    TEST_ASSERT_EQUAL_STRING("Dev2", ev3.device);
}

void test_flush_crit_severity_persisted() {
    // Arrange — CRIT is above WARN threshold
    diag_emit(DIAG_SYS_BOOT_LOOP, DIAG_SEV_CRIT, 0xFF, "System", "crash loop");

    // Act
    diag_journal_flush();

    // Assert
    TEST_ASSERT_TRUE(LittleFS.exists(DIAG_JOURNAL_FILE));
    std::string raw = MockFS::getFile(DIAG_JOURNAL_FILE);
    JournalHeader hdr;
    memcpy(&hdr, raw.data(), sizeof(hdr));
    TEST_ASSERT_EQUAL_UINT32(1, hdr.entryCount);

    DiagEvent ev;
    memcpy(&ev, raw.data() + sizeof(JournalHeader), sizeof(DiagEvent));
    TEST_ASSERT_EQUAL_UINT8(DIAG_SEV_CRIT, ev.severity);
    TEST_ASSERT_EQUAL_STRING("System", ev.device);
}

// =========================================================================
// Read with null output pointer
// =========================================================================

void test_read_null_out_returns_false() {
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "m");
    TEST_ASSERT_FALSE(diag_journal_read(0, nullptr));
}

// =========================================================================
// Correlation ID overload
// =========================================================================

void test_correlation_id_overload() {
    // Arrange — use the 6-argument overload
    diag_emit(DIAG_HAL_REINIT_OK, DIAG_SEV_INFO, 5, "PCM1808", "retry ok", (uint16_t)42);

    // Assert
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT16(42, ev.corrId);
    TEST_ASSERT_EQUAL_UINT16(DIAG_HAL_REINIT_OK, ev.code);
    TEST_ASSERT_EQUAL_UINT8(5, ev.slot);
}

// =========================================================================
// Boot ID changes between emits
// =========================================================================

void test_boot_id_changes_between_emits() {
    // Arrange
    diag_journal_set_boot_id(100);
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "boot100");

    diag_journal_set_boot_id(200);
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "boot200");

    // Assert
    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));  // newest
    TEST_ASSERT_EQUAL_UINT32(200, ev.bootId);

    TEST_ASSERT_TRUE(diag_journal_read(1, &ev));  // oldest
    TEST_ASSERT_EQUAL_UINT32(100, ev.bootId);
}

// =========================================================================
// Timestamp varies with millis mock
// =========================================================================

void test_timestamp_varies_across_emits() {
    ArduinoMock::mockMillis = 1000;
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "t1");

    ArduinoMock::mockMillis = 2000;
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "t2");

    ArduinoMock::mockMillis = 3000;
    diag_emit(DIAG_OK, DIAG_SEV_INFO, 0, "D", "t3");

    DiagEvent ev;
    TEST_ASSERT_TRUE(diag_journal_read(0, &ev));
    TEST_ASSERT_EQUAL_UINT32(3000, ev.timestamp);

    TEST_ASSERT_TRUE(diag_journal_read(1, &ev));
    TEST_ASSERT_EQUAL_UINT32(2000, ev.timestamp);

    TEST_ASSERT_TRUE(diag_journal_read(2, &ev));
    TEST_ASSERT_EQUAL_UINT32(1000, ev.timestamp);
}

// =========================================================================
// Flush with empty ring is no-op
// =========================================================================

void test_flush_empty_ring_no_file() {
    // Act — flush with no events
    diag_journal_flush();

    // Assert
    TEST_ASSERT_FALSE(LittleFS.exists(DIAG_JOURNAL_FILE));
}

// =========================================================================
// main
// =========================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Struct packing (compile-time via static_assert, runtime sanity)
    RUN_TEST(test_single_emit_read_back);

    // Ring buffer
    RUN_TEST(test_count_increments);
    RUN_TEST(test_read_index_ordering);
    RUN_TEST(test_exact_capacity_fill);
    RUN_TEST(test_wrap_evicts_oldest);
    RUN_TEST(test_wrap_two_and_half_times);
    RUN_TEST(test_read_out_of_bounds);
    RUN_TEST(test_empty_buffer_reads);

    // Event fields
    RUN_TEST(test_device_name_truncation);
    RUN_TEST(test_message_truncation);
    RUN_TEST(test_null_device_and_message);
    RUN_TEST(test_severity_stored_correctly);
    RUN_TEST(test_slot_stored_correctly);
    RUN_TEST(test_correlation_id_stored);
    RUN_TEST(test_boot_id_propagated);
    RUN_TEST(test_timestamp_from_millis_mock);
    RUN_TEST(test_heap_free_from_esp_mock);
    RUN_TEST(test_error_code_stored_correctly);

    // Sequence counter
    RUN_TEST(test_seq_starts_at_zero_increments);
    RUN_TEST(test_seq_unique_after_100_emits);
    RUN_TEST(test_seq_returns_next_to_assign);

    // Clear
    RUN_TEST(test_clear_resets_ring);
    RUN_TEST(test_clear_does_not_reset_seq);

    // Latest
    RUN_TEST(test_latest_returns_newest);
    RUN_TEST(test_latest_empty_returns_false);

    // Flush / persistence
    RUN_TEST(test_flush_info_only_no_file_created);
    RUN_TEST(test_flush_warn_creates_file_with_header_and_records);
    RUN_TEST(test_flush_mixed_severity_only_warn_plus_persisted);
    RUN_TEST(test_flush_crit_severity_persisted);
    RUN_TEST(test_flush_appends_to_existing_file);
    RUN_TEST(test_flush_empty_ring_no_file);
    RUN_TEST(test_clear_removes_persistent_file);

    // Dirty flags
    RUN_TEST(test_emit_sets_dirty_flag);
    RUN_TEST(test_clear_dirty_flag);

    // Edge cases
    RUN_TEST(test_read_null_out_returns_false);
    RUN_TEST(test_correlation_id_overload);
    RUN_TEST(test_boot_id_changes_between_emits);
    RUN_TEST(test_timestamp_varies_across_emits);

    return UNITY_END();
}
