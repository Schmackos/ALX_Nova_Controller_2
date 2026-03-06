// test_hal_pcm5102a.cpp
// Tests a mock HalPcm5102a class modelling the TI PCM5102A passive I2S DAC.
//
// The PCM5102A has NO I2C control interface and NO hardware volume/mute.
// Key behaviours:
//   - probe() always returns true  (no I2C — hardware is always assumed present)
//   - init()  always succeeds and sets state = AVAILABLE
//   - setVolume() returns false    (no hardware volume control)
//   - setMute()   returns false    (no hardware mute unless paPin is wired)
//   - configure() accepts 44100, 48000, 96000 and rejects others
//   - type = HAL_DEV_DAC, capabilities = HAL_CAP_DAC_PATH only
//   - channelCount = 2 (stereo)
//   - healthCheck() always returns true (passive device)
//   - deinit() sets state = REMOVED

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Wire.h"
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_audio_device.h"

// ===== Inline capability flag not yet in hal_types.h =====
#ifndef HAL_CAP_CODEC
#define HAL_CAP_CODEC        (1 << 7)
#endif

// ===== Mock PCM5102A device =====
class HalPcm5102aMock : public HalAudioDevice {
public:
    uint32_t _sampleRate = 48000;
    uint8_t  _bitDepth   = 32;   // PCM5102A natively outputs 32-bit frames

    HalPcm5102aMock() {
        strncpy(_descriptor.compatible, "ti,pcm5102a", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "PCM5102A", 32);
        _descriptor.name[32] = '\0';
        _descriptor.type         = HAL_DEV_DAC;
        _descriptor.channelCount = 2;    // Stereo DAC
        _descriptor.capabilities = HAL_CAP_DAC_PATH;  // No volume, no mute in HW
        _descriptor.sampleRatesMask =
            HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
    }

    // No I2C — always detectable
    bool probe() override { return true; }

    bool init() override {
        _state = HAL_STATE_AVAILABLE;
        _ready = true;
        return true;
    }

    void deinit() override {
        _ready = false;
        _state = HAL_STATE_REMOVED;
    }

    void dumpConfig() override {}

    // Passive device — always healthy
    bool healthCheck() override { return true; }

    bool configure(uint32_t rate, uint8_t bits) override {
        if (rate != 44100 && rate != 48000 && rate != 96000) return false;
        if (bits != 16 && bits != 24 && bits != 32) return false;
        _sampleRate = rate;
        _bitDepth   = bits;
        return true;
    }

    // No hardware volume control
    bool setVolume(uint8_t) override { return false; }

    // No hardware mute (would require PA control pin — not wired in this mock)
    bool setMute(bool) override { return false; }
};

// ===== Fixtures =====
static HalPcm5102aMock* dac;

void setUp(void) {
    WireMock::reset();
    dac = new HalPcm5102aMock();
}

void tearDown(void) {
    delete dac;
    dac = nullptr;
}

// ===== Tests =====

// ----- 1. probe() always returns true (no I2C bus to NACK) -----
void test_probe_always_true(void) {
    TEST_ASSERT_TRUE(dac->probe());
}

// ----- 2. init() marks device AVAILABLE -----
void test_init_marks_available(void) {
    TEST_ASSERT_TRUE(dac->init());
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)dac->_state);
    TEST_ASSERT_TRUE(dac->_ready);
}

// ----- 3. setVolume() returns false (no hardware volume control) -----
void test_set_volume_returns_false(void) {
    dac->init();
    TEST_ASSERT_FALSE(dac->setVolume(75));
}

// ----- 4. setMute() returns false when no PA control pin -----
void test_set_mute_returns_false_without_pa_pin(void) {
    dac->init();
    TEST_ASSERT_FALSE(dac->setMute(true));
    TEST_ASSERT_FALSE(dac->setMute(false));
}

// ----- 5. configure(48000, 32) is accepted -----
void test_configure_48k_32bit_ok(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(32,     dac->_bitDepth);
}

// ----- 6. configure(44100, 16) is accepted -----
void test_configure_44100_16bit_ok(void) {
    TEST_ASSERT_TRUE(dac->configure(44100, 16));
}

// ----- 7. configure(96000, 24) is accepted -----
void test_configure_96k_24bit_ok(void) {
    TEST_ASSERT_TRUE(dac->configure(96000, 24));
}

// ----- 8. configure(192000, 32) is rejected (unsupported rate) -----
void test_configure_rejects_192khz(void) {
    TEST_ASSERT_FALSE(dac->configure(192000, 32));
    // State should be unchanged (default 48000)
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
}

// ----- 9. deinit() sets state to REMOVED and _ready to false -----
void test_deinit_sets_removed_state(void) {
    dac->init();
    dac->deinit();
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)dac->_state);
    TEST_ASSERT_FALSE(dac->_ready);
}

// ----- 10. descriptor type is HAL_DEV_DAC -----
void test_descriptor_type_is_dac(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, (int)dac->getDescriptor().type);
}

// ----- 11. channel count is 2 (stereo) -----
void test_channel_count_is_stereo(void) {
    TEST_ASSERT_EQUAL(2, dac->getDescriptor().channelCount);
}

// ----- 12. capabilities is DAC_PATH only — no volume or mute flag -----
void test_capabilities_dac_path_only(void) {
    uint8_t caps = dac->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_FALSE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_FALSE(caps & HAL_CAP_MUTE);
    TEST_ASSERT_FALSE(caps & HAL_CAP_ADC_PATH);
}

// ===== Main =====
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_probe_always_true);
    RUN_TEST(test_init_marks_available);
    RUN_TEST(test_set_volume_returns_false);
    RUN_TEST(test_set_mute_returns_false_without_pa_pin);
    RUN_TEST(test_configure_48k_32bit_ok);
    RUN_TEST(test_configure_44100_16bit_ok);
    RUN_TEST(test_configure_96k_24bit_ok);
    RUN_TEST(test_configure_rejects_192khz);
    RUN_TEST(test_deinit_sets_removed_state);
    RUN_TEST(test_descriptor_type_is_dac);
    RUN_TEST(test_channel_count_is_stereo);
    RUN_TEST(test_capabilities_dac_path_only);

    return UNITY_END();
}
