// test_hal_es9039pro.cpp
// Tests a mock HalEs9039pro class modelling the ESS ES9039PRO/ES9039MPRO 8-channel DAC.
//
// The ES9039PRO/MPRO is ESS's HyperStream IV 8-channel DAC:
//   - I2C address 0x48
//   - 8-channel (TDM) DAC, exposed as 4 stereo sink pairs
//   - 132dB DNR, PCM up to 768kHz
//   - Variant auto-detection: chip ID 0x39=PRO, 0x3A=MPRO
//   - MPRO updates descriptor name/compatible in-place at init
//   - getSinkCount() returns 4 after init; buildSinkAt(0-3) returns each stereo pair
//   - Sink names reflect detected variant: "ES9039PRO CH1/2" vs "ES9039MPRO CH1/2"

#include <unity.h>
#include <cstring>
#include <cstdio>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Wire.h"
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_audio_device.h"
#include "../../src/hal/hal_audio_interfaces.h"
#include "../../src/audio_output_sink.h"

// ===== Mock ES9039PRO/MPRO 8-channel DAC =====
//
// Simulates variant detection: if _forceChipId == 0x3A, init() treats the device
// as ES9039MPRO and updates descriptor + sink names accordingly.

class HalEs9039ProMock : public HalAudioDevice, public HalAudioDacInterface {
public:
    static const uint8_t kChipIdPro  = 0x39;
    static const uint8_t kChipIdMpro = 0x3A;
    static const uint8_t kI2cAddr    = 0x48;

    uint8_t  _filterPreset = 0;
    bool     _muted        = false;
    uint8_t  _volume       = 100;
    uint32_t _sampleRate   = 48000;
    uint8_t  _bitDepth     = 32;
    bool     _initialized  = false;
    bool     _sinksBuilt   = false;
    bool     _isMpro       = false;

    // Stored sink names (member arrays so pointers stay valid)
    char _sinkName0[32];
    char _sinkName1[32];
    char _sinkName2[32];
    char _sinkName3[32];

    // Controls which chip ID is "read" during init()
    uint8_t _forceChipId = kChipIdPro;

    HalEs9039ProMock() {
        memset(&_descriptor, 0, sizeof(_descriptor));
        strncpy(_descriptor.compatible, "ess,es9039pro", 31);
        strncpy(_descriptor.name, "ES9039PRO", 32);
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
        memset(_sinkName0, 0, sizeof(_sinkName0));
        memset(_sinkName1, 0, sizeof(_sinkName1));
        memset(_sinkName2, 0, sizeof(_sinkName2));
        memset(_sinkName3, 0, sizeof(_sinkName3));
    }

    bool probe() override { return true; }

    HalInitResult init() override {
        // Simulate variant detection
        if (_forceChipId == kChipIdMpro) {
            _isMpro = true;
            strncpy(_descriptor.name, "ES9039MPRO", 32);
            strncpy(_descriptor.compatible, "ess,es9039mpro", 31);
        } else {
            _isMpro = false;
        }
        const char* baseName = _isMpro ? "ES9039MPRO" : "ES9039PRO";
        snprintf(_sinkName0, sizeof(_sinkName0), "%s CH1/2", baseName);
        snprintf(_sinkName1, sizeof(_sinkName1), "%s CH3/4", baseName);
        snprintf(_sinkName2, sizeof(_sinkName2), "%s CH5/6", baseName);
        snprintf(_sinkName3, sizeof(_sinkName3), "%s CH7/8", baseName);

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
        const char* names[4] = { _sinkName0, _sinkName1, _sinkName2, _sinkName3 };
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
static HalEs9039ProMock* dac;

void setUp(void) {
    WireMock::reset();
    dac = new HalEs9039ProMock();
}

void tearDown(void) {
    delete dac;
    dac = nullptr;
}

// ==========================================================================
// Section 1: Descriptor tests (PRO variant, default)
// ==========================================================================

void test_descriptor_compatible_string_pro(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9039pro", dac->getDescriptor().compatible);
}

void test_descriptor_name_pro(void) {
    TEST_ASSERT_EQUAL_STRING("ES9039PRO", dac->getDescriptor().name);
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
// Section 2: Variant detection — PRO
// ==========================================================================

void test_pro_variant_init_not_mpro(void) {
    dac->_forceChipId = HalEs9039ProMock::kChipIdPro;
    dac->init();
    TEST_ASSERT_FALSE(dac->_isMpro);
}

void test_pro_variant_compatible_unchanged(void) {
    dac->_forceChipId = HalEs9039ProMock::kChipIdPro;
    dac->init();
    TEST_ASSERT_EQUAL_STRING("ess,es9039pro", dac->getDescriptor().compatible);
}

void test_pro_variant_name_unchanged(void) {
    dac->_forceChipId = HalEs9039ProMock::kChipIdPro;
    dac->init();
    TEST_ASSERT_EQUAL_STRING("ES9039PRO", dac->getDescriptor().name);
}

void test_pro_variant_sink_names(void) {
    dac->_forceChipId = HalEs9039ProMock::kChipIdPro;
    dac->init();
    const char* expected[4] = {
        "ES9039PRO CH1/2", "ES9039PRO CH3/4",
        "ES9039PRO CH5/6", "ES9039PRO CH7/8"
    };
    AudioOutputSink out;
    for (int i = 0; i < 4; i++) {
        memset(&out, 0, sizeof(out));
        TEST_ASSERT_TRUE(dac->buildSinkAt(i, (uint8_t)i, &out));
        TEST_ASSERT_EQUAL_STRING(expected[i], out.name);
    }
}

// ==========================================================================
// Section 3: Variant detection — MPRO
// ==========================================================================

void test_mpro_variant_init_is_mpro(void) {
    dac->_forceChipId = HalEs9039ProMock::kChipIdMpro;
    dac->init();
    TEST_ASSERT_TRUE(dac->_isMpro);
}

void test_mpro_variant_compatible_updated(void) {
    dac->_forceChipId = HalEs9039ProMock::kChipIdMpro;
    dac->init();
    TEST_ASSERT_EQUAL_STRING("ess,es9039mpro", dac->getDescriptor().compatible);
}

void test_mpro_variant_name_updated(void) {
    dac->_forceChipId = HalEs9039ProMock::kChipIdMpro;
    dac->init();
    TEST_ASSERT_EQUAL_STRING("ES9039MPRO", dac->getDescriptor().name);
}

void test_mpro_variant_sink_names(void) {
    dac->_forceChipId = HalEs9039ProMock::kChipIdMpro;
    dac->init();
    const char* expected[4] = {
        "ES9039MPRO CH1/2", "ES9039MPRO CH3/4",
        "ES9039MPRO CH5/6", "ES9039MPRO CH7/8"
    };
    AudioOutputSink out;
    for (int i = 0; i < 4; i++) {
        memset(&out, 0, sizeof(out));
        TEST_ASSERT_TRUE(dac->buildSinkAt(i, (uint8_t)i, &out));
        TEST_ASSERT_EQUAL_STRING(expected[i], out.name);
    }
}

// ==========================================================================
// Section 4: Lifecycle tests
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

// ==========================================================================
// Section 5: Volume tests
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

void test_volume_clamp_above_100(void) {
    dac->init();
    dac->setVolume(200);
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ==========================================================================
// Section 6: Mute tests
// ==========================================================================

void test_mute_on_off(void) {
    dac->init();
    TEST_ASSERT_FALSE(dac->_muted);
    TEST_ASSERT_TRUE(dac->setMute(true));
    TEST_ASSERT_TRUE(dac->_muted);
    TEST_ASSERT_TRUE(dac->setMute(false));
    TEST_ASSERT_FALSE(dac->_muted);
}

// ==========================================================================
// Section 7: Filter preset tests
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

// ==========================================================================
// Section 8: Sample rate tests
// ==========================================================================

void test_sample_rate_all_supported(void) {
    const uint32_t rates[] = { 44100, 48000, 96000, 192000, 384000, 768000 };
    for (int i = 0; i < 6; i++) {
        TEST_ASSERT_TRUE(dac->configure(rates[i], 32));
        TEST_ASSERT_EQUAL(rates[i], dac->_sampleRate);
    }
}

void test_sample_rate_unsupported(void) {
    TEST_ASSERT_FALSE(dac->configure(8000, 32));
}

// ==========================================================================
// Section 9: Multi-sink tests
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

    // Section 1: Descriptor (default PRO)
    RUN_TEST(test_descriptor_compatible_string_pro);
    RUN_TEST(test_descriptor_name_pro);
    RUN_TEST(test_descriptor_type_is_dac);
    RUN_TEST(test_descriptor_capabilities);
    RUN_TEST(test_descriptor_channel_count);
    RUN_TEST(test_descriptor_i2c_address);
    RUN_TEST(test_descriptor_sample_rate_mask);

    // Section 2: Variant detection PRO
    RUN_TEST(test_pro_variant_init_not_mpro);
    RUN_TEST(test_pro_variant_compatible_unchanged);
    RUN_TEST(test_pro_variant_name_unchanged);
    RUN_TEST(test_pro_variant_sink_names);

    // Section 3: Variant detection MPRO
    RUN_TEST(test_mpro_variant_init_is_mpro);
    RUN_TEST(test_mpro_variant_compatible_updated);
    RUN_TEST(test_mpro_variant_name_updated);
    RUN_TEST(test_mpro_variant_sink_names);

    // Section 4: Lifecycle
    RUN_TEST(test_probe_returns_true);
    RUN_TEST(test_init_success);
    RUN_TEST(test_deinit_clears_ready);
    RUN_TEST(test_health_check_after_init);
    RUN_TEST(test_health_check_before_init);

    // Section 5: Volume
    RUN_TEST(test_volume_0_percent);
    RUN_TEST(test_volume_100_percent);
    RUN_TEST(test_volume_clamp_above_100);

    // Section 6: Mute
    RUN_TEST(test_mute_on_off);

    // Section 7: Filter preset
    RUN_TEST(test_filter_preset_valid);
    RUN_TEST(test_filter_preset_8_invalid);

    // Section 8: Sample rate
    RUN_TEST(test_sample_rate_all_supported);
    RUN_TEST(test_sample_rate_unsupported);

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
    RUN_TEST(test_build_sink_at_first_channel_is_correct);
    RUN_TEST(test_has_hardware_volume);

    return UNITY_END();
}
