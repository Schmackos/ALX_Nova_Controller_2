#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline I/O Registry implementation for native testing =====
// Tests don't compile src/ directly (test_build_src = no)

#define IO_MAX_OUTPUTS 4
#define IO_MAX_INPUTS  4

enum IoDiscovery : uint8_t {
    IO_DISC_BUILTIN = 0,
    IO_DISC_EEPROM  = 1,
    IO_DISC_MANUAL  = 2
};

struct IoOutputEntry {
    bool     active;
    uint8_t  index;
    uint16_t deviceId;
    char     name[33];
    uint8_t  deviceType;
    IoDiscovery discovery;
    uint8_t  i2sPort;
    uint8_t  channelCount;
    uint8_t  firstOutputChannel;
    bool     ready;
};

struct IoInputEntry {
    bool     active;
    uint8_t  index;
    char     name[33];
    IoDiscovery discovery;
    uint8_t  i2sPort;
    uint8_t  channelCount;
    uint8_t  firstInputChannel;
};

// Simulated registry state
static IoOutputEntry _outputs[IO_MAX_OUTPUTS];
static IoInputEntry  _inputs[IO_MAX_INPUTS];

static int find_free_output_slot() {
    for (int i = 0; i < IO_MAX_OUTPUTS; i++) {
        if (!_outputs[i].active) return i;
    }
    return -1;
}

static void register_builtin_output(uint8_t slot, uint16_t deviceId, const char* name,
                                     uint8_t deviceType, uint8_t i2sPort, uint8_t channelCount) {
    if (slot >= IO_MAX_OUTPUTS) return;
    IoOutputEntry& e = _outputs[slot];
    e.active = true;
    e.index = slot;
    e.deviceId = deviceId;
    strncpy(e.name, name, 32);
    e.name[32] = '\0';
    e.deviceType = deviceType;
    e.discovery = IO_DISC_BUILTIN;
    e.i2sPort = i2sPort;
    e.channelCount = channelCount;
    e.firstOutputChannel = slot * 2;
    e.ready = false;
}

static void register_builtin_input(uint8_t slot, const char* name,
                                    uint8_t i2sPort, uint8_t channelCount) {
    if (slot >= IO_MAX_INPUTS) return;
    IoInputEntry& e = _inputs[slot];
    e.active = true;
    e.index = slot;
    strncpy(e.name, name, 32);
    e.name[32] = '\0';
    e.discovery = IO_DISC_BUILTIN;
    e.i2sPort = i2sPort;
    e.channelCount = channelCount;
    e.firstInputChannel = slot * 2;
}

static int io_registry_output_count() {
    int count = 0;
    for (int i = 0; i < IO_MAX_OUTPUTS; i++) {
        if (_outputs[i].active) count++;
    }
    return count;
}

static int io_registry_input_count() {
    int count = 0;
    for (int i = 0; i < IO_MAX_INPUTS; i++) {
        if (_inputs[i].active) count++;
    }
    return count;
}

static int io_registry_add_output(const char* name, uint8_t i2sPort, uint16_t deviceId, uint8_t channelCount) {
    int slot = find_free_output_slot();
    if (slot < 0) return -1;
    IoOutputEntry& e = _outputs[slot];
    e.active = true;
    e.index = (uint8_t)slot;
    e.deviceId = deviceId;
    strncpy(e.name, name ? name : "Manual", 32);
    e.name[32] = '\0';
    e.deviceType = 0;
    e.discovery = IO_DISC_MANUAL;
    e.i2sPort = i2sPort;
    e.channelCount = channelCount > 0 ? channelCount : 2;
    e.firstOutputChannel = (uint8_t)(slot * 2);
    e.ready = false;
    return slot;
}

static bool io_registry_remove_output(uint8_t index) {
    if (index >= IO_MAX_OUTPUTS) return false;
    if (!_outputs[index].active) return false;
    if (_outputs[index].discovery != IO_DISC_MANUAL) return false;
    memset(&_outputs[index], 0, sizeof(IoOutputEntry));
    return true;
}

// ===== Test Setup =====

void setUp(void) {
    memset(_outputs, 0, sizeof(_outputs));
    memset(_inputs, 0, sizeof(_inputs));
    // Register default builtins (mirroring io_registry_init)
    register_builtin_output(0, 0x0001, "PCM5102A", 0, 0, 2);
    register_builtin_output(1, 0x0004, "ES8311", 2, 2, 2);
    register_builtin_input(0, "ADC1 (PCM1808)", 0, 2);
    register_builtin_input(1, "ADC2 (PCM1808)", 1, 2);
}

void tearDown(void) {}

// ===== Channel Assignment Tests =====

void test_registry_channel_assignment_slot0(void) {
    TEST_ASSERT_EQUAL_UINT8(0, _outputs[0].firstOutputChannel);
}

void test_registry_channel_assignment_slot1(void) {
    TEST_ASSERT_EQUAL_UINT8(2, _outputs[1].firstOutputChannel);
}

void test_registry_channel_assignment_slot2(void) {
    io_registry_add_output("Test", 0, 0x0000, 2);
    TEST_ASSERT_EQUAL_UINT8(4, _outputs[2].firstOutputChannel);
}

void test_registry_channel_assignment_slot3(void) {
    io_registry_add_output("Test2", 0, 0x0000, 2);
    io_registry_add_output("Test3", 0, 0x0000, 2);
    TEST_ASSERT_EQUAL_UINT8(6, _outputs[3].firstOutputChannel);
}

// ===== Add/Remove Tests =====

void test_registry_add_output_first_free(void) {
    int slot = io_registry_add_output("Manual DAC", 1, 0x9999, 2);
    TEST_ASSERT_EQUAL(2, slot);
    TEST_ASSERT_TRUE(_outputs[2].active);
    TEST_ASSERT_EQUAL_STRING("Manual DAC", _outputs[2].name);
    TEST_ASSERT_EQUAL(IO_DISC_MANUAL, _outputs[2].discovery);
}

void test_registry_add_output_max_full(void) {
    io_registry_add_output("Fill2", 0, 0, 2);
    io_registry_add_output("Fill3", 0, 0, 2);
    int slot = io_registry_add_output("Overflow", 0, 0, 2);
    TEST_ASSERT_EQUAL(-1, slot);
}

void test_registry_remove_output(void) {
    int slot = io_registry_add_output("Removable", 0, 0, 2);
    TEST_ASSERT_EQUAL(2, slot);
    TEST_ASSERT_TRUE(io_registry_remove_output(2));
    TEST_ASSERT_FALSE(_outputs[2].active);
}

void test_registry_remove_builtin_rejected(void) {
    TEST_ASSERT_FALSE(io_registry_remove_output(0));
    TEST_ASSERT_TRUE(_outputs[0].active);
}

void test_registry_remove_builtin_es8311_rejected(void) {
    TEST_ASSERT_FALSE(io_registry_remove_output(1));
    TEST_ASSERT_TRUE(_outputs[1].active);
}

// ===== Count Tests =====

void test_registry_output_count(void) {
    TEST_ASSERT_EQUAL(2, io_registry_output_count());
    io_registry_add_output("Extra", 0, 0, 2);
    TEST_ASSERT_EQUAL(3, io_registry_output_count());
}

void test_registry_input_count(void) {
    TEST_ASSERT_EQUAL(2, io_registry_input_count());
}

// ===== Input Channel Assignment =====

void test_registry_input_channels(void) {
    TEST_ASSERT_EQUAL_UINT8(0, _inputs[0].firstInputChannel);
    TEST_ASSERT_EQUAL_UINT8(2, _inputs[1].firstInputChannel);
}

// ===== Builtin State =====

void test_registry_builtin_output_names(void) {
    TEST_ASSERT_EQUAL_STRING("PCM5102A", _outputs[0].name);
    TEST_ASSERT_EQUAL_STRING("ES8311", _outputs[1].name);
}

void test_registry_builtin_input_names(void) {
    TEST_ASSERT_EQUAL_STRING("ADC1 (PCM1808)", _inputs[0].name);
    TEST_ASSERT_EQUAL_STRING("ADC2 (PCM1808)", _inputs[1].name);
}

void test_registry_builtin_discovery_type(void) {
    TEST_ASSERT_EQUAL(IO_DISC_BUILTIN, _outputs[0].discovery);
    TEST_ASSERT_EQUAL(IO_DISC_BUILTIN, _outputs[1].discovery);
    TEST_ASSERT_EQUAL(IO_DISC_BUILTIN, _inputs[0].discovery);
    TEST_ASSERT_EQUAL(IO_DISC_BUILTIN, _inputs[1].discovery);
}

// ===== Main =====
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    // Channel assignment
    RUN_TEST(test_registry_channel_assignment_slot0);
    RUN_TEST(test_registry_channel_assignment_slot1);
    RUN_TEST(test_registry_channel_assignment_slot2);
    RUN_TEST(test_registry_channel_assignment_slot3);

    // Add/remove
    RUN_TEST(test_registry_add_output_first_free);
    RUN_TEST(test_registry_add_output_max_full);
    RUN_TEST(test_registry_remove_output);
    RUN_TEST(test_registry_remove_builtin_rejected);
    RUN_TEST(test_registry_remove_builtin_es8311_rejected);

    // Counts
    RUN_TEST(test_registry_output_count);
    RUN_TEST(test_registry_input_count);

    // Inputs
    RUN_TEST(test_registry_input_channels);

    // Builtins
    RUN_TEST(test_registry_builtin_output_names);
    RUN_TEST(test_registry_builtin_input_names);
    RUN_TEST(test_registry_builtin_discovery_type);

    return UNITY_END();
}
