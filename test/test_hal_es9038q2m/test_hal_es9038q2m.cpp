// test_hal_es9038q2m.cpp
// Tests a mock HalEs9038Q2M class modelling the ESS ES9038Q2M I2C DAC.
//
// The ES9038Q2M is a high-performance ESS SABRE DAC (Hyperstream II) with I2C control:
//   - I2C address 0x48 (configurable via address pins)
//   - 2-channel (stereo) DAC
//   - 128dB DNR, PCM up to 768kHz, DSD512
//   - 128-step hardware volume attenuation (0.5dB per step)
//   - 8 digital filter presets (0-7)
//   - Mute via I2C register
//   - buildSink() returns true, populates name/channels/halSlot
//   - dacSetVolume/dacGetVolume/dacSetMute/dacIsMuted delegating interface
//
// Key behaviours:
//   - probe() returns true (NATIVE_TEST stub)
//   - init()  succeeds, sets _ready=true, _state=AVAILABLE
//   - setVolume(): 0-100% scaling (100%=0x00 attenuation, 0%=0xFF attenuation)
//   - setMute(): toggles _muted state
//   - setFilterPreset(): accepts 0-7; rejects >= 8
//   - configure(): accepts 44100/48000/96000/192000/384000/768000 with 16/24/32 bit
//   - type = HAL_DEV_DAC
//   - bus = HAL_BUS_I2C
//   - capabilities include HAL_CAP_DAC_PATH, HAL_CAP_HW_VOLUME, HAL_CAP_MUTE, HAL_CAP_FILTERS
//   - NO ADC path, NO PGA, NO HPF control
//   - channelCount = 2 (stereo)
//   - healthCheck() returns _initialized state
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
#include "../../src/audio_output_sink.h"

// ===== Mock ES9038Q2M DAC device =====
//
// Models the ESS ES9038Q2M stereo DAC with I2C control interface.
// Volume uses inverted attenuation: 100% -> 0x00 (no attenuation), 0% -> 0xFF (full mute).
// Filter presets 0-7 valid. Supported rates: 44100, 48000, 96000, 192000, 384000, 768000 Hz.
//
class HalEs9038Q2MMock : public HalAudioDevice, public HalAudioDacInterface {
public:
    static const uint8_t kI2cAddr = 0x48;

    uint8_t  _filterPreset = 0;
    bool     _muted        = false;
    uint8_t  _volume       = 100;   // 0-100 percent
    uint32_t _sampleRate   = 48000;
    uint8_t  _bitDepth     = 32;
    bool     _initialized  = false;
    float    _muteRampState = 1.0f;

    HalEs9038Q2MMock() {
        strncpy(_descriptor.compatible, "ess,es9038q2m", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "ES9038Q2M", 32);
        _descriptor.name[32] = '\0';
        strncpy(_descriptor.manufacturer, "ESS Technology", 32);
        _descriptor.manufacturer[32] = '\0';
        _descriptor.type         = HAL_DEV_DAC;
        _descriptor.i2cAddr      = kI2cAddr;
        _descriptor.channelCount = 2;
        _descriptor.bus.type     = HAL_BUS_I2C;
        _descriptor.bus.index    = HAL_I2C_BUS_EXP;
        _descriptor.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K |
                                      HAL_RATE_192K  | HAL_RATE_384K | HAL_RATE_768K;
        _descriptor.capabilities =
            HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    // ----- HalDevice lifecycle -----

    bool probe() override { return true; }

    HalInitResult init() override {
        _initialized = true;
        _state = HAL_STATE_AVAILABLE;
        _ready = true;
        return hal_init_ok();
    }

    void deinit() override {
        _ready = false;
        _initialized = false;
        _state = HAL_STATE_REMOVED;
    }

    void dumpConfig() override {}

    bool healthCheck() override { return _initialized; }

    // ----- HalAudioDevice -----

    bool configure(uint32_t rate, uint8_t bits) override {
        if (rate != 44100 && rate != 48000 && rate != 96000 &&
            rate != 192000 && rate != 384000 && rate != 768000) return false;
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

    // ----- HalAudioDacInterface -----

    bool    dacSetVolume(uint8_t pct) override { return setVolume(pct); }
    bool    dacSetMute(bool m) override        { return setMute(m); }
    uint8_t dacGetVolume() const override      { return _volume; }
    bool    dacIsMuted() const override        { return _muted; }

    bool dacSetSampleRate(uint32_t hz) override {
        return configure(hz, _bitDepth);
    }

    bool dacSetBitDepth(uint8_t bits) override {
        return configure(_sampleRate, bits);
    }

    uint32_t dacGetSampleRate() const override { return _sampleRate; }

    // ----- Filter preset -----

    bool setFilterPreset(uint8_t preset) {
        if (preset >= 8) return false;
        _filterPreset = preset;
        return true;
    }

    // ----- buildSink -----

    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override {
        if (!out) return false;
        if (sinkSlot >= AUDIO_OUT_MAX_SINKS) return false;
        *out = AUDIO_OUTPUT_SINK_INIT;
        out->name         = _descriptor.name;
        out->firstChannel = (uint8_t)(sinkSlot * 2);
        out->channelCount = _descriptor.channelCount;
        out->halSlot      = _slot;
        out->write        = nullptr;   // No hardware write in native test
        out->isReady      = nullptr;
        return true;
    }

    bool hasHardwareVolume() const override { return true; }
};

// ===== Fixtures =====
static HalEs9038Q2MMock* dac;

void setUp(void) {
    WireMock::reset();
    dac = new HalEs9038Q2MMock();
}

void tearDown(void) {
    delete dac;
    dac = nullptr;
}

// ==========================================================================
// Section 1: Descriptor tests
// ==========================================================================

// ----- 1. descriptor compatible string is "ess,es9038q2m" -----
void test_descriptor_compatible_string(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9038q2m", dac->getDescriptor().compatible);
}

// ----- 2. descriptor type is HAL_DEV_DAC -----
void test_descriptor_type_is_dac(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, (int)dac->getDescriptor().type);
}

// ----- 3. capabilities include HAL_CAP_DAC_PATH, HW_VOLUME, MUTE, FILTERS -----
void test_descriptor_capabilities(void) {
    uint32_t caps = dac->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(caps & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(caps & HAL_CAP_FILTERS);
    // DAC-only: no ADC path, no PGA, no HPF
    TEST_ASSERT_FALSE(caps & HAL_CAP_ADC_PATH);
    TEST_ASSERT_FALSE(caps & HAL_CAP_PGA_CONTROL);
    TEST_ASSERT_FALSE(caps & HAL_CAP_HPF_CONTROL);
}

// ----- 4. channel count is 2 (stereo) -----
void test_descriptor_channel_count(void) {
    TEST_ASSERT_EQUAL(2, dac->getDescriptor().channelCount);
}

// ----- 5. bus type is I2C -----
void test_descriptor_bus_type(void) {
    TEST_ASSERT_EQUAL(HAL_BUS_I2C, (int)dac->getDescriptor().bus.type);
}

// ----- 6. I2C address is 0x48 -----
void test_descriptor_i2c_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0x48, dac->getDescriptor().i2cAddr);
}

// ----- 7. sample rate mask includes 44.1/48/96/192/384/768 kHz -----
void test_descriptor_sample_rate_mask(void) {
    uint32_t mask = dac->getDescriptor().sampleRatesMask;
    TEST_ASSERT_TRUE(mask & HAL_RATE_44K1);
    TEST_ASSERT_TRUE(mask & HAL_RATE_48K);
    TEST_ASSERT_TRUE(mask & HAL_RATE_96K);
    TEST_ASSERT_TRUE(mask & HAL_RATE_192K);
    TEST_ASSERT_TRUE(mask & HAL_RATE_384K);
    TEST_ASSERT_TRUE(mask & HAL_RATE_768K);
}

// ==========================================================================
// Section 2: Lifecycle tests
// ==========================================================================

// ----- 8. probe() returns true (NATIVE_TEST stub) -----
void test_probe_returns_true(void) {
    TEST_ASSERT_TRUE(dac->probe());
}

// ----- 9. init() succeeds, sets _ready=true and state=AVAILABLE -----
void test_init_success(void) {
    HalInitResult res = dac->init();
    TEST_ASSERT_TRUE(res.success);
    TEST_ASSERT_TRUE(dac->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)dac->_state);
}

// ----- 10. deinit() clears _ready and sets state=REMOVED -----
void test_deinit_clears_ready(void) {
    dac->init();
    dac->deinit();
    TEST_ASSERT_FALSE(dac->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)dac->_state);
}

// ----- 11. healthCheck() returns true after init -----
void test_health_check_after_init(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->healthCheck());
}

// ----- 12. healthCheck() returns false before init -----
void test_health_check_before_init(void) {
    TEST_ASSERT_FALSE(dac->healthCheck());
}

// ----- 13. healthCheck() returns false after deinit -----
void test_health_check_after_deinit(void) {
    dac->init();
    dac->deinit();
    TEST_ASSERT_FALSE(dac->healthCheck());
}

// ==========================================================================
// Section 3: Volume tests
// ==========================================================================

// ----- 14. setVolume(0) succeeds (minimum — full attenuation) -----
void test_volume_0_percent(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setVolume(0));
    TEST_ASSERT_EQUAL(0, dac->_volume);
}

// ----- 15. setVolume(100) succeeds (0 dB — no attenuation) -----
void test_volume_100_percent(void) {
    dac->init();
    dac->_volume = 0;
    TEST_ASSERT_TRUE(dac->setVolume(100));
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ----- 16. setVolume(50) succeeds (mid-range) -----
void test_volume_50_percent(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setVolume(50));
    TEST_ASSERT_EQUAL(50, dac->_volume);
}

// ----- 17. setVolume > 100 is clamped to 100 -----
void test_volume_clamp_above_100(void) {
    dac->init();
    dac->setVolume(150);
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ==========================================================================
// Section 4: Mute tests
// ==========================================================================

// ----- 18. setMute(true) and setMute(false) succeed -----
void test_mute_on_off(void) {
    dac->init();
    TEST_ASSERT_FALSE(dac->_muted);   // Default: not muted
    TEST_ASSERT_TRUE(dac->setMute(true));
    TEST_ASSERT_TRUE(dac->_muted);
    TEST_ASSERT_TRUE(dac->setMute(false));
    TEST_ASSERT_FALSE(dac->_muted);
}

// ----- 19. mute state defaults to false before init -----
void test_mute_default_false(void) {
    TEST_ASSERT_FALSE(dac->_muted);
}

// ==========================================================================
// Section 5: Filter preset tests
// ==========================================================================

// ----- 20. setFilterPreset(0-7) all succeed -----
void test_filter_preset_valid(void) {
    for (uint8_t i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(dac->setFilterPreset(i));
        TEST_ASSERT_EQUAL(i, dac->_filterPreset);
    }
}

// ----- 21. setFilterPreset(8) returns false -----
void test_filter_preset_8_invalid(void) {
    TEST_ASSERT_FALSE(dac->setFilterPreset(8));
    TEST_ASSERT_EQUAL(0, dac->_filterPreset);
}

// ----- 22. setFilterPreset(255) returns false -----
void test_filter_preset_255_invalid(void) {
    TEST_ASSERT_FALSE(dac->setFilterPreset(255));
    TEST_ASSERT_EQUAL(0, dac->_filterPreset);
}

// ----- 23. filter preset defaults to 0 -----
void test_filter_preset_default_zero(void) {
    TEST_ASSERT_EQUAL(0, dac->_filterPreset);
}

// ==========================================================================
// Section 6: Sample rate tests
// ==========================================================================

// ----- 24. configure(44100, 32) succeeds -----
void test_sample_rate_44k1(void) {
    TEST_ASSERT_TRUE(dac->configure(44100, 32));
    TEST_ASSERT_EQUAL(44100u, dac->_sampleRate);
}

// ----- 25. configure(48000, 32) succeeds -----
void test_sample_rate_48k(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
}

// ----- 26. configure(96000, 32) succeeds -----
void test_sample_rate_96k(void) {
    TEST_ASSERT_TRUE(dac->configure(96000, 32));
    TEST_ASSERT_EQUAL(96000u, dac->_sampleRate);
}

// ----- 27. configure(192000, 32) succeeds -----
void test_sample_rate_192k(void) {
    TEST_ASSERT_TRUE(dac->configure(192000, 32));
    TEST_ASSERT_EQUAL(192000u, dac->_sampleRate);
}

// ----- 28. configure(384000, 32) succeeds -----
void test_sample_rate_384k(void) {
    TEST_ASSERT_TRUE(dac->configure(384000, 32));
    TEST_ASSERT_EQUAL(384000u, dac->_sampleRate);
}

// ----- 29. configure(768000, 32) succeeds -----
void test_sample_rate_768k(void) {
    TEST_ASSERT_TRUE(dac->configure(768000, 32));
    TEST_ASSERT_EQUAL(768000u, dac->_sampleRate);
}

// ----- 30. configure(8000, 32) returns false (unsupported) -----
void test_sample_rate_unsupported(void) {
    TEST_ASSERT_FALSE(dac->configure(8000, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);  // unchanged
}

// ==========================================================================
// Section 7: Configure tests (bit depth)
// ==========================================================================

// ----- 31. configure(48000, 16) succeeds -----
void test_configure_16bit(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 16));
    TEST_ASSERT_EQUAL(16, dac->_bitDepth);
}

// ----- 32. configure(48000, 24) succeeds -----
void test_configure_24bit(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 24));
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);
}

// ----- 33. configure(48000, 32) succeeds -----
void test_configure_32bit(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 32));
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);
}

// ----- 34. configure with unsupported bit depth fails -----
void test_configure_unsupported_bits(void) {
    TEST_ASSERT_FALSE(dac->configure(48000, 8));
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);  // unchanged
}

// ----- 35. sequential reconfigure updates rate and depth -----
void test_sequential_reconfigure(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 24));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);

    TEST_ASSERT_TRUE(dac->configure(192000, 32));
    TEST_ASSERT_EQUAL(192000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);

    TEST_ASSERT_TRUE(dac->configure(768000, 16));
    TEST_ASSERT_EQUAL(768000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(16, dac->_bitDepth);
}

// ==========================================================================
// Section 8: DAC interface tests (HalAudioDacInterface delegation)
// ==========================================================================

// ----- 36. dacSetVolume delegates to setVolume -----
void test_dac_set_volume(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->dacSetVolume(75));
    TEST_ASSERT_EQUAL(75, dac->dacGetVolume());
}

// ----- 37. dacGetVolume returns current volume -----
void test_dac_get_volume_initial(void) {
    TEST_ASSERT_EQUAL(100, dac->dacGetVolume());
}

// ----- 38. dacSetMute(true) sets muted state -----
void test_dac_set_mute_true(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->dacSetMute(true));
    TEST_ASSERT_TRUE(dac->dacIsMuted());
}

// ----- 39. dacSetMute(false) clears muted state -----
void test_dac_set_mute_false(void) {
    dac->init();
    dac->dacSetMute(true);
    TEST_ASSERT_TRUE(dac->dacSetMute(false));
    TEST_ASSERT_FALSE(dac->dacIsMuted());
}

// ----- 40. dacIsMuted defaults false -----
void test_dac_is_muted_default(void) {
    TEST_ASSERT_FALSE(dac->dacIsMuted());
}

// ----- 41. dacSetSampleRate delegates to configure -----
void test_dac_set_sample_rate(void) {
    TEST_ASSERT_TRUE(dac->dacSetSampleRate(96000));
    TEST_ASSERT_EQUAL(96000u, dac->dacGetSampleRate());
}

// ----- 42. dacGetSampleRate returns current rate -----
void test_dac_get_sample_rate_initial(void) {
    TEST_ASSERT_EQUAL(48000u, dac->dacGetSampleRate());
}

// ==========================================================================
// Section 9: buildSink tests
// ==========================================================================

// ----- 43. buildSink(0, out) returns true -----
void test_build_sink_returns_true(void) {
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_TRUE(dac->buildSink(0, &out));
}

// ----- 44. buildSink populates name from descriptor -----
void test_build_sink_name(void) {
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    dac->buildSink(0, &out);
    TEST_ASSERT_NOT_NULL(out.name);
    TEST_ASSERT_EQUAL_STRING("ES9038Q2M", out.name);
}

// ----- 45. buildSink populates channelCount = 2 -----
void test_build_sink_channel_count(void) {
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    dac->buildSink(0, &out);
    TEST_ASSERT_EQUAL(2, out.channelCount);
}

// ----- 46. buildSink with null out returns false -----
void test_build_sink_null_out(void) {
    TEST_ASSERT_FALSE(dac->buildSink(0, nullptr));
}

// ----- 47. buildSink with slot >= AUDIO_OUT_MAX_SINKS returns false -----
void test_build_sink_slot_overflow(void) {
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_FALSE(dac->buildSink(AUDIO_OUT_MAX_SINKS, &out));
}

// ----- 48. hasHardwareVolume returns true -----
void test_has_hardware_volume(void) {
    TEST_ASSERT_TRUE(dac->hasHardwareVolume());
}

// ==========================================================================
// Main
// ==========================================================================

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    // Section 1: Descriptor
    RUN_TEST(test_descriptor_compatible_string);
    RUN_TEST(test_descriptor_type_is_dac);
    RUN_TEST(test_descriptor_capabilities);
    RUN_TEST(test_descriptor_channel_count);
    RUN_TEST(test_descriptor_bus_type);
    RUN_TEST(test_descriptor_i2c_address);
    RUN_TEST(test_descriptor_sample_rate_mask);

    // Section 2: Lifecycle
    RUN_TEST(test_probe_returns_true);
    RUN_TEST(test_init_success);
    RUN_TEST(test_deinit_clears_ready);
    RUN_TEST(test_health_check_after_init);
    RUN_TEST(test_health_check_before_init);
    RUN_TEST(test_health_check_after_deinit);

    // Section 3: Volume
    RUN_TEST(test_volume_0_percent);
    RUN_TEST(test_volume_100_percent);
    RUN_TEST(test_volume_50_percent);
    RUN_TEST(test_volume_clamp_above_100);

    // Section 4: Mute
    RUN_TEST(test_mute_on_off);
    RUN_TEST(test_mute_default_false);

    // Section 5: Filter preset
    RUN_TEST(test_filter_preset_valid);
    RUN_TEST(test_filter_preset_8_invalid);
    RUN_TEST(test_filter_preset_255_invalid);
    RUN_TEST(test_filter_preset_default_zero);

    // Section 6: Sample rate
    RUN_TEST(test_sample_rate_44k1);
    RUN_TEST(test_sample_rate_48k);
    RUN_TEST(test_sample_rate_96k);
    RUN_TEST(test_sample_rate_192k);
    RUN_TEST(test_sample_rate_384k);
    RUN_TEST(test_sample_rate_768k);
    RUN_TEST(test_sample_rate_unsupported);

    // Section 7: Configure (bit depth)
    RUN_TEST(test_configure_16bit);
    RUN_TEST(test_configure_24bit);
    RUN_TEST(test_configure_32bit);
    RUN_TEST(test_configure_unsupported_bits);
    RUN_TEST(test_sequential_reconfigure);

    // Section 8: DAC interface
    RUN_TEST(test_dac_set_volume);
    RUN_TEST(test_dac_get_volume_initial);
    RUN_TEST(test_dac_set_mute_true);
    RUN_TEST(test_dac_set_mute_false);
    RUN_TEST(test_dac_is_muted_default);
    RUN_TEST(test_dac_set_sample_rate);
    RUN_TEST(test_dac_get_sample_rate_initial);

    // Section 9: buildSink
    RUN_TEST(test_build_sink_returns_true);
    RUN_TEST(test_build_sink_name);
    RUN_TEST(test_build_sink_channel_count);
    RUN_TEST(test_build_sink_null_out);
    RUN_TEST(test_build_sink_slot_overflow);
    RUN_TEST(test_has_hardware_volume);

    return UNITY_END();
}
