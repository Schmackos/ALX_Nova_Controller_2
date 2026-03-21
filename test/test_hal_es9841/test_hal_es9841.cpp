// test_hal_es9841.cpp
// Tests a mock HalEs9841 class modelling the ESS ES9841 I2C ADC.
//
// The ES9841 is an ESS SABRE 4-channel ADC (122dB DNR):
//   - I2C address 0x40 (configurable via address pins)
//   - 4-channel ADC with TDM output
//   - Chip ID: 0x91 (register 0xE1)
//   - Supported sample rates: 44.1kHz, 48kHz, 96kHz, 192kHz
//   - 8-bit per-channel volume (0xFF=0dB, 0x00=mute) — differs from ES9842PRO/ES9840
//   - 3-bit gain per channel packed two-per-register (0-42 dB, 6 dB steps)
//   - Per-channel HPF (DC blocking) at same offsets as ES9842PRO
//   - Global filter preset (0-7, single register 0x4A bits[7:5])
//   - Exposes 2 AudioInputSources: CH1/2 (pair A) and CH3/4 (pair B)
//
// Key differences vs ES9842PRO:
//   - Volume encoding: 8-bit (0xFF=0dB, 0x00=mute) vs 16-bit (0x7FFF=0dB, 0x0000=mute)
//   - Gain range: 0-42 dB (3-bit, 7 steps) vs 0-18 dB (2-bit, 3 steps)
//   - Filter: single global register vs per-channel registers
//   - setVolume(percent): 100%->0xFF, 0%->0x00 (linear, 0xFF is full scale)
//   - setMute(true): write 0x00 to all vol regs; setMute(false): restore saved volume

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

#ifndef HAL_CAP_PGA_CONTROL
#define HAL_CAP_PGA_CONTROL  (1 << 5)
#endif
#ifndef HAL_CAP_HPF_CONTROL
#define HAL_CAP_HPF_CONTROL  (1 << 6)
#endif

// ===== Mock ES9841 ADC device =====
//
// Models the ESS ES9841 4-channel ADC.
// Volume is 8-bit: 0xFF=0dB (100%), 0x00=mute (0%). Linear mapping.
// Gain is 3-bit per channel (0-7 steps, 6 dB each = 0-42 dB).
// Filter preset: single global reg bits[7:5].
//
class HalEs9841Mock : public HalAudioDevice, public HalAudioAdcInterface {
public:
    static const uint8_t  kI2cAddr  = 0x40;
    static const uint8_t  kChipId   = 0x91;

    uint8_t  _gainDb       = 0;
    bool     _hpfEnabled   = true;
    uint32_t _sampleRate   = 48000;
    uint8_t  _bitDepth     = 32;
    uint8_t  _filterPreset = 0;
    bool     _muted        = false;
    uint8_t  _volume       = 100;        // percent
    uint8_t  _savedVol     = 0xFF;       // saved 8-bit vol for mute/unmute
    uint8_t  _chVolume[4]  = {0xFF, 0xFF, 0xFF, 0xFF};  // 8-bit per-channel (0xFF=0dB)

    AudioInputSource _inputSrc  = {};
    AudioInputSource _inputSrcB = {};
    bool             _inputSrcReady = false;

    HalEs9841Mock() {
        strncpy(_descriptor.compatible, "ess,es9841", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "ES9841", 32);
        _descriptor.name[32] = '\0';
        strncpy(_descriptor.manufacturer, "ESS Technology", 32);
        _descriptor.manufacturer[32] = '\0';
        _descriptor.type         = HAL_DEV_ADC;
        _descriptor.i2cAddr      = kI2cAddr;
        _descriptor.channelCount = 4;
        _descriptor.bus.type     = HAL_BUS_I2C;
        _descriptor.bus.index    = HAL_I2C_BUS_EXP;
        _descriptor.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        _descriptor.capabilities =
            HAL_CAP_ADC_PATH | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL | HAL_CAP_HW_VOLUME;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    bool probe() override { return true; }

    HalInitResult init() override {
        _state = HAL_STATE_AVAILABLE;
        _ready = true;

        memset(&_inputSrc, 0, sizeof(_inputSrc));
        _inputSrc.name          = "ES9841 CH1/2";
        _inputSrc.isHardwareAdc = true;
        _inputSrc.gainLinear    = 1.0f;
        _inputSrc.vuL           = -90.0f;
        _inputSrc.vuR           = -90.0f;
        _inputSrc.halSlot       = 0xFF;

        memset(&_inputSrcB, 0, sizeof(_inputSrcB));
        _inputSrcB.name          = "ES9841 CH3/4";
        _inputSrcB.isHardwareAdc = true;
        _inputSrcB.gainLinear    = 1.0f;
        _inputSrcB.vuL           = -90.0f;
        _inputSrcB.vuR           = -90.0f;
        _inputSrcB.halSlot       = 0xFF;

        _inputSrcReady = true;
        for (int i = 0; i < 4; i++) _chVolume[i] = 0xFF;
        _savedVol = 0xFF;

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
        return getInputSourceAt(0);
    }

    int getInputSourceCount() const override {
        return _inputSrcReady ? 2 : 0;
    }

    const AudioInputSource* getInputSourceAt(int idx) const override {
        if (!_inputSrcReady) return nullptr;
        if (idx == 0) return &_inputSrc;
        if (idx == 1) return &_inputSrcB;
        return nullptr;
    }

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
        // ES9841: 8-bit volume. 100%->0xFF (0dB), 0%->0x00 (mute).
        uint8_t vol8;
        if (pct == 0) {
            vol8 = 0x00;
        } else if (pct == 100) {
            vol8 = 0xFF;
        } else {
            vol8 = (uint8_t)(((uint32_t)pct * 0xFF) / 100);
        }
        _savedVol = vol8;
        for (int i = 0; i < 4; i++) _chVolume[i] = vol8;
        return true;
    }

    bool setMute(bool mute) override {
        _muted = mute;
        uint8_t vol8 = mute ? 0x00 : _savedVol;
        for (int i = 0; i < 4; i++) _chVolume[i] = vol8;
        return true;
    }

    bool adcSetGain(uint8_t dB) override {
        // 3-bit gain: max 42 dB
        if (dB > 42) dB = 42;
        uint8_t step = dB / 6;
        if (step > 7) step = 7;
        _gainDb = (uint8_t)(step * 6);
        return true;
    }

    bool adcSetHpfEnabled(bool en) override {
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

    // Global filter preset (single register)
    bool setFilterPreset(uint8_t preset) {
        if (preset >= 8) return false;
        _filterPreset = preset;
        return true;
    }

    // Per-channel 8-bit volume
    bool setChannelVolume(uint8_t ch, uint8_t vol8) {
        if (ch >= 4) return false;
        _chVolume[ch] = vol8;
        return true;
    }
};

static HalEs9841Mock* adc;

void setUp(void) {
    WireMock::reset();
    adc = new HalEs9841Mock();
}

void tearDown(void) {
    delete adc;
    adc = nullptr;
}

// ==========================================================================
// Section 1: Descriptor tests
// ==========================================================================

void test_descriptor_compatible_string(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9841", adc->getDescriptor().compatible);
}

void test_descriptor_type_is_adc(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_ADC, (int)adc->getDescriptor().type);
}

void test_descriptor_capabilities(void) {
    uint8_t caps = adc->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_PGA_CONTROL);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HPF_CONTROL);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_FALSE(caps & HAL_CAP_DAC_PATH);
}

void test_descriptor_channel_count(void) {
    TEST_ASSERT_EQUAL(4, adc->getDescriptor().channelCount);
}

void test_descriptor_bus_type(void) {
    TEST_ASSERT_EQUAL(HAL_BUS_I2C, (int)adc->getDescriptor().bus.type);
}

void test_descriptor_i2c_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0x40, adc->getDescriptor().i2cAddr);
}

void test_descriptor_chip_id_constant(void) {
    // ES9841 chip ID is 0x91
    TEST_ASSERT_EQUAL_HEX8(0x91, HalEs9841Mock::kChipId);
}

// ==========================================================================
// Section 2: Lifecycle tests
// ==========================================================================

void test_probe_returns_true(void) {
    TEST_ASSERT_TRUE(adc->probe());
}

void test_init_success(void) {
    HalInitResult res = adc->init();
    TEST_ASSERT_TRUE(res.success);
    TEST_ASSERT_TRUE(adc->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)adc->_state);
}

void test_deinit_clears_ready(void) {
    adc->init();
    adc->deinit();
    TEST_ASSERT_FALSE(adc->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)adc->_state);
}

void test_health_check_after_init(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->healthCheck());
}

void test_health_check_before_init(void) {
    TEST_ASSERT_FALSE(adc->healthCheck());
}

// ==========================================================================
// Section 3: Audio input source tests
// ==========================================================================

void test_get_input_source_after_init(void) {
    adc->init();
    TEST_ASSERT_NOT_NULL(adc->getInputSource());
}

void test_get_input_source_before_init(void) {
    TEST_ASSERT_NULL(adc->getInputSource());
}

void test_input_source_is_hardware_adc(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_TRUE(src->isHardwareAdc);
}

void test_input_source_name_pair_a(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL_STRING("ES9841 CH1/2", src->name);
}

void test_input_source_count(void) {
    TEST_ASSERT_EQUAL_INT(0, adc->getInputSourceCount());
    adc->init();
    TEST_ASSERT_EQUAL_INT(2, adc->getInputSourceCount());
}

void test_input_source_at_1_is_pair_b(void) {
    adc->init();
    const AudioInputSource* srcB = adc->getInputSourceAt(1);
    TEST_ASSERT_NOT_NULL(srcB);
    TEST_ASSERT_EQUAL_STRING("ES9841 CH3/4", srcB->name);
    TEST_ASSERT_TRUE(srcB->isHardwareAdc);
    TEST_ASSERT_EQUAL_FLOAT(-90.0f, srcB->vuL);
    TEST_ASSERT_EQUAL_FLOAT(-90.0f, srcB->vuR);
}

void test_input_source_at_out_of_range(void) {
    adc->init();
    TEST_ASSERT_NULL(adc->getInputSourceAt(2));
    TEST_ASSERT_NULL(adc->getInputSourceAt(-1));
}

void test_input_source_vu_initial_minus90(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL_FLOAT(-90.0f, src->vuL);
    TEST_ASSERT_EQUAL_FLOAT(-90.0f, src->vuR);
}

void test_input_source_gain_linear_unity(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, src->gainLinear);
}

void test_get_input_source_after_deinit(void) {
    adc->init();
    TEST_ASSERT_NOT_NULL(adc->getInputSource());
    adc->deinit();
    TEST_ASSERT_NULL(adc->getInputSource());
}

// ==========================================================================
// Section 4: Gain tests (3-bit, 6 dB steps, 0-42 dB max)
// ==========================================================================

void test_gain_set_0db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(0));
    TEST_ASSERT_EQUAL(0, adc->_gainDb);
}

void test_gain_set_6db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(6));
    TEST_ASSERT_EQUAL(6, adc->_gainDb);
}

void test_gain_set_12db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(12));
    TEST_ASSERT_EQUAL(12, adc->_gainDb);
}

void test_gain_set_18db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(18));
    TEST_ASSERT_EQUAL(18, adc->_gainDb);
}

void test_gain_set_24db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(24));
    TEST_ASSERT_EQUAL(24, adc->_gainDb);
}

void test_gain_set_30db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(30));
    TEST_ASSERT_EQUAL(30, adc->_gainDb);
}

void test_gain_set_36db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(36));
    TEST_ASSERT_EQUAL(36, adc->_gainDb);
}

void test_gain_set_42db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(42));
    TEST_ASSERT_EQUAL(42, adc->_gainDb);
}

void test_gain_clamp_above_42(void) {
    // ES9841: values above 42 clamped to 42 (3-bit max = 7 steps = 42 dB)
    TEST_ASSERT_TRUE(adc->adcSetGain(43));
    TEST_ASSERT_EQUAL(42, adc->_gainDb);
    TEST_ASSERT_TRUE(adc->adcSetGain(255));
    TEST_ASSERT_EQUAL(42, adc->_gainDb);
}

void test_gain_floor_to_step(void) {
    // 7 dB -> floor(7/6)*6 = 6 dB
    TEST_ASSERT_TRUE(adc->adcSetGain(7));
    TEST_ASSERT_EQUAL(6, adc->_gainDb);
    // 25 dB -> floor(25/6)*6 = 24 dB
    TEST_ASSERT_TRUE(adc->adcSetGain(25));
    TEST_ASSERT_EQUAL(24, adc->_gainDb);
    // 41 dB -> floor(41/6)*6 = 36 dB
    TEST_ASSERT_TRUE(adc->adcSetGain(41));
    TEST_ASSERT_EQUAL(36, adc->_gainDb);
}

// ==========================================================================
// Section 5: Volume tests (8-bit: 0%=0x00 mute, 100%=0xFF 0dB)
// ==========================================================================

void test_volume_0_percent_is_mute(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setVolume(0));
    TEST_ASSERT_EQUAL(0, adc->_volume);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x00, adc->_chVolume[i]);
    }
}

void test_volume_100_percent_is_0db(void) {
    adc->init();
    adc->setVolume(0);
    TEST_ASSERT_TRUE(adc->setVolume(100));
    TEST_ASSERT_EQUAL(100, adc->_volume);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, adc->_chVolume[i]);
    }
}

void test_volume_50_percent_midrange(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setVolume(50));
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_GREATER_THAN(0x00, adc->_chVolume[i]);
        TEST_ASSERT_LESS_THAN(0xFF, adc->_chVolume[i]);
    }
}

void test_volume_all_channels_equal(void) {
    adc->init();
    adc->setVolume(75);
    TEST_ASSERT_EQUAL(adc->_chVolume[0], adc->_chVolume[1]);
    TEST_ASSERT_EQUAL(adc->_chVolume[1], adc->_chVolume[2]);
    TEST_ASSERT_EQUAL(adc->_chVolume[2], adc->_chVolume[3]);
}

// ==========================================================================
// Section 6: Per-channel 8-bit volume tests
// ==========================================================================

void test_channel_volume_ch0_0db(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setChannelVolume(0, 0xFF));
    TEST_ASSERT_EQUAL_HEX8(0xFF, adc->_chVolume[0]);
}

void test_channel_volume_ch1_mute(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setChannelVolume(1, 0x00));
    TEST_ASSERT_EQUAL_HEX8(0x00, adc->_chVolume[1]);
}

void test_channel_volume_ch2_mid(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setChannelVolume(2, 0x80));
    TEST_ASSERT_EQUAL_HEX8(0x80, adc->_chVolume[2]);
}

void test_channel_volume_ch3_custom(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setChannelVolume(3, 0x40));
    TEST_ASSERT_EQUAL_HEX8(0x40, adc->_chVolume[3]);
}

void test_channel_volume_invalid_channel(void) {
    adc->init();
    TEST_ASSERT_FALSE(adc->setChannelVolume(4, 0xFF));
    TEST_ASSERT_FALSE(adc->setChannelVolume(255, 0xFF));
}

void test_channel_volume_independent(void) {
    adc->init();
    adc->setChannelVolume(0, 0xFF);
    adc->setChannelVolume(1, 0x80);
    adc->setChannelVolume(2, 0x40);
    adc->setChannelVolume(3, 0x00);
    TEST_ASSERT_EQUAL_HEX8(0xFF, adc->_chVolume[0]);
    TEST_ASSERT_EQUAL_HEX8(0x80, adc->_chVolume[1]);
    TEST_ASSERT_EQUAL_HEX8(0x40, adc->_chVolume[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, adc->_chVolume[3]);
}

// ==========================================================================
// Section 7: HPF tests
// ==========================================================================

void test_hpf_enable(void) {
    adc->_hpfEnabled = false;
    TEST_ASSERT_TRUE(adc->adcSetHpfEnabled(true));
    TEST_ASSERT_TRUE(adc->_hpfEnabled);
}

void test_hpf_disable(void) {
    TEST_ASSERT_TRUE(adc->_hpfEnabled);
    TEST_ASSERT_TRUE(adc->adcSetHpfEnabled(false));
    TEST_ASSERT_FALSE(adc->_hpfEnabled);
}

void test_hpf_toggle_round_trip(void) {
    TEST_ASSERT_TRUE(adc->adcSetHpfEnabled(false));
    TEST_ASSERT_FALSE(adc->_hpfEnabled);
    TEST_ASSERT_TRUE(adc->adcSetHpfEnabled(true));
    TEST_ASSERT_TRUE(adc->_hpfEnabled);
}

// ==========================================================================
// Section 8: Sample rate tests
// ==========================================================================

void test_sample_rate_44k1(void) {
    TEST_ASSERT_TRUE(adc->adcSetSampleRate(44100));
    TEST_ASSERT_EQUAL(44100u, adc->_sampleRate);
}

void test_sample_rate_48k(void) {
    TEST_ASSERT_TRUE(adc->adcSetSampleRate(48000));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
}

void test_sample_rate_96k(void) {
    TEST_ASSERT_TRUE(adc->adcSetSampleRate(96000));
    TEST_ASSERT_EQUAL(96000u, adc->_sampleRate);
}

void test_sample_rate_192k(void) {
    TEST_ASSERT_TRUE(adc->adcSetSampleRate(192000));
    TEST_ASSERT_EQUAL(192000u, adc->_sampleRate);
}

void test_sample_rate_8k_rejected(void) {
    TEST_ASSERT_FALSE(adc->adcSetSampleRate(8000));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
}

void test_sample_rate_384k_rejected(void) {
    TEST_ASSERT_FALSE(adc->adcSetSampleRate(384000));
}

void test_get_sample_rate(void) {
    adc->adcSetSampleRate(96000);
    TEST_ASSERT_EQUAL(96000u, adc->adcGetSampleRate());
}

// ==========================================================================
// Section 9: Filter preset tests (global, single register)
// ==========================================================================

void test_filter_preset_valid(void) {
    for (uint8_t i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(adc->setFilterPreset(i));
        TEST_ASSERT_EQUAL(i, adc->_filterPreset);
    }
}

void test_filter_preset_invalid(void) {
    TEST_ASSERT_FALSE(adc->setFilterPreset(8));
    TEST_ASSERT_FALSE(adc->setFilterPreset(255));
    TEST_ASSERT_EQUAL(0, adc->_filterPreset);
}

// ==========================================================================
// Section 10: Mute tests
// ==========================================================================

void test_mute_on(void) {
    adc->init();
    TEST_ASSERT_FALSE(adc->_muted);
    TEST_ASSERT_TRUE(adc->setMute(true));
    TEST_ASSERT_TRUE(adc->_muted);
    // ES9841: mute = 0x00 on all channels
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x00, adc->_chVolume[i]);
    }
}

void test_mute_off(void) {
    adc->init();
    adc->setMute(true);
    TEST_ASSERT_TRUE(adc->setMute(false));
    TEST_ASSERT_FALSE(adc->_muted);
    // ES9841: unmute restores saved volume (0xFF after init)
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, adc->_chVolume[i]);
    }
}

void test_mute_restores_saved_volume(void) {
    // Set volume to 50%, then mute, then unmute — should restore ~50% not 0dB
    adc->init();
    adc->setVolume(50);  // saves vol8 ~= 0x7F
    uint8_t savedVol = adc->_savedVol;
    adc->setMute(true);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x00, adc->_chVolume[i]);
    }
    adc->setMute(false);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX8(savedVol, adc->_chVolume[i]);
    }
}

// ==========================================================================
// Section 11: Configure tests
// ==========================================================================

void test_configure_valid(void) {
    TEST_ASSERT_TRUE(adc->configure(48000, 32));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(32, adc->_bitDepth);
}

void test_configure_44k1_24bit(void) {
    TEST_ASSERT_TRUE(adc->configure(44100, 24));
    TEST_ASSERT_EQUAL(44100u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(24, adc->_bitDepth);
}

void test_configure_96k_24bit(void) {
    TEST_ASSERT_TRUE(adc->configure(96000, 24));
    TEST_ASSERT_EQUAL(96000u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(24, adc->_bitDepth);
}

void test_configure_unsupported_rate(void) {
    TEST_ASSERT_FALSE(adc->configure(8000, 16));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
}

void test_configure_unsupported_bits(void) {
    TEST_ASSERT_FALSE(adc->configure(48000, 8));
}

void test_sequential_reconfigure(void) {
    TEST_ASSERT_TRUE(adc->configure(44100, 24));
    TEST_ASSERT_EQUAL(44100u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(24, adc->_bitDepth);

    TEST_ASSERT_TRUE(adc->configure(192000, 32));
    TEST_ASSERT_EQUAL(192000u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(32, adc->_bitDepth);

    TEST_ASSERT_TRUE(adc->configure(48000, 16));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(16, adc->_bitDepth);
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
    RUN_TEST(test_descriptor_chip_id_constant);

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
    RUN_TEST(test_input_source_name_pair_a);
    RUN_TEST(test_input_source_count);
    RUN_TEST(test_input_source_at_1_is_pair_b);
    RUN_TEST(test_input_source_at_out_of_range);
    RUN_TEST(test_input_source_vu_initial_minus90);
    RUN_TEST(test_input_source_gain_linear_unity);
    RUN_TEST(test_get_input_source_after_deinit);

    // Section 4: Gain (3-bit, 0-42 dB)
    RUN_TEST(test_gain_set_0db);
    RUN_TEST(test_gain_set_6db);
    RUN_TEST(test_gain_set_12db);
    RUN_TEST(test_gain_set_18db);
    RUN_TEST(test_gain_set_24db);
    RUN_TEST(test_gain_set_30db);
    RUN_TEST(test_gain_set_36db);
    RUN_TEST(test_gain_set_42db);
    RUN_TEST(test_gain_clamp_above_42);
    RUN_TEST(test_gain_floor_to_step);

    // Section 5: Volume (8-bit, 0%=0x00, 100%=0xFF)
    RUN_TEST(test_volume_0_percent_is_mute);
    RUN_TEST(test_volume_100_percent_is_0db);
    RUN_TEST(test_volume_50_percent_midrange);
    RUN_TEST(test_volume_all_channels_equal);

    // Section 6: Per-channel 8-bit volume
    RUN_TEST(test_channel_volume_ch0_0db);
    RUN_TEST(test_channel_volume_ch1_mute);
    RUN_TEST(test_channel_volume_ch2_mid);
    RUN_TEST(test_channel_volume_ch3_custom);
    RUN_TEST(test_channel_volume_invalid_channel);
    RUN_TEST(test_channel_volume_independent);

    // Section 7: HPF
    RUN_TEST(test_hpf_enable);
    RUN_TEST(test_hpf_disable);
    RUN_TEST(test_hpf_toggle_round_trip);

    // Section 8: Sample rate
    RUN_TEST(test_sample_rate_44k1);
    RUN_TEST(test_sample_rate_48k);
    RUN_TEST(test_sample_rate_96k);
    RUN_TEST(test_sample_rate_192k);
    RUN_TEST(test_sample_rate_8k_rejected);
    RUN_TEST(test_sample_rate_384k_rejected);
    RUN_TEST(test_get_sample_rate);

    // Section 9: Filter preset (global)
    RUN_TEST(test_filter_preset_valid);
    RUN_TEST(test_filter_preset_invalid);

    // Section 10: Mute (ES9841: 0x00=mute, restore _savedVol on unmute)
    RUN_TEST(test_mute_on);
    RUN_TEST(test_mute_off);
    RUN_TEST(test_mute_restores_saved_volume);

    // Section 11: Configure
    RUN_TEST(test_configure_valid);
    RUN_TEST(test_configure_44k1_24bit);
    RUN_TEST(test_configure_96k_24bit);
    RUN_TEST(test_configure_unsupported_rate);
    RUN_TEST(test_configure_unsupported_bits);
    RUN_TEST(test_sequential_reconfigure);

    return UNITY_END();
}
