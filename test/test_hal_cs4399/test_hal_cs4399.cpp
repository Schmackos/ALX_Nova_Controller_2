// test_hal_cs4399.cpp
// Tests a mock HalCs4399 class modelling the Cirrus Logic CS4399 I2C DAC.
//
// The CS4399 is a high-performance Cirrus Logic MasterHIFI DAC:
//   - I2C address 0x48
//   - 2-channel (stereo) DAC
//   - 130dBA dynamic range
//   - -108dB THD+N
//   - 32-bit, PCM up to 384kHz, DSD
//   - MasterHIFI architecture with NOS filter mode
//   - Hardware volume: 0.5dB/step attenuation
//   - 5 digital filter presets (0-4, including NOS mode)
//   - Mute via I2C register
//   - buildSink() returns true, populates name/channels/halSlot
//   - dacSetVolume/dacGetVolume/dacSetMute/dacIsMuted delegating interface
//   - NOS mode: 512 single-bit elements, toggle via setNosMode()/isNosMode()
//
// Key behaviours:
//   - probe() returns true (NATIVE_TEST stub)
//   - init()  succeeds, sets _ready=true, _state=AVAILABLE
//   - setVolume(): 0-100% scaling, clamped at 100
//   - setMute(): toggles _muted state
//   - setFilterPreset(): accepts 0-4; rejects >= 5 (CS4399 has 5 presets)
//   - configure(): accepts 44100/48000/96000/192000/384000 with 16/24/32 bit
//     (NO 768kHz support — unlike ESS SABRE DACs)
//   - type = HAL_DEV_DAC
//   - bus = HAL_BUS_I2C
//   - capabilities include HAL_CAP_DAC_PATH, HAL_CAP_HW_VOLUME, HAL_CAP_MUTE,
//     HAL_CAP_FILTERS, HAL_CAP_DSD
//   - NO ADC path, NO PGA, NO HPF control
//   - channelCount = 2 (stereo)
//   - healthCheck() returns _initialized state
//   - deinit() sets state = REMOVED, _ready = false
//   - setNosMode(bool) / isNosMode() control NOS filter mode
//   - NOS mode survives reconfigure (rate/bit depth change)

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

// ===== Mock CS4399 DAC device =====
//
// Models the Cirrus Logic CS4399 stereo DAC with I2C control interface.
// Volume uses percentage scaling: 100% = 0dB, 0% = full attenuation.
// Filter presets 0-4 valid (5 presets total, including NOS mode).
// Supported rates: 44100, 48000, 96000, 192000, 384000 Hz.
// Does NOT support 768kHz (unlike ESS SABRE DACs).
// NOS mode: 512 single-bit elements, persists across reconfigure().
//
class HalCs4399Mock : public HalAudioDevice, public HalAudioDacInterface {
public:
    static const uint8_t kI2cAddr = 0x48;

    uint8_t  _filterPreset  = 0;
    bool     _muted         = false;
    uint8_t  _volume        = 100;   // 0-100 percent
    uint32_t _sampleRate    = 48000;
    uint8_t  _bitDepth      = 32;
    bool     _initialized   = false;
    float    _muteRampState = 1.0f;
    bool     _nosEnabled    = false;

    HalCs4399Mock() {
        strncpy(_descriptor.compatible, "cirrus,cs4399", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "CS4399", 32);
        _descriptor.name[32] = '\0';
        strncpy(_descriptor.manufacturer, "Cirrus Logic", 32);
        _descriptor.manufacturer[32] = '\0';
        _descriptor.type         = HAL_DEV_DAC;
        _descriptor.i2cAddr      = kI2cAddr;
        _descriptor.channelCount = 2;
        _descriptor.bus.type     = HAL_BUS_I2C;
        _descriptor.bus.index    = HAL_I2C_BUS_EXP;
        _descriptor.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K |
                                      HAL_RATE_192K | HAL_RATE_384K;
        _descriptor.capabilities =
            HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE |
            HAL_CAP_FILTERS  | HAL_CAP_DSD;
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
        // CS4399 supports up to 384kHz — NO 768kHz
        if (rate != 44100 && rate != 48000 && rate != 96000 &&
            rate != 192000 && rate != 384000) return false;
        if (bits != 16 && bits != 24 && bits != 32) return false;
        _sampleRate = rate;
        _bitDepth   = bits;
        // NOS mode persists across reconfigure
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
        if (preset >= 5) return false;   // CS4399 has 5 presets (0-4)
        _filterPreset = preset;
        return true;
    }

    // ----- NOS mode -----

    bool setNosMode(bool enabled) {
        _nosEnabled = enabled;
        return true;
    }

    bool isNosMode() const {
        return _nosEnabled;
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
static HalCs4399Mock* dac;

void setUp(void) {
    WireMock::reset();
    dac = new HalCs4399Mock();
}

void tearDown(void) {
    delete dac;
    dac = nullptr;
}

// ==========================================================================
// Section 1: Descriptor tests (7 tests)
// ==========================================================================

// ----- 1. descriptor compatible string is "cirrus,cs4399" -----
void test_descriptor_compatible_string(void) {
    TEST_ASSERT_EQUAL_STRING("cirrus,cs4399", dac->getDescriptor().compatible);
}

// ----- 2. descriptor type is HAL_DEV_DAC -----
void test_descriptor_type_is_dac(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, (int)dac->getDescriptor().type);
}

// ----- 3. capabilities include HAL_CAP_DAC_PATH -----
void test_descriptor_capabilities_include_dac_path(void) {
    uint32_t caps = dac->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
}

// ----- 4. capabilities include HAL_CAP_HW_VOLUME -----
void test_descriptor_capabilities_include_hw_volume(void) {
    uint32_t caps = dac->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
}

// ----- 5. capabilities exclude HAL_CAP_ADC_PATH -----
void test_descriptor_capabilities_exclude_adc_path(void) {
    uint32_t caps = dac->getDescriptor().capabilities;
    TEST_ASSERT_FALSE(caps & HAL_CAP_ADC_PATH);
}

// ----- 6. channel count is 2 (stereo) -----
void test_descriptor_channel_count(void) {
    TEST_ASSERT_EQUAL(2, dac->getDescriptor().channelCount);
}

// ----- 7. sample rate mask includes 44K1-384K but excludes 768K -----
void test_descriptor_sample_rate_mask(void) {
    uint32_t mask = dac->getDescriptor().sampleRatesMask;
    TEST_ASSERT_TRUE(mask & HAL_RATE_44K1);
    TEST_ASSERT_TRUE(mask & HAL_RATE_48K);
    TEST_ASSERT_TRUE(mask & HAL_RATE_96K);
    TEST_ASSERT_TRUE(mask & HAL_RATE_192K);
    TEST_ASSERT_TRUE(mask & HAL_RATE_384K);
    TEST_ASSERT_FALSE(mask & HAL_RATE_768K);
}

// ==========================================================================
// Section 2: Lifecycle tests (6 tests)
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
// Section 3: Volume tests (4 tests)
// ==========================================================================

// ----- 14. setVolume(0) stores 0 (full attenuation) -----
void test_volume_0_percent(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setVolume(0));
    TEST_ASSERT_EQUAL(0, dac->_volume);
}

// ----- 15. setVolume(50) stores 50 (mid-range) -----
void test_volume_50_percent(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setVolume(50));
    TEST_ASSERT_EQUAL(50, dac->_volume);
}

// ----- 16. setVolume(100) stores 100 (0 dB) -----
void test_volume_100_percent(void) {
    dac->init();
    dac->_volume = 0;
    TEST_ASSERT_TRUE(dac->setVolume(100));
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ----- 17. setVolume > 100 is clamped to 100 -----
void test_volume_clamp_above_100(void) {
    dac->init();
    dac->setVolume(150);
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ==========================================================================
// Section 4: Mute tests (2 tests)
// ==========================================================================

// ----- 18. setMute(true) enables mute -----
void test_mute_on(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setMute(true));
    TEST_ASSERT_TRUE(dac->_muted);
}

// ----- 19. mute defaults to false -----
void test_mute_default_off(void) {
    TEST_ASSERT_FALSE(dac->_muted);
}

// ==========================================================================
// Section 5: Filter preset tests (4 tests)
// ==========================================================================

// ----- 20. setFilterPreset(0) succeeds -----
void test_filter_preset_valid_0(void) {
    TEST_ASSERT_TRUE(dac->setFilterPreset(0));
    TEST_ASSERT_EQUAL(0, dac->_filterPreset);
}

// ----- 21. setFilterPreset(4) succeeds (max valid for CS4399) -----
void test_filter_preset_valid_4(void) {
    TEST_ASSERT_TRUE(dac->setFilterPreset(4));
    TEST_ASSERT_EQUAL(4, dac->_filterPreset);
}

// ----- 22. setFilterPreset(5) rejected (CS4399 has 5 presets, 0-4) -----
void test_filter_preset_invalid_5(void) {
    TEST_ASSERT_FALSE(dac->setFilterPreset(5));
    TEST_ASSERT_EQUAL(0, dac->_filterPreset);   // unchanged from default
}

// ----- 23. setFilterPreset(255) rejected -----
void test_filter_preset_invalid_255(void) {
    TEST_ASSERT_FALSE(dac->setFilterPreset(255));
    TEST_ASSERT_EQUAL(0, dac->_filterPreset);   // unchanged from default
}

// ==========================================================================
// Section 6: Sample rate tests (7 tests)
// ==========================================================================

// ----- 24. configure(44100, 32) succeeds -----
void test_configure_44100(void) {
    TEST_ASSERT_TRUE(dac->configure(44100, 32));
    TEST_ASSERT_EQUAL(44100u, dac->_sampleRate);
}

// ----- 25. configure(48000, 32) succeeds -----
void test_configure_48000(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
}

// ----- 26. configure(96000, 32) succeeds -----
void test_configure_96000(void) {
    TEST_ASSERT_TRUE(dac->configure(96000, 32));
    TEST_ASSERT_EQUAL(96000u, dac->_sampleRate);
}

// ----- 27. configure(192000, 32) succeeds -----
void test_configure_192000(void) {
    TEST_ASSERT_TRUE(dac->configure(192000, 32));
    TEST_ASSERT_EQUAL(192000u, dac->_sampleRate);
}

// ----- 28. configure(384000, 32) succeeds -----
void test_configure_384000(void) {
    TEST_ASSERT_TRUE(dac->configure(384000, 32));
    TEST_ASSERT_EQUAL(384000u, dac->_sampleRate);
}

// ----- 29. configure(768000, 32) rejected (CS4399 max is 384kHz) -----
void test_configure_768000_rejected(void) {
    TEST_ASSERT_FALSE(dac->configure(768000, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);  // unchanged
}

// ----- 30. configure(22050, 32) rejected (unsupported rate) -----
void test_configure_unsupported_rate(void) {
    TEST_ASSERT_FALSE(dac->configure(22050, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);  // unchanged
}

// ==========================================================================
// Section 7: Bit depth tests (5 tests)
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

// ----- 34. configure(48000, 8) rejected -----
void test_configure_8bit_rejected(void) {
    TEST_ASSERT_FALSE(dac->configure(48000, 8));
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);  // unchanged
}

// ----- 35. sequential reconfigure updates rate and depth -----
void test_configure_sequential(void) {
    TEST_ASSERT_TRUE(dac->configure(96000, 24));
    TEST_ASSERT_EQUAL(96000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);

    TEST_ASSERT_TRUE(dac->configure(192000, 32));
    TEST_ASSERT_EQUAL(192000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);
}

// ==========================================================================
// Section 8: DAC interface delegation tests (7 tests)
// ==========================================================================

// ----- 36. dacSetVolume delegates to setVolume -----
void test_dac_set_volume(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->dacSetVolume(75));
    TEST_ASSERT_EQUAL(75, dac->dacGetVolume());
}

// ----- 37. dacSetMute(true) sets muted state -----
void test_dac_set_mute(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->dacSetMute(true));
    TEST_ASSERT_TRUE(dac->dacIsMuted());
}

// ----- 38. dacSetSampleRate delegates to configure -----
void test_dac_set_sample_rate(void) {
    TEST_ASSERT_TRUE(dac->dacSetSampleRate(96000));
    TEST_ASSERT_EQUAL(96000u, dac->dacGetSampleRate());
}

// ----- 39. dacSetSampleRate(768000) rejected (CS4399 max 384kHz) -----
void test_dac_set_sample_rate_invalid(void) {
    TEST_ASSERT_FALSE(dac->dacSetSampleRate(768000));
    TEST_ASSERT_EQUAL(48000u, dac->dacGetSampleRate());  // unchanged
}

// ----- 40. dacSetBitDepth(24) succeeds -----
void test_dac_set_bit_depth(void) {
    TEST_ASSERT_TRUE(dac->dacSetBitDepth(24));
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);
}

// ----- 41. dacGetVolume returns default 100 -----
void test_dac_get_volume_default(void) {
    TEST_ASSERT_EQUAL(100, dac->dacGetVolume());
}

// ----- 42. dacIsMuted returns default false -----
void test_dac_is_muted_default(void) {
    TEST_ASSERT_FALSE(dac->dacIsMuted());
}

// ==========================================================================
// Section 9: buildSink tests (6 tests)
// ==========================================================================

// ----- 43. buildSink(0, &out) returns true -----
void test_build_sink_success(void) {
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_TRUE(dac->buildSink(0, &out));
}

// ----- 44. buildSink populates name from descriptor -----
void test_build_sink_populates_name(void) {
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    dac->buildSink(0, &out);
    TEST_ASSERT_NOT_NULL(out.name);
    TEST_ASSERT_EQUAL_STRING("CS4399", out.name);
}

// ----- 45. buildSink populates channelCount = 2 -----
void test_build_sink_populates_channels(void) {
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
// Section 10: NOS Mode tests (4 tests)
// ==========================================================================

// ----- 49. setNosMode(true) enables NOS mode -----
void test_nos_mode_enable(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setNosMode(true));
    TEST_ASSERT_TRUE(dac->isNosMode());
}

// ----- 50. setNosMode(false) disables NOS mode after enabling -----
void test_nos_mode_disable(void) {
    dac->init();
    dac->setNosMode(true);
    TEST_ASSERT_TRUE(dac->isNosMode());
    dac->setNosMode(false);
    TEST_ASSERT_FALSE(dac->isNosMode());
}

// ----- 51. NOS mode defaults to disabled after init -----
void test_nos_mode_default_disabled(void) {
    dac->init();
    TEST_ASSERT_FALSE(dac->isNosMode());
}

// ----- 52. NOS mode survives reconfigure (rate/bit depth change) -----
void test_nos_mode_survives_reconfigure(void) {
    dac->init();
    dac->setNosMode(true);
    TEST_ASSERT_TRUE(dac->isNosMode());

    // Change sample rate and bit depth
    TEST_ASSERT_TRUE(dac->configure(96000, 24));

    // NOS mode must persist across reconfigure
    TEST_ASSERT_TRUE(dac->isNosMode());
    TEST_ASSERT_EQUAL(96000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);
}

// ==========================================================================
// Main
// ==========================================================================

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    // Section 1: Descriptor (7 tests)
    RUN_TEST(test_descriptor_compatible_string);
    RUN_TEST(test_descriptor_type_is_dac);
    RUN_TEST(test_descriptor_capabilities_include_dac_path);
    RUN_TEST(test_descriptor_capabilities_include_hw_volume);
    RUN_TEST(test_descriptor_capabilities_exclude_adc_path);
    RUN_TEST(test_descriptor_channel_count);
    RUN_TEST(test_descriptor_sample_rate_mask);

    // Section 2: Lifecycle (6 tests)
    RUN_TEST(test_probe_returns_true);
    RUN_TEST(test_init_success);
    RUN_TEST(test_deinit_clears_ready);
    RUN_TEST(test_health_check_after_init);
    RUN_TEST(test_health_check_before_init);
    RUN_TEST(test_health_check_after_deinit);

    // Section 3: Volume (4 tests)
    RUN_TEST(test_volume_0_percent);
    RUN_TEST(test_volume_50_percent);
    RUN_TEST(test_volume_100_percent);
    RUN_TEST(test_volume_clamp_above_100);

    // Section 4: Mute (2 tests)
    RUN_TEST(test_mute_on);
    RUN_TEST(test_mute_default_off);

    // Section 5: Filter preset (4 tests)
    RUN_TEST(test_filter_preset_valid_0);
    RUN_TEST(test_filter_preset_valid_4);
    RUN_TEST(test_filter_preset_invalid_5);
    RUN_TEST(test_filter_preset_invalid_255);

    // Section 6: Sample rate (7 tests)
    RUN_TEST(test_configure_44100);
    RUN_TEST(test_configure_48000);
    RUN_TEST(test_configure_96000);
    RUN_TEST(test_configure_192000);
    RUN_TEST(test_configure_384000);
    RUN_TEST(test_configure_768000_rejected);
    RUN_TEST(test_configure_unsupported_rate);

    // Section 7: Bit depth (5 tests)
    RUN_TEST(test_configure_16bit);
    RUN_TEST(test_configure_24bit);
    RUN_TEST(test_configure_32bit);
    RUN_TEST(test_configure_8bit_rejected);
    RUN_TEST(test_configure_sequential);

    // Section 8: DAC interface delegation (7 tests)
    RUN_TEST(test_dac_set_volume);
    RUN_TEST(test_dac_set_mute);
    RUN_TEST(test_dac_set_sample_rate);
    RUN_TEST(test_dac_set_sample_rate_invalid);
    RUN_TEST(test_dac_set_bit_depth);
    RUN_TEST(test_dac_get_volume_default);
    RUN_TEST(test_dac_is_muted_default);

    // Section 9: buildSink (6 tests)
    RUN_TEST(test_build_sink_success);
    RUN_TEST(test_build_sink_populates_name);
    RUN_TEST(test_build_sink_populates_channels);
    RUN_TEST(test_build_sink_null_out);
    RUN_TEST(test_build_sink_slot_overflow);
    RUN_TEST(test_has_hardware_volume);

    // Section 10: NOS Mode (4 tests)
    RUN_TEST(test_nos_mode_enable);
    RUN_TEST(test_nos_mode_disable);
    RUN_TEST(test_nos_mode_default_disabled);
    RUN_TEST(test_nos_mode_survives_reconfigure);

    return UNITY_END();
}
