// test_hal_es9842pro.cpp
// Tests a mock HalEs9842pro class modelling the ESS ES9842PRO I2C ADC.
//
// The ES9842PRO is a high-performance ESS SABRE 4-channel ADC (122dB DNR):
//   - I2C address 0x40 (configurable via address pins)
//   - 4-channel ADC with TDM output
//   - Chip ID: 0x83 (register 0xE1)
//   - Supported sample rates: 44.1kHz, 48kHz, 96kHz, 192kHz
//   - 16-bit per-channel volume (0x7FFF=0dB, 0x0000=mute)
//   - 2-bit gain per channel: 0=0dB, 1=+6dB, 2=+12dB, 3=+18dB (18dB max)
//   - Per-channel HPF (DC blocking) via dedicated registers
//   - Per-channel filter preset (0-7) via bits[4:2] in per-channel filter regs
//   - Exposes 2 AudioInputSources: CH1/2 (pair A) and CH3/4 (pair B)
//
// Key behaviours:
//   - probe() returns true (NATIVE_TEST stub)
//   - init() succeeds, sets _ready=true, _state=AVAILABLE
//   - adcSetGain(): valid 0, 6, 12, 18; values >18 clamped to 18
//   - adcSetHpfEnabled(): toggles HPF state for all 4 channels
//   - adcSetSampleRate(): accepts 44100, 48000, 96000, 192000; rejects 8000
//   - setFilterPreset(): accepts 0-7; rejects >=8
//   - setVolume(): 0%->mute(0x0000), 100%->0dB(0x7FFF)
//   - setMute(): all channels muted/unmuted
//   - getInputSourceCount() returns 2 after init, 0 before
//   - getInputSourceAt(0) and (1) non-null after init
//   - getInputSourceAt(2) returns nullptr
//   - healthCheck() returns _ready
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

// ===== Mock ES9842PRO ADC device =====
//
// Models the ESS ES9842PRO 4-channel ADC with TDM output.
// Gain 2-bit (0-18 dB, 6 dB steps). Filter presets 0-7 per channel.
// 16-bit volume: 0x7FFF=0dB (100%), 0x0000=mute (0%).
//
class HalEs9842proMock : public HalAudioDevice, public HalAudioAdcInterface {
public:
    static const uint8_t  kI2cAddr  = 0x40;
    static const uint8_t  kChipId   = 0x83;

    uint8_t  _gainDb       = 0;
    bool     _hpfEnabled   = true;
    uint32_t _sampleRate   = 48000;
    uint8_t  _bitDepth     = 32;
    uint8_t  _filterPreset = 0;
    bool     _muted        = false;
    uint8_t  _volume       = 100;    // percent
    uint16_t _chVol16[4]   = {0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF}; // 16-bit per channel

    AudioInputSource _inputSrc  = {};
    AudioInputSource _inputSrcB = {};
    bool             _inputSrcReady = false;

    HalEs9842proMock() {
        strncpy(_descriptor.compatible, "ess,es9842pro", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "ES9842PRO", 32);
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

    // ----- HalDevice lifecycle -----

    bool probe() override { return true; }

    HalInitResult init() override {
        _state = HAL_STATE_AVAILABLE;
        _ready = true;

        memset(&_inputSrc, 0, sizeof(_inputSrc));
        _inputSrc.name          = "ES9842PRO CH1/2";
        _inputSrc.isHardwareAdc = true;
        _inputSrc.gainLinear    = 1.0f;
        _inputSrc.vuL           = -90.0f;
        _inputSrc.vuR           = -90.0f;
        _inputSrc.halSlot       = 0xFF;

        memset(&_inputSrcB, 0, sizeof(_inputSrcB));
        _inputSrcB.name          = "ES9842PRO CH3/4";
        _inputSrcB.isHardwareAdc = true;
        _inputSrcB.gainLinear    = 1.0f;
        _inputSrcB.vuL           = -90.0f;
        _inputSrcB.vuR           = -90.0f;
        _inputSrcB.halSlot       = 0xFF;

        _inputSrcReady = true;

        for (int i = 0; i < 4; i++) _chVol16[i] = 0x7FFF;

        return hal_init_ok();
    }

    void deinit() override {
        _ready         = false;
        _state         = HAL_STATE_REMOVED;
        _inputSrcReady = false;
    }

    void dumpConfig() override {}

    bool healthCheck() override { return _ready; }

    // Multi-source interface
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
        uint16_t vol16;
        if (pct == 0) {
            vol16 = 0x0000;
        } else if (pct == 100) {
            vol16 = 0x7FFF;
        } else {
            vol16 = (uint16_t)(((uint32_t)pct * 0x7FFF) / 100);
        }
        for (int i = 0; i < 4; i++) _chVol16[i] = vol16;
        return true;
    }

    bool setMute(bool mute) override {
        _muted = mute;
        uint16_t vol16 = mute ? 0x0000 : 0x7FFF;
        for (int i = 0; i < 4; i++) _chVol16[i] = vol16;
        return true;
    }

    // ----- HalAudioAdcInterface -----

    bool adcSetGain(uint8_t dB) override {
        // 2-bit gain: max 18 dB
        if (dB > 18) dB = 18;
        uint8_t step = dB / 6;
        if (step > 3) step = 3;
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

    // ----- ES9842PRO-specific: per-channel filter preset -----

    bool setFilterPreset(uint8_t preset) {
        if (preset >= 8) return false;
        _filterPreset = preset;
        return true;
    }

    // ----- ES9842PRO-specific: per-channel 16-bit volume -----

    bool setChannelVolume16(uint8_t ch, uint16_t vol) {
        if (ch >= 4) return false;
        _chVol16[ch] = vol;
        return true;
    }
};

// ===== Fixtures =====
static HalEs9842proMock* adc;

void setUp(void) {
    WireMock::reset();
    adc = new HalEs9842proMock();
}

void tearDown(void) {
    delete adc;
    adc = nullptr;
}

// ==========================================================================
// Section 1: Descriptor tests
// ==========================================================================

void test_descriptor_compatible_string(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9842pro", adc->getDescriptor().compatible);
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
    TEST_ASSERT_EQUAL_HEX8(0x83, HalEs9842proMock::kChipId);
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
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
}

void test_get_input_source_before_init(void) {
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NULL(src);
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
    TEST_ASSERT_EQUAL_STRING("ES9842PRO CH1/2", src->name);
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
    TEST_ASSERT_EQUAL_STRING("ES9842PRO CH3/4", srcB->name);
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
// Section 4: Gain tests (2-bit, 6 dB steps, 0-18 dB max)
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

void test_gain_clamp_above_18(void) {
    // ES9842PRO 2-bit gain: values above 18 clamped to 18
    TEST_ASSERT_TRUE(adc->adcSetGain(19));
    TEST_ASSERT_EQUAL(18, adc->_gainDb);
    TEST_ASSERT_TRUE(adc->adcSetGain(42));
    TEST_ASSERT_EQUAL(18, adc->_gainDb);
    TEST_ASSERT_TRUE(adc->adcSetGain(255));
    TEST_ASSERT_EQUAL(18, adc->_gainDb);
}

void test_gain_floor_to_step(void) {
    // 7 dB -> floor(7/6)*6 = 6 dB
    TEST_ASSERT_TRUE(adc->adcSetGain(7));
    TEST_ASSERT_EQUAL(6, adc->_gainDb);
    // 11 dB -> floor(11/6)*6 = 6 dB
    TEST_ASSERT_TRUE(adc->adcSetGain(11));
    TEST_ASSERT_EQUAL(6, adc->_gainDb);
    // 13 dB -> floor(13/6)*6 = 12 dB
    TEST_ASSERT_TRUE(adc->adcSetGain(13));
    TEST_ASSERT_EQUAL(12, adc->_gainDb);
}

// ==========================================================================
// Section 5: Volume tests (16-bit: 0%=0x0000 mute, 100%=0x7FFF 0dB)
// ==========================================================================

void test_volume_0_percent_is_mute(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setVolume(0));
    TEST_ASSERT_EQUAL(0, adc->_volume);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX16(0x0000, adc->_chVol16[i]);
    }
}

void test_volume_100_percent_is_0db(void) {
    adc->init();
    adc->setVolume(0);
    TEST_ASSERT_TRUE(adc->setVolume(100));
    TEST_ASSERT_EQUAL(100, adc->_volume);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX16(0x7FFF, adc->_chVol16[i]);
    }
}

void test_volume_50_percent_midrange(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setVolume(50));
    TEST_ASSERT_EQUAL(50, adc->_volume);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_GREATER_THAN(0x0000, adc->_chVol16[i]);
        TEST_ASSERT_LESS_THAN(0x7FFF, adc->_chVol16[i]);
    }
}

void test_volume_all_channels_equal(void) {
    adc->init();
    adc->setVolume(75);
    TEST_ASSERT_EQUAL(adc->_chVol16[0], adc->_chVol16[1]);
    TEST_ASSERT_EQUAL(adc->_chVol16[1], adc->_chVol16[2]);
    TEST_ASSERT_EQUAL(adc->_chVol16[2], adc->_chVol16[3]);
}

// ==========================================================================
// Section 6: Per-channel 16-bit volume tests
// ==========================================================================

void test_channel_volume_ch0_0db(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setChannelVolume16(0, 0x7FFF));
    TEST_ASSERT_EQUAL_HEX16(0x7FFF, adc->_chVol16[0]);
}

void test_channel_volume_ch1_mute(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setChannelVolume16(1, 0x0000));
    TEST_ASSERT_EQUAL_HEX16(0x0000, adc->_chVol16[1]);
}

void test_channel_volume_ch2_mid(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setChannelVolume16(2, 0x4000));
    TEST_ASSERT_EQUAL_HEX16(0x4000, adc->_chVol16[2]);
}

void test_channel_volume_ch3_custom(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setChannelVolume16(3, 0x2000));
    TEST_ASSERT_EQUAL_HEX16(0x2000, adc->_chVol16[3]);
}

void test_channel_volume_invalid_channel(void) {
    adc->init();
    TEST_ASSERT_FALSE(adc->setChannelVolume16(4, 0x7FFF));
    TEST_ASSERT_FALSE(adc->setChannelVolume16(255, 0x7FFF));
}

void test_channel_volume_independent(void) {
    adc->init();
    adc->setChannelVolume16(0, 0x7FFF);
    adc->setChannelVolume16(1, 0x4000);
    adc->setChannelVolume16(2, 0x2000);
    adc->setChannelVolume16(3, 0x0000);
    TEST_ASSERT_EQUAL_HEX16(0x7FFF, adc->_chVol16[0]);
    TEST_ASSERT_EQUAL_HEX16(0x4000, adc->_chVol16[1]);
    TEST_ASSERT_EQUAL_HEX16(0x2000, adc->_chVol16[2]);
    TEST_ASSERT_EQUAL_HEX16(0x0000, adc->_chVol16[3]);
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
// Section 9: Filter preset tests
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
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX16(0x0000, adc->_chVol16[i]);
    }
}

void test_mute_off(void) {
    adc->init();
    adc->setMute(true);
    TEST_ASSERT_TRUE(adc->setMute(false));
    TEST_ASSERT_FALSE(adc->_muted);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX16(0x7FFF, adc->_chVol16[i]);
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

    // Section 4: Gain (2-bit, 0-18 dB)
    RUN_TEST(test_gain_set_0db);
    RUN_TEST(test_gain_set_6db);
    RUN_TEST(test_gain_set_12db);
    RUN_TEST(test_gain_set_18db);
    RUN_TEST(test_gain_clamp_above_18);
    RUN_TEST(test_gain_floor_to_step);

    // Section 5: Volume (16-bit)
    RUN_TEST(test_volume_0_percent_is_mute);
    RUN_TEST(test_volume_100_percent_is_0db);
    RUN_TEST(test_volume_50_percent_midrange);
    RUN_TEST(test_volume_all_channels_equal);

    // Section 6: Per-channel 16-bit volume
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

    // Section 9: Filter preset
    RUN_TEST(test_filter_preset_valid);
    RUN_TEST(test_filter_preset_invalid);

    // Section 10: Mute
    RUN_TEST(test_mute_on);
    RUN_TEST(test_mute_off);

    // Section 11: Configure
    RUN_TEST(test_configure_valid);
    RUN_TEST(test_configure_44k1_24bit);
    RUN_TEST(test_configure_96k_24bit);
    RUN_TEST(test_configure_unsupported_rate);
    RUN_TEST(test_configure_unsupported_bits);
    RUN_TEST(test_sequential_reconfigure);

    return UNITY_END();
}
