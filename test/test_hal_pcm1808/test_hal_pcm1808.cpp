// test_hal_pcm1808.cpp
// Tests a mock HalPcm1808 class modelling the TI PCM1808 I2S ADC.
//
// The PCM1808 has NO I2C control interface — gain and HPF are controlled via
// hardware GPIO pins (MD0/MD1 for format, FMT for HPF).  In software simulation
// we model these via stored state variables.
//
// Key behaviours:
//   - probe() always returns true      (no I2C)
//   - init()  always succeeds
//   - adcSetGain(): valid values 0, 6, 12, 18, 23 dB — others rejected
//   - adcSetHpfEnabled(): toggles HPF state
//   - configure(): accepts 48000 and 96000; rejects 192000
//   - type = HAL_DEV_ADC
//   - capabilities includes HAL_CAP_ADC_PATH and HAL_CAP_PGA_CONTROL
//   - NO DAC path
//   - channelCount = 2 (stereo input)
//   - healthCheck() always returns true (passive/fixed device)
//   - deinit() sets state = REMOVED
//   - sequential reconfigure updates stored rate

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Wire.h"
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_audio_device.h"

// ===== Inline capability flags not yet in hal_types.h =====
#ifndef HAL_CAP_PGA_CONTROL
#define HAL_CAP_PGA_CONTROL  (1 << 5)
#endif
#ifndef HAL_CAP_HPF_CONTROL
#define HAL_CAP_HPF_CONTROL  (1 << 6)
#endif

// ===== Mock PCM1808 ADC device =====
// Valid PGA gain settings for the PCM1808 (controlled via MD0/MD1 GPIO):
//   0 dB, +6 dB, +12 dB, +18 dB, +23.5 dB (rounded to 23)
class HalPcm1808Mock : public HalAudioDevice {
public:
    uint8_t  _gainDb     = 0;
    bool     _hpfEnabled = true;   // HPF defaults on at power-up
    uint32_t _sampleRate = 48000;
    uint8_t  _bitDepth   = 24;

    HalPcm1808Mock() {
        strncpy(_descriptor.compatible, "ti,pcm1808", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "PCM1808 ADC", 32);
        _descriptor.name[32] = '\0';
        _descriptor.type         = HAL_DEV_ADC;
        _descriptor.channelCount = 2;    // Stereo ADC
        _descriptor.capabilities =
            HAL_CAP_ADC_PATH | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL;
        _descriptor.sampleRatesMask = HAL_RATE_48K | HAL_RATE_96K;
    }

    bool probe() override { return true; }   // No I2C — always present

    HalInitResult init() override {
        _state = HAL_STATE_AVAILABLE;
        _ready = true;
        return hal_init_ok();
    }

    void deinit() override {
        _ready = false;
        _state = HAL_STATE_REMOVED;
    }

    void dumpConfig() override {}

    bool healthCheck() override { return true; }

    bool configure(uint32_t rate, uint8_t bits) override {
        if (rate != 48000 && rate != 96000) return false;
        if (bits != 16 && bits != 24 && bits != 32) return false;
        _sampleRate = rate;
        _bitDepth   = bits;
        return true;
    }

    // PCM1808 has no software-controllable output — no volume concept
    bool setVolume(uint8_t) override { return false; }
    bool setMute(bool) override      { return false; }

    // PGA gain — only discrete values allowed (matches MD0/MD1 pin states)
    bool adcSetGain(uint8_t dB) {
        // Valid: 0, 6, 12, 18, 23
        if (dB != 0 && dB != 6 && dB != 12 && dB != 18 && dB != 23) return false;
        _gainDb = dB;
        return true;
    }

    // HPF — controlled via FMT pin (GPIO only, simulated in software here)
    bool adcSetHpfEnabled(bool en) {
        _hpfEnabled = en;
        return true;
    }
};

// ===== Fixtures =====
static HalPcm1808Mock* adc;

void setUp(void) {
    WireMock::reset();
    adc = new HalPcm1808Mock();
}

void tearDown(void) {
    delete adc;
    adc = nullptr;
}

// ===== Tests =====

// ----- 1. probe() always returns true -----
void test_probe_always_true(void) {
    TEST_ASSERT_TRUE(adc->probe());
}

// ----- 2. init() marks device AVAILABLE -----
void test_init_marks_available(void) {
    TEST_ASSERT_TRUE(adc->init().success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)adc->_state);
    TEST_ASSERT_TRUE(adc->_ready);
}

// ----- 3. adcSetGain accepts all valid PCM1808 gain steps -----
void test_adc_gain_valid_range(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(0));   TEST_ASSERT_EQUAL(0,  adc->_gainDb);
    TEST_ASSERT_TRUE(adc->adcSetGain(6));   TEST_ASSERT_EQUAL(6,  adc->_gainDb);
    TEST_ASSERT_TRUE(adc->adcSetGain(12));  TEST_ASSERT_EQUAL(12, adc->_gainDb);
    TEST_ASSERT_TRUE(adc->adcSetGain(18));  TEST_ASSERT_EQUAL(18, adc->_gainDb);
    TEST_ASSERT_TRUE(adc->adcSetGain(23));  TEST_ASSERT_EQUAL(23, adc->_gainDb);
}

// ----- 4. adcSetGain rejects values not in the discrete set -----
void test_adc_gain_invalid_rejected(void) {
    TEST_ASSERT_FALSE(adc->adcSetGain(1));
    TEST_ASSERT_FALSE(adc->adcSetGain(5));
    TEST_ASSERT_FALSE(adc->adcSetGain(7));
    TEST_ASSERT_FALSE(adc->adcSetGain(24));
    TEST_ASSERT_FALSE(adc->adcSetGain(100));
    // Gain should be unchanged from default (0)
    TEST_ASSERT_EQUAL(0, adc->_gainDb);
}

// ----- 5. adcSetHpfEnabled toggles HPF state -----
void test_hpf_enable_disable(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->_hpfEnabled);      // Default: HPF on
    TEST_ASSERT_TRUE(adc->adcSetHpfEnabled(false));
    TEST_ASSERT_FALSE(adc->_hpfEnabled);
    TEST_ASSERT_TRUE(adc->adcSetHpfEnabled(true));
    TEST_ASSERT_TRUE(adc->_hpfEnabled);
}

// ----- 6. configure(48000, 24) is accepted -----
void test_configure_48k_24bit_ok(void) {
    TEST_ASSERT_TRUE(adc->configure(48000, 24));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
}

// ----- 7. configure(96000, 24) is accepted -----
void test_configure_96k_24bit_ok(void) {
    TEST_ASSERT_TRUE(adc->configure(96000, 24));
    TEST_ASSERT_EQUAL(96000u, adc->_sampleRate);
}

// ----- 8. descriptor type is HAL_DEV_ADC -----
void test_descriptor_type_is_adc(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_ADC, (int)adc->getDescriptor().type);
}

// ----- 9. no DAC_PATH capability (ADC-only device) -----
void test_no_dac_path_capability(void) {
    uint8_t caps = adc->getDescriptor().capabilities;
    TEST_ASSERT_FALSE(caps & HAL_CAP_DAC_PATH);
}

// ----- 10. channelCount is 2 (stereo ADC) -----
void test_channel_count_stereo(void) {
    TEST_ASSERT_EQUAL(2, adc->getDescriptor().channelCount);
}

// ----- 11. deinit() sets state to REMOVED -----
void test_deinit_sets_removed(void) {
    adc->init();
    adc->deinit();
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)adc->_state);
    TEST_ASSERT_FALSE(adc->_ready);
}

// ----- 12. healthCheck() always returns true -----
void test_health_check_always_true(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->healthCheck());
    // Even if Wire is reset (no I2C dependency)
    WireMock::reset();
    TEST_ASSERT_TRUE(adc->healthCheck());
}

// ----- 13. ADC_PATH capability flag is set -----
void test_capability_adc_path_set(void) {
    uint8_t caps = adc->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_ADC_PATH);
}

// ----- 14. sequential reconfigure updates sample rate correctly -----
void test_sequential_reconfigure_updates_rate(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->configure(48000, 24));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);

    TEST_ASSERT_TRUE(adc->configure(96000, 24));
    TEST_ASSERT_EQUAL(96000u, adc->_sampleRate);

    // Back to 48k
    TEST_ASSERT_TRUE(adc->configure(48000, 32));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(32,     adc->_bitDepth);
}

// ===== Main =====
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_probe_always_true);
    RUN_TEST(test_init_marks_available);
    RUN_TEST(test_adc_gain_valid_range);
    RUN_TEST(test_adc_gain_invalid_rejected);
    RUN_TEST(test_hpf_enable_disable);
    RUN_TEST(test_configure_48k_24bit_ok);
    RUN_TEST(test_configure_96k_24bit_ok);
    RUN_TEST(test_descriptor_type_is_adc);
    RUN_TEST(test_no_dac_path_capability);
    RUN_TEST(test_channel_count_stereo);
    RUN_TEST(test_deinit_sets_removed);
    RUN_TEST(test_health_check_always_true);
    RUN_TEST(test_capability_adc_path_set);
    RUN_TEST(test_sequential_reconfigure_updates_rate);

    return UNITY_END();
}
