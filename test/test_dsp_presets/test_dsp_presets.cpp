#include <unity.h>
#include <string.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Mock preset storage
#define DSP_PRESET_MAX_SLOTS 32
static bool mock_preset_exists[DSP_PRESET_MAX_SLOTS];
static char mock_preset_names[DSP_PRESET_MAX_SLOTS][21];

// Mock AppState
struct MockAppState {
    char dspPresetNames[DSP_PRESET_MAX_SLOTS][21];
    int8_t dspPresetIndex;
} appState;

void setUp(void) {
    // Reset mock state
    memset(mock_preset_exists, 0, sizeof(mock_preset_exists));
    memset(mock_preset_names, 0, sizeof(mock_preset_names));
    memset(&appState, 0, sizeof(appState));
    appState.dspPresetIndex = -1;
}

void tearDown(void) {}

// ===== Helper Functions (extracted from dsp_preset_save logic) =====

// Simulates the auto-assign logic from dsp_preset_save when slot=-1
int dsp_find_free_slot(void) {
    for (int i = 0; i < DSP_PRESET_MAX_SLOTS; i++) {
        if (!mock_preset_exists[i] || appState.dspPresetNames[i][0] == '\0') {
            return i;
        }
    }
    return -1; // All slots full
}

// ===== Tests =====

void test_preset_auto_assign_empty_list(void) {
    // All slots empty, should return slot 0
    int slot = dsp_find_free_slot();
    TEST_ASSERT_EQUAL_INT(0, slot);
}

void test_preset_auto_assign_first_occupied(void) {
    // Slot 0 occupied, should return slot 1
    mock_preset_exists[0] = true;
    strncpy(appState.dspPresetNames[0], "First", 20);

    int slot = dsp_find_free_slot();
    TEST_ASSERT_EQUAL_INT(1, slot);
}

void test_preset_auto_assign_gaps(void) {
    // Slots 0, 2, 4 occupied, should return slot 1
    mock_preset_exists[0] = true;
    mock_preset_exists[2] = true;
    mock_preset_exists[4] = true;
    strncpy(appState.dspPresetNames[0], "First", 20);
    strncpy(appState.dspPresetNames[2], "Third", 20);
    strncpy(appState.dspPresetNames[4], "Fifth", 20);

    int slot = dsp_find_free_slot();
    TEST_ASSERT_EQUAL_INT(1, slot);
}

void test_preset_auto_assign_all_full(void) {
    // Fill all slots
    for (int i = 0; i < DSP_PRESET_MAX_SLOTS; i++) {
        mock_preset_exists[i] = true;
        snprintf(appState.dspPresetNames[i], 21, "Preset%d", i);
    }

    int slot = dsp_find_free_slot();
    TEST_ASSERT_EQUAL_INT(-1, slot); // Should return -1 when all full
}

void test_preset_auto_assign_deleted_slot(void) {
    // Slots 0-2 occupied, slot 1 deleted (exists=false)
    mock_preset_exists[0] = true;
    mock_preset_exists[1] = false; // Deleted
    mock_preset_exists[2] = true;
    strncpy(appState.dspPresetNames[0], "First", 20);
    appState.dspPresetNames[1][0] = '\0'; // Empty name
    strncpy(appState.dspPresetNames[2], "Third", 20);

    int slot = dsp_find_free_slot();
    TEST_ASSERT_EQUAL_INT(1, slot); // Should reuse deleted slot
}

void test_preset_auto_assign_empty_name_slot(void) {
    // Slot exists but name is empty (edge case)
    mock_preset_exists[0] = true;
    mock_preset_exists[1] = true;
    appState.dspPresetNames[0][0] = '\0'; // Empty name despite exists=true

    int slot = dsp_find_free_slot();
    TEST_ASSERT_EQUAL_INT(0, slot); // Should still return slot 0
}

void test_preset_auto_assign_last_slot(void) {
    // Fill all but last slot
    for (int i = 0; i < DSP_PRESET_MAX_SLOTS - 1; i++) {
        mock_preset_exists[i] = true;
        snprintf(appState.dspPresetNames[i], 21, "Preset%d", i);
    }

    int slot = dsp_find_free_slot();
    TEST_ASSERT_EQUAL_INT(DSP_PRESET_MAX_SLOTS - 1, slot);
}

void test_preset_auto_assign_middle_gap(void) {
    // Fill first half, leave gap in middle, fill second half
    for (int i = 0; i < DSP_PRESET_MAX_SLOTS; i++) {
        if (i != 15) {
            mock_preset_exists[i] = true;
            snprintf(appState.dspPresetNames[i], 21, "Preset%d", i);
        }
    }

    int slot = dsp_find_free_slot();
    TEST_ASSERT_EQUAL_INT(15, slot); // Should find the gap
}

// ===== Main Test Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_preset_auto_assign_empty_list);
    RUN_TEST(test_preset_auto_assign_first_occupied);
    RUN_TEST(test_preset_auto_assign_gaps);
    RUN_TEST(test_preset_auto_assign_all_full);
    RUN_TEST(test_preset_auto_assign_deleted_slot);
    RUN_TEST(test_preset_auto_assign_empty_name_slot);
    RUN_TEST(test_preset_auto_assign_last_slot);
    RUN_TEST(test_preset_auto_assign_middle_gap);

    return UNITY_END();
}
