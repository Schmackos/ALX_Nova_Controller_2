// test_hal_es9038pro.cpp
// Tests a mock HalEs9038pro class modelling the ESS ES9038PRO 8-channel I2C DAC.
//
// The ES9038PRO is ESS's 8-channel HyperStream II flagship DAC:
//   - I2C address 0x48 (configurable via address pins)
//   - 8-channel (TDM) DAC, exposed as 4 stereo sink pairs
//   - 132dB DNR, PCM up to 768kHz, DSD512
//   - 8-bit per-channel hardware volume attenuation (0.5dB per step)
//   - 8 digital filter presets (0-7); mute via bit5 of filter/mute register
//   - getSinkCount() returns 4 after init; buildSinkAt(0-3) returns each stereo pair
//
// Key behaviours:
//   - probe() returns true (NATIVE_TEST stub)
//   - init()  succeeds, sets _ready=true, _state=AVAILABLE, _sinksBuilt=true
//   - setVolume(): 0-100% scaling (100%=0x00 attenuation, 0%=0xFF attenuation)
//   - setMute(): toggles _muted state
//   - setFilterPreset(): accepts 0-7; rejects >= 8
//   - configure(): accepts 44100/48000/96000/192000/384000/768000 with 16/24/32 bit
//   - getSinkCount() = 4 after init, 0 before init
//   - buildSinkAt(idx, slot, out): returns true for idx 0-3, false for idx >= 4 or null out
//   - type = HAL_DEV_DAC, channelCount = 8, capabilities include HAL_CAP_DAC_PATH
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

// ===== Mock ES9038PRO 8-channel DAC =====
//
// Models the ESS ES9038PRO 8ch DAC. Volume is inverted attenuation.
// Exposes 4 stereo sink pairs via getSinkCount()/buildSinkAt().
//
class HalEs9038ProMock : public HalAudioDevice, public HalAudioDacInterface {
public:
    static const uint8_t kI2cAddr = 0x48;

    uint8_t  _filterPreset = 0;
    bool     _muted        = false;
    uint8_t  _volume       = 100;
    uint32_t _sampleRate   = 48000;
    uint8_t  _bitDepth     = 32;
    bool     _initialized  = false;
    bool     _sinksBuilt   = false;

    HalEs9038ProMock() {
        memset(&_descriptor, 0, sizeof(_descriptor));
        strncpy(_descriptor.compatible, "ess,es9038pro", 31);
        strncpy(_descriptor.name, "ES9038PRO", 32);
        strncpy(_descriptor.manufacturer, "ESS Technology", 32);
        _descriptor.type         = HAL_DEV_DAC;
        _descriptor.i2cAddr      = kI2cAddr;
        _descriptor.channelCount = 8;
        _descriptor.bus.type     = HAL_BUS_I2C;
        _descriptor.bus.index    = HAL_I2C_BUS_EXP;
        _descriptor.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K |
                                      HAL_RATE_192K  | HAL_RATE_384K | HAL_RATE_768K;
        _descriptor.capabilities =
            HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    bool probe() override { return true; }

    HalInitResult init() override {
        _initialized = true;
        _sinksBuilt  = true;
        _state = HAL_STATE_AVAILABLE;
        _ready = true;
        return hal_init_ok();
    }

    void deinit() override {
        _ready       = false;
        _initialized = false;
        _sinksBuilt  = false;
        _state = HAL_STATE_REMOVED;
    }

    void dumpConfig() override {}
    bool healthCheck() override { return _initialized; }

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

    bool setFilterPreset(uint8_t preset) {
        if (preset >= 8) return false;
        _filterPreset = preset;
        return true;
    }

    // HalAudioDacInterface
    bool    dacSetVolume(uint8_t pct) override { return setVolume(pct); }
    bool    dacSetMute(bool m) override        { return setMute(m); }
    uint8_t dacGetVolume() const override      { return _volume; }
    bool    dacIsMuted() const override        { return _muted; }
    bool    dacSetSampleRate(uint32_t hz) override { return configure(hz, _bitDepth); }
    bool    dacSetBitDepth(uint8_t bits) override  { return configure(_sampleRate, bits); }
    uint32_t dacGetSampleRate() const override { return _sampleRate; }

    // Multi-sink
    int getSinkCount() const override { return _sinksBuilt ? 4 : 0; }

    bool buildSinkAt(int idx, uint8_t sinkSlot, AudioOutputSink* out) override {
        if (!out) return false;
        if (idx < 0 || idx >= 4 || !_sinksBuilt) return false;
        memset(out, 0, sizeof(AudioOutputSink));
        static const char* names[4] = {
            "ES9038PRO CH1/2", "ES9038PRO CH3/4",
            "ES9038PRO CH5/6", "ES9038PRO CH7/8"
        };
        out->name         = names[idx];
        out->firstChannel = (uint8_t)(idx * 2);
        out->channelCount = 2;
        out->halSlot      = _slot;
        (void)sinkSlot;
        return true;
    }

    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override {
        return buildSinkAt(0, sinkSlot, out);
    }

    bool hasHardwareVolume() const override { return true; }
};

// ===== Fixtures =====
static HalEs9038ProMock* dac;

void setUp(void) {
    WireMock::reset();
    dac = new HalEs9038ProMock();
}

void tearDown(void) {
    delete dac;
    dac = nullptr;
}

// ==========================================================================
// Section 1: Descriptor tests
// ==========================================================================

void test_descriptor_compatible_string(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9038pro", dac->getDescriptor().compatible);
}

void test_descriptor_type_is_dac(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, (int)dac->getDescriptor().type);
}

void test_descriptor_capabilities(void) {
    uint16_t caps = dac->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(caps & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(caps & HAL_CAP_FILTERS);
    TEST_ASSERT_FALSE(caps & HAL_CAP_ADC_PATH);
}

void test_descriptor_channel_count(void) {
    TEST_ASSERT_EQUAL(8, dac->getDescriptor().channelCount);
}

void test_descriptor_bus_type(void) {
    TEST_ASSERT_EQUAL(HAL_BUS_I2C, (int)dac->getDescriptor().bus.type);
}

void test_descriptor_i2c_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0x48, dac->getDescriptor().i2cAddr);
}

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

void test_probe_returns_true(void) {
    TEST_ASSERT_TRUE(dac->probe());
}

void test_init_success(void) {
    HalInitResult res = dac->init();
    TEST_ASSERT_TRUE(res.success);
    TEST_ASSERT_TRUE(dac->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)dac->_state);
}

void test_deinit_clears_ready(void) {
    dac->init();
    dac->deinit();
    TEST_ASSERT_FALSE(dac->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)dac->_state);
}

void test_health_check_after_init(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->healthCheck());
}

void test_health_check_before_init(void) {
    TEST_ASSERT_FALSE(dac->healthCheck());
}

void test_health_check_after_deinit(void) {
    dac->init();
    dac->deinit();
    TEST_ASSERT_FALSE(dac->healthCheck());
}

// ==========================================================================
// Section 3: Volume tests
// ==========================================================================

void test_volume_0_percent(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setVolume(0));
    TEST_ASSERT_EQUAL(0, dac->_volume);
}

void test_volume_100_percent(void) {
    dac->init();
    dac->_volume = 0;
    TEST_ASSERT_TRUE(dac->setVolume(100));
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

void test_volume_50_percent(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setVolume(50));
    TEST_ASSERT_EQUAL(50, dac->_volume);
}

void test_volume_clamp_above_100(void) {
    dac->init();
    dac->setVolume(150);
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ==========================================================================
// Section 4: Mute tests
// ==========================================================================

void test_mute_on_off(void) {
    dac->init();
    TEST_ASSERT_FALSE(dac->_muted);
    TEST_ASSERT_TRUE(dac->setMute(true));
    TEST_ASSERT_TRUE(dac->_muted);
    TEST_ASSERT_TRUE(dac->setMute(false));
    TEST_ASSERT_FALSE(dac->_muted);
}

void test_mute_default_false(void) {
    TEST_ASSERT_FALSE(dac->_muted);
}

// ==========================================================================
// Section 5: Filter preset tests
// ==========================================================================

void test_filter_preset_valid(void) {
    for (uint8_t i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(dac->setFilterPreset(i));
        TEST_ASSERT_EQUAL(i, dac->_filterPreset);
    }
}

void test_filter_preset_8_invalid(void) {
    TEST_ASSERT_FALSE(dac->setFilterPreset(8));
    TEST_ASSERT_EQUAL(0, dac->_filterPreset);
}

void test_filter_preset_255_invalid(void) {
    TEST_ASSERT_FALSE(dac->setFilterPreset(255));
    TEST_ASSERT_EQUAL(0, dac->_filterPreset);
}

void test_filter_preset_default_zero(void) {
    TEST_ASSERT_EQUAL(0, dac->_filterPreset);
}

// ==========================================================================
// Section 6: Sample rate tests
// ==========================================================================

void test_sample_rate_44k1(void) {
    TEST_ASSERT_TRUE(dac->configure(44100, 32));
    TEST_ASSERT_EQUAL(44100u, dac->_sampleRate);
}

void test_sample_rate_48k(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
}

void test_sample_rate_96k(void) {
    TEST_ASSERT_TRUE(dac->configure(96000, 32));
    TEST_ASSERT_EQUAL(96000u, dac->_sampleRate);
}

void test_sample_rate_192k(void) {
    TEST_ASSERT_TRUE(dac->configure(192000, 32));
    TEST_ASSERT_EQUAL(192000u, dac->_sampleRate);
}

void test_sample_rate_384k(void) {
    TEST_ASSERT_TRUE(dac->configure(384000, 32));
    TEST_ASSERT_EQUAL(384000u, dac->_sampleRate);
}

void test_sample_rate_768k(void) {
    TEST_ASSERT_TRUE(dac->configure(768000, 32));
    TEST_ASSERT_EQUAL(768000u, dac->_sampleRate);
}

void test_sample_rate_unsupported(void) {
    TEST_ASSERT_FALSE(dac->configure(8000, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
}

// ==========================================================================
// Section 7: Bit depth tests
// ==========================================================================

void test_configure_16bit(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 16));
    TEST_ASSERT_EQUAL(16, dac->_bitDepth);
}

void test_configure_24bit(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 24));
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);
}

void test_configure_32bit(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 32));
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);
}

void test_configure_unsupported_bits(void) {
    TEST_ASSERT_FALSE(dac->configure(48000, 8));
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);
}

// ==========================================================================
// Section 8: DAC interface delegation tests
// ==========================================================================

void test_dac_set_volume(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->dacSetVolume(75));
    TEST_ASSERT_EQUAL(75, dac->dacGetVolume());
}

void test_dac_get_volume_initial(void) {
    TEST_ASSERT_EQUAL(100, dac->dacGetVolume());
}

void test_dac_set_mute_true(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->dacSetMute(true));
    TEST_ASSERT_TRUE(dac->dacIsMuted());
}

void test_dac_set_mute_false(void) {
    dac->init();
    dac->dacSetMute(true);
    TEST_ASSERT_TRUE(dac->dacSetMute(false));
    TEST_ASSERT_FALSE(dac->dacIsMuted());
}

void test_dac_is_muted_default(void) {
    TEST_ASSERT_FALSE(dac->dacIsMuted());
}

void test_dac_set_sample_rate(void) {
    TEST_ASSERT_TRUE(dac->dacSetSampleRate(96000));
    TEST_ASSERT_EQUAL(96000u, dac->dacGetSampleRate());
}

void test_dac_get_sample_rate_initial(void) {
    TEST_ASSERT_EQUAL(48000u, dac->dacGetSampleRate());
}

// ==========================================================================
// Section 9: Multi-sink tests (8ch TDM — 4 stereo pairs)
// ==========================================================================

void test_get_sink_count_zero_before_init(void) {
    TEST_ASSERT_EQUAL(0, dac->getSinkCount());
}

void test_get_sink_count_four_after_init(void) {
    dac->init();
    TEST_ASSERT_EQUAL(4, dac->getSinkCount());
}

void test_get_sink_count_zero_after_deinit(void) {
    dac->init();
    dac->deinit();
    TEST_ASSERT_EQUAL(0, dac->getSinkCount());
}

void test_build_sink_at_0_returns_true(void) {
    dac->init();
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_TRUE(dac->buildSinkAt(0, 0, &out));
}

void test_build_sink_at_3_returns_true(void) {
    dac->init();
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_TRUE(dac->buildSinkAt(3, 3, &out));
}

void test_build_sink_at_4_returns_false(void) {
    dac->init();
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_FALSE(dac->buildSinkAt(4, 4, &out));
}

void test_build_sink_at_null_returns_false(void) {
    dac->init();
    TEST_ASSERT_FALSE(dac->buildSinkAt(0, 0, nullptr));
}

void test_build_sink_at_before_init_returns_false(void) {
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_FALSE(dac->buildSinkAt(0, 0, &out));
}

void test_build_sink_at_channel_count_is_2(void) {
    dac->init();
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    dac->buildSinkAt(0, 0, &out);
    TEST_ASSERT_EQUAL(2, out.channelCount);
}

void test_build_sink_at_names_are_correct(void) {
    dac->init();
    AudioOutputSink out;
    const char* expected[4] = {
        "ES9038PRO CH1/2", "ES9038PRO CH3/4",
        "ES9038PRO CH5/6", "ES9038PRO CH7/8"
    };
    for (int i = 0; i < 4; i++) {
        memset(&out, 0, sizeof(out));
        TEST_ASSERT_TRUE(dac->buildSinkAt(i, (uint8_t)i, &out));
        TEST_ASSERT_NOT_NULL(out.name);
        TEST_ASSERT_EQUAL_STRING(expected[i], out.name);
    }
}

void test_build_sink_at_first_channel_is_correct(void) {
    dac->init();
    AudioOutputSink out;
    for (int i = 0; i < 4; i++) {
        memset(&out, 0, sizeof(out));
        dac->buildSinkAt(i, (uint8_t)i, &out);
        TEST_ASSERT_EQUAL((uint8_t)(i * 2), out.firstChannel);
    }
}

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

    // Section 7: Bit depth
    RUN_TEST(test_configure_16bit);
    RUN_TEST(test_configure_24bit);
    RUN_TEST(test_configure_32bit);
    RUN_TEST(test_configure_unsupported_bits);

    // Section 8: DAC interface
    RUN_TEST(test_dac_set_volume);
    RUN_TEST(test_dac_get_volume_initial);
    RUN_TEST(test_dac_set_mute_true);
    RUN_TEST(test_dac_set_mute_false);
    RUN_TEST(test_dac_is_muted_default);
    RUN_TEST(test_dac_set_sample_rate);
    RUN_TEST(test_dac_get_sample_rate_initial);

    // Section 9: Multi-sink
    RUN_TEST(test_get_sink_count_zero_before_init);
    RUN_TEST(test_get_sink_count_four_after_init);
    RUN_TEST(test_get_sink_count_zero_after_deinit);
    RUN_TEST(test_build_sink_at_0_returns_true);
    RUN_TEST(test_build_sink_at_3_returns_true);
    RUN_TEST(test_build_sink_at_4_returns_false);
    RUN_TEST(test_build_sink_at_null_returns_false);
    RUN_TEST(test_build_sink_at_before_init_returns_false);
    RUN_TEST(test_build_sink_at_channel_count_is_2);
    RUN_TEST(test_build_sink_at_names_are_correct);
    RUN_TEST(test_build_sink_at_first_channel_is_correct);
    RUN_TEST(test_has_hardware_volume);

    return UNITY_END();
}
