#include <cmath>
#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline re-implementations for native testing =====
// Tests don't compile src/ directly (test_build_src = no)

// --- Volume curve ---
static float test_dac_volume_to_linear(uint8_t percent) {
    if (percent == 0) return 0.0f;
    if (percent >= 100) return 1.0f;
    float exponent = (float)percent / 50.0f;
    float x = exponent * 2.302585f; // ln(10)
    float term = 1.0f;
    float power = 1.0f;
    for (int i = 1; i <= 12; i++) {
        term *= x / (float)i;
        power += term;
    }
    return (power - 1.0f) / 99.0f;
}

// --- Software volume ---
static void test_dac_apply_software_volume(float* buffer, int samples, float gain) {
    if (!buffer || samples <= 0) return;
    if (gain == 1.0f) return;
    for (int i = 0; i < samples; i++) {
        buffer[i] *= gain;
    }
}

// --- DacCapabilities ---
struct TestDacCapabilities {
    const char* name;
    const char* manufacturer;
    uint16_t deviceId;
    uint8_t maxChannels;
    bool hasHardwareVolume;
    bool hasI2cControl;
    bool needsIndependentClock;
    uint8_t i2cAddress;
    const uint32_t* supportedRates;
    uint8_t numSupportedRates;
    bool hasFilterModes;
    uint8_t numFilterModes;
};

// --- PCM5102A capabilities (copied from driver) ---
static const uint32_t PCM5102_RATES[] = {
    8000, 16000, 32000, 44100, 48000, 88200, 96000, 176400, 192000
};
static const uint8_t PCM5102_NUM_RATES = sizeof(PCM5102_RATES) / sizeof(PCM5102_RATES[0]);

static const TestDacCapabilities PCM5102_CAPS = {
    "PCM5102A", "Texas Instruments", 0x0001, 2,
    false, false, false, 0x00,
    PCM5102_RATES, PCM5102_NUM_RATES, false, 0
};

// --- Simulated PCM5102A driver ---
static bool pcm5102_initialized = false;
static bool pcm5102_configured = false;
static uint32_t pcm5102_sampleRate = 0;
static uint8_t pcm5102_bitDepth = 0;

static bool pcm5102_init() { pcm5102_initialized = true; return true; }
static void pcm5102_deinit() { pcm5102_initialized = false; pcm5102_configured = false; pcm5102_sampleRate = 0; pcm5102_bitDepth = 0; }
static bool pcm5102_configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!pcm5102_initialized) return false;
    bool valid = false;
    for (uint8_t i = 0; i < PCM5102_NUM_RATES; i++) {
        if (PCM5102_RATES[i] == sampleRate) { valid = true; break; }
    }
    if (!valid) return false;
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) return false;
    pcm5102_sampleRate = sampleRate;
    pcm5102_bitDepth = bitDepth;
    pcm5102_configured = true;
    return true;
}
static bool pcm5102_setVolume(uint8_t volume) { (void)volume; return true; }
static bool pcm5102_setMute(bool mute) { (void)mute; return true; }
static bool pcm5102_isReady() { return pcm5102_initialized && pcm5102_configured; }

// --- Registry simulation ---
#define DAC_ID_PCM5102A   0x0001
#define DAC_ID_ES9038Q2M  0x0002
#define DAC_ID_UNKNOWN    0xFFFF

struct TestRegistryEntry {
    uint16_t deviceId;
    const char* name;
};

static const TestRegistryEntry TEST_REGISTRY[] = {
    { DAC_ID_PCM5102A, "PCM5102A" },
};
static const int TEST_REGISTRY_COUNT = 1;

static const TestRegistryEntry* test_find_by_id(uint16_t id) {
    for (int i = 0; i < TEST_REGISTRY_COUNT; i++) {
        if (TEST_REGISTRY[i].deviceId == id) return &TEST_REGISTRY[i];
    }
    return nullptr;
}

static const TestRegistryEntry* test_find_by_name(const char* name) {
    if (!name) return nullptr;
    for (int i = 0; i < TEST_REGISTRY_COUNT; i++) {
        if (strcmp(TEST_REGISTRY[i].name, name) == 0) return &TEST_REGISTRY[i];
    }
    return nullptr;
}

// ===== Test State Reset =====
void setUp(void) {
    pcm5102_initialized = false;
    pcm5102_configured = false;
    pcm5102_sampleRate = 0;
    pcm5102_bitDepth = 0;
}

void tearDown(void) {}

// ===== Registry Tests =====

void test_registry_find_pcm5102(void) {
    const TestRegistryEntry* entry = test_find_by_id(DAC_ID_PCM5102A);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT16(DAC_ID_PCM5102A, entry->deviceId);
    TEST_ASSERT_EQUAL_STRING("PCM5102A", entry->name);
}

void test_registry_unknown_returns_null(void) {
    const TestRegistryEntry* entry = test_find_by_id(DAC_ID_UNKNOWN);
    TEST_ASSERT_NULL(entry);
}

void test_registry_find_by_name(void) {
    const TestRegistryEntry* entry = test_find_by_name("PCM5102A");
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT16(DAC_ID_PCM5102A, entry->deviceId);
}

void test_registry_find_by_name_null(void) {
    const TestRegistryEntry* entry = test_find_by_name(nullptr);
    TEST_ASSERT_NULL(entry);
}

void test_registry_find_by_name_unknown(void) {
    const TestRegistryEntry* entry = test_find_by_name("ES9999");
    TEST_ASSERT_NULL(entry);
}

// ===== PCM5102A Driver Tests =====

void test_pcm5102_capabilities(void) {
    TEST_ASSERT_EQUAL_STRING("PCM5102A", PCM5102_CAPS.name);
    TEST_ASSERT_EQUAL_STRING("Texas Instruments", PCM5102_CAPS.manufacturer);
    TEST_ASSERT_EQUAL_UINT8(2, PCM5102_CAPS.maxChannels);
    TEST_ASSERT_FALSE(PCM5102_CAPS.hasHardwareVolume);
    TEST_ASSERT_FALSE(PCM5102_CAPS.hasI2cControl);
    TEST_ASSERT_FALSE(PCM5102_CAPS.needsIndependentClock);
    TEST_ASSERT_EQUAL_UINT8(0, PCM5102_CAPS.i2cAddress);
    TEST_ASSERT_FALSE(PCM5102_CAPS.hasFilterModes);
    TEST_ASSERT_EQUAL_UINT8(9, PCM5102_CAPS.numSupportedRates);
}

void test_pcm5102_init_deinit(void) {
    TEST_ASSERT_TRUE(pcm5102_init());
    TEST_ASSERT_TRUE(pcm5102_initialized);
    pcm5102_deinit();
    TEST_ASSERT_FALSE(pcm5102_initialized);
}

void test_pcm5102_configure_valid_rate(void) {
    pcm5102_init();
    TEST_ASSERT_TRUE(pcm5102_configure(48000, 32));
    TEST_ASSERT_TRUE(pcm5102_isReady());
}

void test_pcm5102_configure_44100(void) {
    pcm5102_init();
    TEST_ASSERT_TRUE(pcm5102_configure(44100, 24));
    TEST_ASSERT_TRUE(pcm5102_isReady());
}

void test_pcm5102_configure_invalid_rate(void) {
    pcm5102_init();
    TEST_ASSERT_FALSE(pcm5102_configure(22050, 32));
    TEST_ASSERT_FALSE(pcm5102_isReady());
}

void test_pcm5102_configure_invalid_bitdepth(void) {
    pcm5102_init();
    TEST_ASSERT_FALSE(pcm5102_configure(48000, 8));
    TEST_ASSERT_FALSE(pcm5102_isReady());
}

void test_pcm5102_configure_without_init(void) {
    TEST_ASSERT_FALSE(pcm5102_configure(48000, 32));
}

void test_pcm5102_volume_noop(void) {
    pcm5102_init();
    TEST_ASSERT_TRUE(pcm5102_setVolume(50));
    TEST_ASSERT_TRUE(pcm5102_setVolume(0));
    TEST_ASSERT_TRUE(pcm5102_setVolume(100));
}

void test_pcm5102_mute_noop(void) {
    pcm5102_init();
    TEST_ASSERT_TRUE(pcm5102_setMute(true));
    TEST_ASSERT_TRUE(pcm5102_setMute(false));
}

void test_pcm5102_not_ready_before_configure(void) {
    pcm5102_init();
    TEST_ASSERT_FALSE(pcm5102_isReady());
}

// ===== Volume Curve Tests =====

void test_volume_zero_is_zero(void) {
    float gain = test_dac_volume_to_linear(0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, gain);
}

void test_volume_full_is_one(void) {
    float gain = test_dac_volume_to_linear(100);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, gain);
}

void test_volume_monotonic_increase(void) {
    float prev = 0.0f;
    for (uint8_t p = 1; p <= 100; p++) {
        float g = test_dac_volume_to_linear(p);
        TEST_ASSERT_TRUE_MESSAGE(g > prev, "Volume curve must be monotonically increasing");
        prev = g;
    }
}

void test_volume_midpoint_reasonable(void) {
    float mid = test_dac_volume_to_linear(50);
    // Log curve: 50% should be well below 0.5 linear (~0.056)
    TEST_ASSERT_TRUE(mid > 0.01f);
    TEST_ASSERT_TRUE(mid < 0.2f);
}

void test_volume_above_100_clamps(void) {
    float gain = test_dac_volume_to_linear(255);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, gain);
}

// ===== Software Volume Tests =====

void test_sw_volume_unity_unchanged(void) {
    float buf[] = {0.5f, -0.3f, 0.8f, -1.0f};
    float expected[] = {0.5f, -0.3f, 0.8f, -1.0f};
    test_dac_apply_software_volume(buf, 4, 1.0f);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected[i], buf[i]);
    }
}

void test_sw_volume_half_gain(void) {
    float buf[] = {1.0f, -0.5f, 0.0f, 0.25f};
    test_dac_apply_software_volume(buf, 4, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, buf[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.25f, buf[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, buf[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.125f, buf[3]);
}

void test_sw_volume_zero_silence(void) {
    float buf[] = {1.0f, -1.0f, 0.5f, -0.5f};
    test_dac_apply_software_volume(buf, 4, 0.0f);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, buf[i]);
    }
}

void test_sw_volume_null_buffer_safe(void) {
    // Should not crash
    test_dac_apply_software_volume(nullptr, 4, 0.5f);
}

void test_sw_volume_zero_samples_safe(void) {
    float buf[] = {1.0f};
    test_dac_apply_software_volume(buf, 0, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, buf[0]); // Unchanged
}

// ===== Main =====
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    // Registry tests
    RUN_TEST(test_registry_find_pcm5102);
    RUN_TEST(test_registry_unknown_returns_null);
    RUN_TEST(test_registry_find_by_name);
    RUN_TEST(test_registry_find_by_name_null);
    RUN_TEST(test_registry_find_by_name_unknown);

    // PCM5102A driver tests
    RUN_TEST(test_pcm5102_capabilities);
    RUN_TEST(test_pcm5102_init_deinit);
    RUN_TEST(test_pcm5102_configure_valid_rate);
    RUN_TEST(test_pcm5102_configure_44100);
    RUN_TEST(test_pcm5102_configure_invalid_rate);
    RUN_TEST(test_pcm5102_configure_invalid_bitdepth);
    RUN_TEST(test_pcm5102_configure_without_init);
    RUN_TEST(test_pcm5102_volume_noop);
    RUN_TEST(test_pcm5102_mute_noop);
    RUN_TEST(test_pcm5102_not_ready_before_configure);

    // Volume curve tests
    RUN_TEST(test_volume_zero_is_zero);
    RUN_TEST(test_volume_full_is_one);
    RUN_TEST(test_volume_monotonic_increase);
    RUN_TEST(test_volume_midpoint_reasonable);
    RUN_TEST(test_volume_above_100_clamps);

    // Software volume tests
    RUN_TEST(test_sw_volume_unity_unchanged);
    RUN_TEST(test_sw_volume_half_gain);
    RUN_TEST(test_sw_volume_zero_silence);
    RUN_TEST(test_sw_volume_null_buffer_safe);
    RUN_TEST(test_sw_volume_zero_samples_safe);

    return UNITY_END();
}
