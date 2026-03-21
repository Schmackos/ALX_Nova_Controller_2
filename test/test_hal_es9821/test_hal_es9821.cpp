// test_hal_es9821.cpp
// Tests a mock HalEs9821 class modelling the ESS ES9821 I2C ADC.
//
// The ES9821 is an ESS SABRE ADC with I2C control:
//   - I2C address 0x40 (configurable via address pins)
//   - Chip ID: 0x88 (register 0xE1)
//   - 2-channel (stereo) ADC
//   - Supported sample rates: 44100, 48000, 96000, 192000 Hz
//   - Hardware volume (16-bit per channel) via I2C register
//   - NO hardware PGA (adcSetGain(0) accepted; non-zero returns false)
//   - 8 digital filter presets (0-7) via reg 0x40 bits[4:2]
//   - No dedicated HPF register (adcSetHpfEnabled stores flag, no-op on HW)
//   - Mute via volume register (set to 0x0000)
//   - getInputSource() returns AudioInputSource with isHardwareAdc=true
//
// Key behaviours:
//   - probe() returns true (NATIVE_TEST stub)
//   - init()  succeeds, sets _ready=true, _state=AVAILABLE
//   - adcSetGain(0): accepted (0 dB, no hardware PGA)
//   - adcSetGain(non-zero): returns false (no PGA hardware)
//   - adcSetHpfEnabled(): stores flag, returns true (no-op on hardware)
//   - adcSetSampleRate(): accepts 44100/48000/96000/192000; rejects others
//   - setFilterPreset(): accepts 0-7; rejects >= 8
//   - setVolume(): 0-100% scaling
//   - setMute(): toggles mute via volume register
//   - configure(): accepts valid rates with 16/24/32 bit depth
//   - type = HAL_DEV_ADC, bus = HAL_BUS_I2C
//   - capabilities includes HAL_CAP_ADC_PATH, HAL_CAP_HW_VOLUME
//   - No DAC path, no HAL_CAP_PGA_CONTROL, no HAL_CAP_HPF_CONTROL
//   - channelCount = 2 (stereo)
//   - healthCheck() returns _ready state
//   - deinit() sets state = REMOVED, _ready = false

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Wire.h"
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_audio_device.h"
#include "../../src/hal/hal_audio_interfaces.h"
#include "../../src/audio_input_source.h"

// ===== Inline capability flags (guard against missing definitions) =====
#ifndef HAL_CAP_PGA_CONTROL
#define HAL_CAP_PGA_CONTROL  (1 << 5)
#endif
#ifndef HAL_CAP_HPF_CONTROL
#define HAL_CAP_HPF_CONTROL  (1 << 6)
#endif

// ===== Mock ES9821 ADC device =====
//
// Models the ESS ES9821 stereo ADC with I2C control interface.
// No hardware PGA — only 0 dB gain accepted. Filter presets 0-7 valid.
// Supported rates: 44100, 48000, 96000, 192000 Hz.
// No dedicated HPF register.
//
class HalEs9821Mock : public HalAudioDevice, public HalAudioAdcInterface {
public:
    static const uint8_t kI2cAddr = 0x40;
    static const uint8_t kChipId  = 0x88;

    uint8_t  _gainDb       = 0;
    bool     _hpfEnabled   = true;   // stored flag, no HW register
    uint32_t _sampleRate   = 48000;
    uint8_t  _bitDepth     = 32;
    uint8_t  _filterPreset = 0;
    bool     _muted        = false;
    uint8_t  _volume       = 100;    // 0-100 percent
    AudioInputSource _inputSrc      = {};
    bool             _inputSrcReady = false;

    HalEs9821Mock() {
        strncpy(_descriptor.compatible, "ess,es9821", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "ES9821", 32);
        _descriptor.name[32] = '\0';
        strncpy(_descriptor.manufacturer, "ESS Technology", 32);
        _descriptor.manufacturer[32] = '\0';
        _descriptor.type         = HAL_DEV_ADC;
        _descriptor.i2cAddr      = kI2cAddr;
        _descriptor.channelCount = 2;
        _descriptor.bus.type     = HAL_BUS_I2C;
        _descriptor.bus.index    = HAL_I2C_BUS_EXP;
        _descriptor.sampleRatesMask =
            HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        // No PGA_CONTROL — ES9821 has no hardware PGA
        _descriptor.capabilities = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    // ----- HalDevice lifecycle -----

    bool probe() override { return true; }

    HalInitResult init() override {
        _state = HAL_STATE_AVAILABLE;
        _ready = true;

        memset(&_inputSrc, 0, sizeof(_inputSrc));
        _inputSrc.name          = _descriptor.name;
        _inputSrc.isHardwareAdc = true;
        _inputSrc.gainLinear    = 1.0f;
        _inputSrc.vuL           = -90.0f;
        _inputSrc.vuR           = -90.0f;
        _inputSrcReady = true;

        return hal_init_ok();
    }

    void deinit() override {
        _ready         = false;
        _state         = HAL_STATE_REMOVED;
        _inputSrcReady = false;
    }

    void dumpConfig() override {}

    bool healthCheck() override { return _ready; }

    const AudioInputSource* getInputSource() const override {
        return _inputSrcReady ? &_inputSrc : nullptr;
    }

    // ----- HalAudioDevice -----

    bool configure(uint32_t rate, uint8_t bits) override {
        if (rate != 44100 && rate != 48000 && rate != 96000 && rate != 192000) return false;
        if (bits != 16 && bits != 24 && bits != 32) return false;
        _sampleRate = rate;
        _bitDepth   = bits;
        return true;
    }

    bool setVolume(uint8_t pct) override {
        if (pct > 100) pct = 100;
        _volume = pct;
        return true;
    }

    bool setMute(bool mute) override {
        _muted = mute;
        return true;
    }

    // ----- HalAudioAdcInterface -----

    bool adcSetGain(uint8_t dB) override {
        // No hardware PGA — only 0 dB is supported
        if (dB != 0) return false;
        _gainDb = 0;
        return true;
    }

    bool adcSetHpfEnabled(bool en) override {
        // No hardware register — store flag only
        _hpfEnabled = en;
        return true;
    }

    bool adcSetSampleRate(uint32_t hz) override {
        if (hz != 44100 && hz != 48000 && hz != 96000 && hz != 192000) return false;
        _sampleRate = hz;
        return true;
    }

    uint32_t adcGetSampleRate() const override {
        return _sampleRate;
    }

    // ----- ES9821-specific: filter preset -----

    bool setFilterPreset(uint8_t preset) {
        if (preset >= 8) return false;
        _filterPreset = preset;
        return true;
    }
};

// ===== Fixtures =====
static HalEs9821Mock* adc;

void setUp(void) {
    WireMock::reset();
    adc = new HalEs9821Mock();
}

void tearDown(void) {
    delete adc;
    adc = nullptr;
}

// ==========================================================================
// Section 1: Descriptor tests
// ==========================================================================

// ----- 1. descriptor compatible string is "ess,es9821" -----
void test_descriptor_compatible_string(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9821", adc->getDescriptor().compatible);
}

// ----- 2. descriptor type is HAL_DEV_ADC -----
void test_descriptor_type_is_adc(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_ADC, (int)adc->getDescriptor().type);
}

// ----- 3. capabilities include HAL_CAP_ADC_PATH and HAL_CAP_HW_VOLUME only -----
void test_descriptor_capabilities(void) {
    uint8_t caps = adc->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    // ES9821 has no hardware PGA and no HPF register
    TEST_ASSERT_FALSE(caps & HAL_CAP_PGA_CONTROL);
    TEST_ASSERT_FALSE(caps & HAL_CAP_HPF_CONTROL);
    // ADC-only: no DAC path
    TEST_ASSERT_FALSE(caps & HAL_CAP_DAC_PATH);
}

// ----- 4. channel count is 2 (stereo) -----
void test_descriptor_channel_count(void) {
    TEST_ASSERT_EQUAL(2, adc->getDescriptor().channelCount);
}

// ----- 5. bus type is I2C -----
void test_descriptor_bus_type(void) {
    TEST_ASSERT_EQUAL(HAL_BUS_I2C, (int)adc->getDescriptor().bus.type);
}

// ----- 6. I2C address is 0x40 -----
void test_descriptor_i2c_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0x40, adc->getDescriptor().i2cAddr);
}

// ==========================================================================
// Section 2: Lifecycle tests
// ==========================================================================

// ----- 7. probe() returns true (NATIVE_TEST stub) -----
void test_probe_returns_true(void) {
    TEST_ASSERT_TRUE(adc->probe());
}

// ----- 8. init() succeeds, sets _ready=true and state=AVAILABLE -----
void test_init_success(void) {
    HalInitResult res = adc->init();
    TEST_ASSERT_TRUE(res.success);
    TEST_ASSERT_TRUE(adc->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)adc->_state);
}

// ----- 9. deinit() clears _ready and sets state=REMOVED -----
void test_deinit_clears_ready(void) {
    adc->init();
    adc->deinit();
    TEST_ASSERT_FALSE(adc->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)adc->_state);
}

// ----- 10. healthCheck() returns true after init -----
void test_health_check_after_init(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->healthCheck());
}

// ----- 11. healthCheck() returns false before init -----
void test_health_check_before_init(void) {
    TEST_ASSERT_FALSE(adc->healthCheck());
}

// ==========================================================================
// Section 3: Audio input source tests
// ==========================================================================

// ----- 12. getInputSource() returns non-null after init -----
void test_get_input_source_after_init(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
}

// ----- 13. getInputSource() returns null before init -----
void test_get_input_source_before_init(void) {
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NULL(src);
}

// ----- 14. input source isHardwareAdc is true -----
void test_input_source_is_hardware_adc(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_TRUE(src->isHardwareAdc);
}

// ----- 15. input source name matches descriptor name "ES9821" -----
void test_input_source_name_matches_descriptor(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL_STRING("ES9821", src->name);
}

// ----- 15b. input source initial VU is -90 dBFS -----
void test_input_source_vu_initial_minus90(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL_FLOAT(-90.0f, src->vuL);
    TEST_ASSERT_EQUAL_FLOAT(-90.0f, src->vuR);
}

// ----- 15c. input source gainLinear is 1.0 (unity) -----
void test_input_source_gain_linear_unity(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, src->gainLinear);
}

// ----- 15d. getInputSource() returns null after deinit -----
void test_get_input_source_after_deinit(void) {
    adc->init();
    TEST_ASSERT_NOT_NULL(adc->getInputSource());
    adc->deinit();
    TEST_ASSERT_NULL(adc->getInputSource());
}

// ==========================================================================
// Section 4: Gain tests (no PGA — only 0 dB accepted)
// ==========================================================================

// ----- 16. adcSetGain(0) succeeds -----
void test_gain_0db_accepted(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(0));
    TEST_ASSERT_EQUAL(0, adc->_gainDb);
}

// ----- 17. adcSetGain(1) returns false (no PGA) -----
void test_gain_1db_rejected(void) {
    TEST_ASSERT_FALSE(adc->adcSetGain(1));
    TEST_ASSERT_EQUAL(0, adc->_gainDb);  // Unchanged
}

// ----- 18. adcSetGain(6) returns false (no PGA) -----
void test_gain_6db_rejected(void) {
    TEST_ASSERT_FALSE(adc->adcSetGain(6));
    TEST_ASSERT_EQUAL(0, adc->_gainDb);
}

// ----- 19. adcSetGain(18) returns false (no PGA) -----
void test_gain_18db_rejected(void) {
    TEST_ASSERT_FALSE(adc->adcSetGain(18));
    TEST_ASSERT_EQUAL(0, adc->_gainDb);
}

// ----- 20. adcSetGain(30) returns false (no PGA) -----
void test_gain_30db_rejected(void) {
    TEST_ASSERT_FALSE(adc->adcSetGain(30));
    TEST_ASSERT_EQUAL(0, adc->_gainDb);
}

// ----- 20b. adcSetGain(255) returns false (no PGA) -----
void test_gain_255_rejected(void) {
    TEST_ASSERT_FALSE(adc->adcSetGain(255));
    TEST_ASSERT_EQUAL(0, adc->_gainDb);
}

// ==========================================================================
// Section 5: Volume tests
// ==========================================================================

// ----- 21. setVolume(0) succeeds (mute-level) -----
void test_volume_0_percent(void) {
    TEST_ASSERT_TRUE(adc->setVolume(0));
    TEST_ASSERT_EQUAL(0, adc->_volume);
}

// ----- 22. setVolume(100) succeeds (0 dB) -----
void test_volume_100_percent(void) {
    adc->_volume = 0;
    TEST_ASSERT_TRUE(adc->setVolume(100));
    TEST_ASSERT_EQUAL(100, adc->_volume);
}

// ----- 23. setVolume(50) succeeds (mid-range) -----
void test_volume_50_percent(void) {
    TEST_ASSERT_TRUE(adc->setVolume(50));
    TEST_ASSERT_EQUAL(50, adc->_volume);
}

// ==========================================================================
// Section 6: HPF tests (stored flag, no hardware register)
// ==========================================================================

// ----- 24. adcSetHpfEnabled(true) succeeds and stores flag -----
void test_hpf_enable(void) {
    adc->_hpfEnabled = false;
    TEST_ASSERT_TRUE(adc->adcSetHpfEnabled(true));
    TEST_ASSERT_TRUE(adc->_hpfEnabled);
}

// ----- 25. adcSetHpfEnabled(false) succeeds and stores flag -----
void test_hpf_disable(void) {
    TEST_ASSERT_TRUE(adc->_hpfEnabled);  // Default is on
    TEST_ASSERT_TRUE(adc->adcSetHpfEnabled(false));
    TEST_ASSERT_FALSE(adc->_hpfEnabled);
}

// ==========================================================================
// Section 7: Sample rate tests
// ==========================================================================

// ----- 26. adcSetSampleRate(44100) succeeds -----
void test_sample_rate_44k1(void) {
    TEST_ASSERT_TRUE(adc->adcSetSampleRate(44100));
    TEST_ASSERT_EQUAL(44100u, adc->_sampleRate);
}

// ----- 27. adcSetSampleRate(48000) succeeds -----
void test_sample_rate_48k(void) {
    TEST_ASSERT_TRUE(adc->adcSetSampleRate(48000));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
}

// ----- 28. adcSetSampleRate(96000) succeeds -----
void test_sample_rate_96k(void) {
    TEST_ASSERT_TRUE(adc->adcSetSampleRate(96000));
    TEST_ASSERT_EQUAL(96000u, adc->_sampleRate);
}

// ----- 28b. adcSetSampleRate(192000) succeeds -----
void test_sample_rate_192k(void) {
    TEST_ASSERT_TRUE(adc->adcSetSampleRate(192000));
    TEST_ASSERT_EQUAL(192000u, adc->_sampleRate);
}

// ----- 28c. adcSetSampleRate(8000) returns false -----
void test_sample_rate_unsupported(void) {
    TEST_ASSERT_FALSE(adc->adcSetSampleRate(8000));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
}

// ----- 28d. adcSetSampleRate(384000) returns false (not supported on ES9821) -----
void test_sample_rate_384k_rejected(void) {
    TEST_ASSERT_FALSE(adc->adcSetSampleRate(384000));
}

// ----- 29. adcGetSampleRate() returns the set rate -----
void test_get_sample_rate(void) {
    adc->adcSetSampleRate(96000);
    TEST_ASSERT_EQUAL(96000u, adc->adcGetSampleRate());
}

// ==========================================================================
// Section 8: Filter preset tests
// ==========================================================================

// ----- 30. setFilterPreset(0-7) all succeed -----
void test_filter_preset_valid(void) {
    for (uint8_t i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(adc->setFilterPreset(i));
        TEST_ASSERT_EQUAL(i, adc->_filterPreset);
    }
}

// ----- 31. setFilterPreset(8) returns false -----
void test_filter_preset_invalid(void) {
    TEST_ASSERT_FALSE(adc->setFilterPreset(8));
    TEST_ASSERT_FALSE(adc->setFilterPreset(255));
    // Should remain at default (0)
    TEST_ASSERT_EQUAL(0, adc->_filterPreset);
}

// ==========================================================================
// Section 9: Mute test
// ==========================================================================

// ----- 32. setMute(true) and setMute(false) succeed -----
void test_mute_on_off(void) {
    TEST_ASSERT_FALSE(adc->_muted);  // Default: not muted
    TEST_ASSERT_TRUE(adc->setMute(true));
    TEST_ASSERT_TRUE(adc->_muted);
    TEST_ASSERT_TRUE(adc->setMute(false));
    TEST_ASSERT_FALSE(adc->_muted);
}

// ==========================================================================
// Section 10: Configure test
// ==========================================================================

// ----- 33. configure(48000, 32) succeeds -----
void test_configure_valid(void) {
    TEST_ASSERT_TRUE(adc->configure(48000, 32));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(32, adc->_bitDepth);
}

// ----- 33b. configure(44100, 24) succeeds -----
void test_configure_44k1_24bit(void) {
    TEST_ASSERT_TRUE(adc->configure(44100, 24));
    TEST_ASSERT_EQUAL(44100u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(24, adc->_bitDepth);
}

// ----- 33c. configure with unsupported rate fails -----
void test_configure_unsupported_rate(void) {
    TEST_ASSERT_FALSE(adc->configure(8000, 16));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
}

// ----- 33d. configure with unsupported bit depth fails -----
void test_configure_unsupported_bits(void) {
    TEST_ASSERT_FALSE(adc->configure(48000, 8));
}

// ----- 33e. sequential reconfigure updates rate and depth -----
void test_sequential_reconfigure(void) {
    TEST_ASSERT_TRUE(adc->configure(44100, 16));
    TEST_ASSERT_EQUAL(44100u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(16, adc->_bitDepth);

    TEST_ASSERT_TRUE(adc->configure(192000, 32));
    TEST_ASSERT_EQUAL(192000u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(32, adc->_bitDepth);

    TEST_ASSERT_TRUE(adc->configure(48000, 24));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(24, adc->_bitDepth);
}

// ==========================================================================
// Main
// ==========================================================================

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    // Section 1: Descriptor
    RUN_TEST(test_descriptor_compatible_string);
    RUN_TEST(test_descriptor_type_is_adc);
    RUN_TEST(test_descriptor_capabilities);
    RUN_TEST(test_descriptor_channel_count);
    RUN_TEST(test_descriptor_bus_type);
    RUN_TEST(test_descriptor_i2c_address);

    // Section 2: Lifecycle
    RUN_TEST(test_probe_returns_true);
    RUN_TEST(test_init_success);
    RUN_TEST(test_deinit_clears_ready);
    RUN_TEST(test_health_check_after_init);
    RUN_TEST(test_health_check_before_init);

    // Section 3: Audio input source
    RUN_TEST(test_get_input_source_after_init);
    RUN_TEST(test_get_input_source_before_init);
    RUN_TEST(test_input_source_is_hardware_adc);
    RUN_TEST(test_input_source_name_matches_descriptor);
    RUN_TEST(test_input_source_vu_initial_minus90);
    RUN_TEST(test_input_source_gain_linear_unity);
    RUN_TEST(test_get_input_source_after_deinit);

    // Section 4: Gain (no PGA)
    RUN_TEST(test_gain_0db_accepted);
    RUN_TEST(test_gain_1db_rejected);
    RUN_TEST(test_gain_6db_rejected);
    RUN_TEST(test_gain_18db_rejected);
    RUN_TEST(test_gain_30db_rejected);
    RUN_TEST(test_gain_255_rejected);

    // Section 5: Volume
    RUN_TEST(test_volume_0_percent);
    RUN_TEST(test_volume_100_percent);
    RUN_TEST(test_volume_50_percent);

    // Section 6: HPF
    RUN_TEST(test_hpf_enable);
    RUN_TEST(test_hpf_disable);

    // Section 7: Sample rate
    RUN_TEST(test_sample_rate_44k1);
    RUN_TEST(test_sample_rate_48k);
    RUN_TEST(test_sample_rate_96k);
    RUN_TEST(test_sample_rate_192k);
    RUN_TEST(test_sample_rate_unsupported);
    RUN_TEST(test_sample_rate_384k_rejected);
    RUN_TEST(test_get_sample_rate);

    // Section 8: Filter preset
    RUN_TEST(test_filter_preset_valid);
    RUN_TEST(test_filter_preset_invalid);

    // Section 9: Mute
    RUN_TEST(test_mute_on_off);

    // Section 10: Configure
    RUN_TEST(test_configure_valid);
    RUN_TEST(test_configure_44k1_24bit);
    RUN_TEST(test_configure_unsupported_rate);
    RUN_TEST(test_configure_unsupported_bits);
    RUN_TEST(test_sequential_reconfigure);

    return UNITY_END();
}
