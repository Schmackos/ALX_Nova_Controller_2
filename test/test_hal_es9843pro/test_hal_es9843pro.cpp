// test_hal_es9843pro.cpp
// Tests a mock HalEs9843pro class modelling the ESS ES9843PRO I2C ADC.
//
// The ES9843PRO is a high-performance ESS SABRE 4-channel ADC with I2C control:
//   - I2C address 0x40 (configurable via address pins)
//   - 4-channel ADC
//   - Chip ID: 0x8F (register 0xE1)
//   - Supported sample rates: 44.1kHz, 48kHz, 96kHz, 192kHz
//   - Hardware volume (8-bit per channel): 0x00=0dB, 0xFE=-127dB, 0xFF=mute
//   - Gain settings: 0, 6, 12, 18, 24, 30, 36, 42 dB (42 dB max, values above clamped)
//   - HPF per-channel enable/disable via I2C (all 4 channels simultaneously)
//   - 8 global digital filter presets (0-7)
//   - Per-channel volume via setChannelVolume(ch, vol8) for channels 0-3
//   - Mute via I2C register (all 4 channels)
//   - getInputSource() returns AudioInputSource with isHardwareAdc=true
//
// Key behaviours:
//   - probe() returns true (NATIVE_TEST stub)
//   - init()  succeeds, sets _ready=true, _state=AVAILABLE
//   - adcSetGain(): valid 0, 6, 12, 18, 24, 30, 36, 42; values >42 clamped
//   - adcSetHpfEnabled(): toggles HPF state for all 4 channels
//   - adcSetSampleRate(): accepts 44100, 48000, 96000, 192000; rejects 8000
//   - setFilterPreset(): accepts 0-7 (global); rejects >=8
//   - setVolume(): 0%->mute(0xFF), 100%->0dB(0x00)
//   - setChannelVolume(): per-channel 8-bit volume for channels 0-3
//   - setMute(): all 4 channels muted/unmuted
//   - configure(): accepts 44100/48000/96000/192000 with 16/24/32 bit depth
//   - type = HAL_DEV_ADC
//   - bus = HAL_BUS_I2C
//   - capabilities includes HAL_CAP_ADC_PATH, HAL_CAP_PGA_CONTROL,
//                           HAL_CAP_HPF_CONTROL, HAL_CAP_HW_VOLUME
//   - NO DAC path
//   - channelCount = 4
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

// ===== Mock ES9843PRO ADC device =====
//
// Models the ESS ES9843PRO 4-channel ADC with I2C control interface.
// Gain is clamped to 42 dB maximum (3-bit, 6 dB steps). Filter presets 0-7 valid.
// Supported rates: 44100, 48000, 96000, 192000 Hz.
// Volume is 8-bit: 0x00=0dB (100%), 0xFF=mute (0%).
//
class HalEs9843proMock : public HalAudioDevice, public HalAudioAdcInterface {
public:
    static const uint8_t kI2cAddr  = 0x40;
    static const uint8_t kChipId   = 0x8F;

    uint8_t  _gainDb         = 0;
    bool     _hpfEnabled     = true;    // HPF defaults on at power-up
    uint32_t _sampleRate     = 48000;
    uint8_t  _bitDepth       = 32;
    uint8_t  _filterPreset   = 0;
    bool     _muted          = false;
    uint8_t  _volume         = 100;     // 0-100 percent
    uint8_t  _chVolume[4]    = {0x00, 0x00, 0x00, 0x00};  // 8-bit per-channel (0x00 = 0dB)

    AudioInputSource _inputSrc     = {};
    bool             _inputSrcReady = false;

    HalEs9843proMock() {
        strncpy(_descriptor.compatible, "ess,es9843pro", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "ES9843PRO", 32);
        _descriptor.name[32] = '\0';
        strncpy(_descriptor.manufacturer, "ESS Technology", 32);
        _descriptor.manufacturer[32] = '\0';
        _descriptor.type         = HAL_DEV_ADC;
        _descriptor.i2cAddr      = kI2cAddr;
        _descriptor.channelCount = 4;       // 4-channel ADC
        _descriptor.bus.type     = HAL_BUS_I2C;
        _descriptor.bus.index    = HAL_I2C_BUS_EXP;
        _descriptor.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        _descriptor.capabilities =
            HAL_CAP_ADC_PATH | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL | HAL_CAP_HW_VOLUME;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    // ----- HalDevice lifecycle -----

    bool probe() override { return true; }  // NATIVE_TEST stub

    HalInitResult init() override {
        _state = HAL_STATE_AVAILABLE;
        _ready = true;

        // Populate pair A AudioInputSource (CH1/CH2)
        memset(&_inputSrc, 0, sizeof(_inputSrc));
        _inputSrc.name          = "ES9843PRO CH1/2";
        _inputSrc.isHardwareAdc = true;
        _inputSrc.gainLinear    = 1.0f;
        _inputSrc.vuL           = -90.0f;
        _inputSrc.vuR           = -90.0f;
        _inputSrc.halSlot       = 0xFF;

        // Populate pair B AudioInputSource (CH3/CH4)
        memset(&_inputSrcB, 0, sizeof(_inputSrcB));
        _inputSrcB.name          = "ES9843PRO CH3/4";
        _inputSrcB.isHardwareAdc = true;
        _inputSrcB.gainLinear    = 1.0f;
        _inputSrcB.vuL           = -90.0f;
        _inputSrcB.vuR           = -90.0f;
        _inputSrcB.halSlot       = 0xFF;

        _inputSrcReady = true;

        // Reset per-channel volumes to 0dB
        for (int i = 0; i < 4; i++) _chVolume[i] = 0x00;

        return hal_init_ok();
    }

    void deinit() override {
        _ready         = false;
        _state         = HAL_STATE_REMOVED;
        _inputSrcReady = false;
    }

    void dumpConfig() override {}

    bool healthCheck() override { return _ready; }

    // Multi-source interface (mirrors the real ES9843PRO dual-source TDM design).
    // The mock exposes two sources when initialized to match production behaviour.
    AudioInputSource _inputSrcB = {};

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
        // Compute 8-bit register value: 0%->0xFF(mute), 100%->0x00(0dB)
        uint8_t vol8;
        if (pct == 0) {
            vol8 = 0xFF;
        } else if (pct == 100) {
            vol8 = 0x00;
        } else {
            vol8 = (uint8_t)(0xFE - (uint8_t)(((uint32_t)(pct - 1) * 0xFE) / 99));
        }
        for (int i = 0; i < 4; i++) _chVolume[i] = vol8;
        return true;
    }

    bool setMute(bool mute) override {
        _muted = mute;
        uint8_t vol8 = mute ? 0xFF : 0x00;
        for (int i = 0; i < 4; i++) _chVolume[i] = vol8;
        return true;
    }

    // ----- HalAudioAdcInterface -----

    bool adcSetGain(uint8_t dB) override {
        // Floor to nearest 6 dB step, clamp to 42 dB (7 steps max)
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

    // ----- ES9843PRO-specific: global filter preset -----

    bool setFilterPreset(uint8_t preset) {
        if (preset >= 8) return false;
        _filterPreset = preset;
        return true;
    }

    // ----- ES9843PRO-specific: per-channel 8-bit volume -----

    bool setChannelVolume(uint8_t ch, uint8_t vol8) {
        if (ch >= 4) return false;
        _chVolume[ch] = vol8;
        return true;
    }
};

// ===== Fixtures =====
static HalEs9843proMock* adc;

void setUp(void) {
    WireMock::reset();
    adc = new HalEs9843proMock();
}

void tearDown(void) {
    delete adc;
    adc = nullptr;
}

// ==========================================================================
// Section 1: Descriptor tests
// ==========================================================================

// ----- 1. descriptor compatible string is "ess,es9843pro" -----
void test_descriptor_compatible_string(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9843pro", adc->getDescriptor().compatible);
}

// ----- 2. descriptor type is HAL_DEV_ADC -----
void test_descriptor_type_is_adc(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_ADC, (int)adc->getDescriptor().type);
}

// ----- 3. capabilities include required ADC capability flags -----
void test_descriptor_capabilities(void) {
    uint8_t caps = adc->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_PGA_CONTROL);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HPF_CONTROL);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    // ADC-only: no DAC path
    TEST_ASSERT_FALSE(caps & HAL_CAP_DAC_PATH);
}

// ----- 4. channel count is 4 -----
void test_descriptor_channel_count(void) {
    TEST_ASSERT_EQUAL(4, adc->getDescriptor().channelCount);
}

// ----- 5. bus type is I2C -----
void test_descriptor_bus_type(void) {
    TEST_ASSERT_EQUAL(HAL_BUS_I2C, (int)adc->getDescriptor().bus.type);
}

// ----- 6. I2C address is 0x40 -----
void test_descriptor_i2c_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0x40, adc->getDescriptor().i2cAddr);
}

// ----- 7. chip ID constant is 0x8F -----
void test_descriptor_chip_id_constant(void) {
    TEST_ASSERT_EQUAL_HEX8(0x8F, HalEs9843proMock::kChipId);
}

// ==========================================================================
// Section 2: Lifecycle tests
// ==========================================================================

// ----- 8. probe() returns true (NATIVE_TEST stub) -----
void test_probe_returns_true(void) {
    TEST_ASSERT_TRUE(adc->probe());
}

// ----- 9. init() succeeds, sets _ready=true and state=AVAILABLE -----
void test_init_success(void) {
    HalInitResult res = adc->init();
    TEST_ASSERT_TRUE(res.success);
    TEST_ASSERT_TRUE(adc->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)adc->_state);
}

// ----- 10. deinit() clears _ready and sets state=REMOVED -----
void test_deinit_clears_ready(void) {
    adc->init();
    adc->deinit();
    TEST_ASSERT_FALSE(adc->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)adc->_state);
}

// ----- 11. healthCheck() returns true after init -----
void test_health_check_after_init(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->healthCheck());
}

// ----- 12. healthCheck() returns false before init -----
void test_health_check_before_init(void) {
    // _ready is false by default (HalDevice constructor)
    TEST_ASSERT_FALSE(adc->healthCheck());
}

// ==========================================================================
// Section 3: Audio input source tests
// ==========================================================================

// ----- 13. getInputSource() returns non-null after init -----
void test_get_input_source_after_init(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
}

// ----- 14. getInputSource() returns null before init -----
void test_get_input_source_before_init(void) {
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NULL(src);
}

// ----- 15. input source isHardwareAdc is true -----
void test_input_source_is_hardware_adc(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_TRUE(src->isHardwareAdc);
}

// ----- 16. source 0 name is "ES9843PRO CH1/2" (TDM pair A) -----
void test_input_source_name_matches_descriptor(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL_STRING("ES9843PRO CH1/2", src->name);
}

// ----- 16b. getInputSourceCount() returns 2 after init, 0 before -----
void test_input_source_count(void) {
    TEST_ASSERT_EQUAL_INT(0, adc->getInputSourceCount());
    adc->init();
    TEST_ASSERT_EQUAL_INT(2, adc->getInputSourceCount());
}

// ----- 16c. getInputSourceAt(1) returns pair B with correct name -----
void test_input_source_at_1_is_pair_b(void) {
    adc->init();
    const AudioInputSource* srcB = adc->getInputSourceAt(1);
    TEST_ASSERT_NOT_NULL(srcB);
    TEST_ASSERT_EQUAL_STRING("ES9843PRO CH3/4", srcB->name);
    TEST_ASSERT_TRUE(srcB->isHardwareAdc);
    TEST_ASSERT_EQUAL_FLOAT(-90.0f, srcB->vuL);
    TEST_ASSERT_EQUAL_FLOAT(-90.0f, srcB->vuR);
}

// ----- 16d. getInputSourceAt(2) returns nullptr (out of range) -----
void test_input_source_at_out_of_range(void) {
    adc->init();
    TEST_ASSERT_NULL(adc->getInputSourceAt(2));
    TEST_ASSERT_NULL(adc->getInputSourceAt(-1));
}

// ----- 17. input source initial VU is -90 dBFS -----
void test_input_source_vu_initial_minus90(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL_FLOAT(-90.0f, src->vuL);
    TEST_ASSERT_EQUAL_FLOAT(-90.0f, src->vuR);
}

// ----- 18. input source gainLinear is 1.0 (unity) -----
void test_input_source_gain_linear_unity(void) {
    adc->init();
    const AudioInputSource* src = adc->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, src->gainLinear);
}

// ----- 19. getInputSource() returns null after deinit -----
void test_get_input_source_after_deinit(void) {
    adc->init();
    TEST_ASSERT_NOT_NULL(adc->getInputSource());
    adc->deinit();
    TEST_ASSERT_NULL(adc->getInputSource());
}

// ==========================================================================
// Section 4: Gain tests (3-bit, 6 dB steps, 0-42 dB)
// ==========================================================================

// ----- 20. adcSetGain(0) succeeds, _gainDb == 0 -----
void test_gain_set_0db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(0));
    TEST_ASSERT_EQUAL(0, adc->_gainDb);
}

// ----- 21. adcSetGain(6) succeeds -----
void test_gain_set_6db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(6));
    TEST_ASSERT_EQUAL(6, adc->_gainDb);
}

// ----- 22. adcSetGain(12) succeeds -----
void test_gain_set_12db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(12));
    TEST_ASSERT_EQUAL(12, adc->_gainDb);
}

// ----- 23. adcSetGain(18) succeeds -----
void test_gain_set_18db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(18));
    TEST_ASSERT_EQUAL(18, adc->_gainDb);
}

// ----- 24. adcSetGain(24) succeeds -----
void test_gain_set_24db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(24));
    TEST_ASSERT_EQUAL(24, adc->_gainDb);
}

// ----- 25. adcSetGain(30) succeeds -----
void test_gain_set_30db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(30));
    TEST_ASSERT_EQUAL(30, adc->_gainDb);
}

// ----- 26. adcSetGain(36) succeeds -----
void test_gain_set_36db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(36));
    TEST_ASSERT_EQUAL(36, adc->_gainDb);
}

// ----- 27. adcSetGain(42) succeeds (max step) -----
void test_gain_set_42db(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(42));
    TEST_ASSERT_EQUAL(42, adc->_gainDb);
}

// ----- 28. adcSetGain above 42 clamps to 42 dB -----
void test_gain_clamp_above_42(void) {
    TEST_ASSERT_TRUE(adc->adcSetGain(48));
    TEST_ASSERT_EQUAL(42, adc->_gainDb);
    TEST_ASSERT_TRUE(adc->adcSetGain(255));
    TEST_ASSERT_EQUAL(42, adc->_gainDb);
}

// ----- 29. adcSetGain floors non-multiple to nearest 6 dB step -----
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
// Section 5: Volume tests (8-bit: 0%=0xFF mute, 100%=0x00 0dB)
// ==========================================================================

// ----- 30. setVolume(0) sets mute byte 0xFF across all channels -----
void test_volume_0_percent_is_mute(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setVolume(0));
    TEST_ASSERT_EQUAL(0, adc->_volume);
    // All 4 channel registers must be 0xFF (mute)
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, adc->_chVolume[i]);
    }
}

// ----- 31. setVolume(100) sets 0x00 (0 dB) across all channels -----
void test_volume_100_percent_is_0db(void) {
    adc->init();
    // Start at muted state to confirm it changes
    adc->setVolume(0);
    TEST_ASSERT_TRUE(adc->setVolume(100));
    TEST_ASSERT_EQUAL(100, adc->_volume);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x00, adc->_chVolume[i]);
    }
}

// ----- 32. setVolume(50) is between 0x00 and 0xFF -----
void test_volume_50_percent_midrange(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setVolume(50));
    TEST_ASSERT_EQUAL(50, adc->_volume);
    // Must be strictly between mute and 0dB
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_GREATER_THAN(0x00, adc->_chVolume[i]);
        TEST_ASSERT_LESS_THAN(0xFF, adc->_chVolume[i]);
    }
}

// ----- 33. setVolume: all 4 channels get the same value -----
void test_volume_all_channels_equal(void) {
    adc->init();
    adc->setVolume(75);
    TEST_ASSERT_EQUAL(adc->_chVolume[0], adc->_chVolume[1]);
    TEST_ASSERT_EQUAL(adc->_chVolume[1], adc->_chVolume[2]);
    TEST_ASSERT_EQUAL(adc->_chVolume[2], adc->_chVolume[3]);
}

// ==========================================================================
// Section 6: Per-channel volume tests
// ==========================================================================

// ----- 34. setChannelVolume(0, 0x00) succeeds (CH1, 0dB) -----
void test_channel_volume_ch0_0db(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setChannelVolume(0, 0x00));
    TEST_ASSERT_EQUAL_HEX8(0x00, adc->_chVolume[0]);
}

// ----- 35. setChannelVolume(1, 0xFF) succeeds (CH2, mute) -----
void test_channel_volume_ch1_mute(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setChannelVolume(1, 0xFF));
    TEST_ASSERT_EQUAL_HEX8(0xFF, adc->_chVolume[1]);
}

// ----- 36. setChannelVolume(2, 0x40) succeeds (CH3, mid attenuation) -----
void test_channel_volume_ch2_mid(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setChannelVolume(2, 0x40));
    TEST_ASSERT_EQUAL_HEX8(0x40, adc->_chVolume[2]);
}

// ----- 37. setChannelVolume(3, 0x20) succeeds (CH4) -----
void test_channel_volume_ch3_custom(void) {
    adc->init();
    TEST_ASSERT_TRUE(adc->setChannelVolume(3, 0x20));
    TEST_ASSERT_EQUAL_HEX8(0x20, adc->_chVolume[3]);
}

// ----- 38. setChannelVolume(4, ...) returns false (out of range) -----
void test_channel_volume_invalid_channel(void) {
    adc->init();
    TEST_ASSERT_FALSE(adc->setChannelVolume(4, 0x00));
    TEST_ASSERT_FALSE(adc->setChannelVolume(255, 0x00));
}

// ----- 39. per-channel writes are independent -----
void test_channel_volume_independent(void) {
    adc->init();
    adc->setChannelVolume(0, 0x00);   // CH1: 0dB
    adc->setChannelVolume(1, 0x40);   // CH2: moderate
    adc->setChannelVolume(2, 0x80);   // CH3: more attenuation
    adc->setChannelVolume(3, 0xFF);   // CH4: mute
    TEST_ASSERT_EQUAL_HEX8(0x00, adc->_chVolume[0]);
    TEST_ASSERT_EQUAL_HEX8(0x40, adc->_chVolume[1]);
    TEST_ASSERT_EQUAL_HEX8(0x80, adc->_chVolume[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, adc->_chVolume[3]);
}

// ==========================================================================
// Section 7: HPF tests (all 4 channels simultaneously)
// ==========================================================================

// ----- 40. adcSetHpfEnabled(true) enables HPF -----
void test_hpf_enable(void) {
    adc->_hpfEnabled = false;
    TEST_ASSERT_TRUE(adc->adcSetHpfEnabled(true));
    TEST_ASSERT_TRUE(adc->_hpfEnabled);
}

// ----- 41. adcSetHpfEnabled(false) disables HPF -----
void test_hpf_disable(void) {
    TEST_ASSERT_TRUE(adc->_hpfEnabled);  // Default is on
    TEST_ASSERT_TRUE(adc->adcSetHpfEnabled(false));
    TEST_ASSERT_FALSE(adc->_hpfEnabled);
}

// ----- 42. HPF toggle round-trip -----
void test_hpf_toggle_round_trip(void) {
    TEST_ASSERT_TRUE(adc->adcSetHpfEnabled(false));
    TEST_ASSERT_FALSE(adc->_hpfEnabled);
    TEST_ASSERT_TRUE(adc->adcSetHpfEnabled(true));
    TEST_ASSERT_TRUE(adc->_hpfEnabled);
}

// ==========================================================================
// Section 8: Sample rate tests
// ==========================================================================

// ----- 43. adcSetSampleRate(44100) succeeds -----
void test_sample_rate_44k1(void) {
    TEST_ASSERT_TRUE(adc->adcSetSampleRate(44100));
    TEST_ASSERT_EQUAL(44100u, adc->_sampleRate);
}

// ----- 44. adcSetSampleRate(48000) succeeds -----
void test_sample_rate_48k(void) {
    TEST_ASSERT_TRUE(adc->adcSetSampleRate(48000));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
}

// ----- 45. adcSetSampleRate(96000) succeeds -----
void test_sample_rate_96k(void) {
    TEST_ASSERT_TRUE(adc->adcSetSampleRate(96000));
    TEST_ASSERT_EQUAL(96000u, adc->_sampleRate);
}

// ----- 46. adcSetSampleRate(192000) succeeds -----
void test_sample_rate_192k(void) {
    TEST_ASSERT_TRUE(adc->adcSetSampleRate(192000));
    TEST_ASSERT_EQUAL(192000u, adc->_sampleRate);
}

// ----- 47. adcSetSampleRate(8000) returns false (not supported) -----
void test_sample_rate_8k_rejected(void) {
    TEST_ASSERT_FALSE(adc->adcSetSampleRate(8000));
    // Should remain at default 48000
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
}

// ----- 48. adcSetSampleRate(384000) returns false (not in supported list) -----
void test_sample_rate_384k_rejected(void) {
    TEST_ASSERT_FALSE(adc->adcSetSampleRate(384000));
}

// ----- 49. adcGetSampleRate() returns the set rate -----
void test_get_sample_rate(void) {
    adc->adcSetSampleRate(96000);
    TEST_ASSERT_EQUAL(96000u, adc->adcGetSampleRate());
}

// ==========================================================================
// Section 9: Filter preset tests (global, single register)
// ==========================================================================

// ----- 50. setFilterPreset(0-7) all succeed -----
void test_filter_preset_valid(void) {
    for (uint8_t i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(adc->setFilterPreset(i));
        TEST_ASSERT_EQUAL(i, adc->_filterPreset);
    }
}

// ----- 51. setFilterPreset(8) returns false -----
void test_filter_preset_invalid(void) {
    TEST_ASSERT_FALSE(adc->setFilterPreset(8));
    TEST_ASSERT_FALSE(adc->setFilterPreset(255));
    // Should remain at default (0)
    TEST_ASSERT_EQUAL(0, adc->_filterPreset);
}

// ==========================================================================
// Section 10: Mute tests
// ==========================================================================

// ----- 52. setMute(true) mutes all channels -----
void test_mute_on(void) {
    adc->init();
    TEST_ASSERT_FALSE(adc->_muted);
    TEST_ASSERT_TRUE(adc->setMute(true));
    TEST_ASSERT_TRUE(adc->_muted);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, adc->_chVolume[i]);
    }
}

// ----- 53. setMute(false) unmutes all channels to 0dB -----
void test_mute_off(void) {
    adc->init();
    adc->setMute(true);
    TEST_ASSERT_TRUE(adc->setMute(false));
    TEST_ASSERT_FALSE(adc->_muted);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x00, adc->_chVolume[i]);
    }
}

// ==========================================================================
// Section 11: Configure tests
// ==========================================================================

// ----- 54. configure(48000, 32) succeeds -----
void test_configure_valid(void) {
    TEST_ASSERT_TRUE(adc->configure(48000, 32));
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(32, adc->_bitDepth);
}

// ----- 55. configure(44100, 24) succeeds -----
void test_configure_44k1_24bit(void) {
    TEST_ASSERT_TRUE(adc->configure(44100, 24));
    TEST_ASSERT_EQUAL(44100u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(24, adc->_bitDepth);
}

// ----- 56. configure(96000, 24) succeeds -----
void test_configure_96k_24bit(void) {
    TEST_ASSERT_TRUE(adc->configure(96000, 24));
    TEST_ASSERT_EQUAL(96000u, adc->_sampleRate);
    TEST_ASSERT_EQUAL(24, adc->_bitDepth);
}

// ----- 57. configure with unsupported rate fails -----
void test_configure_unsupported_rate(void) {
    TEST_ASSERT_FALSE(adc->configure(8000, 16));
    // State should remain at defaults
    TEST_ASSERT_EQUAL(48000u, adc->_sampleRate);
}

// ----- 58. configure with unsupported bit depth fails -----
void test_configure_unsupported_bits(void) {
    TEST_ASSERT_FALSE(adc->configure(48000, 8));
}

// ----- 59. sequential reconfigure updates rate and depth -----
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
    RUN_TEST(test_input_source_name_matches_descriptor);
    RUN_TEST(test_input_source_count);
    RUN_TEST(test_input_source_at_1_is_pair_b);
    RUN_TEST(test_input_source_at_out_of_range);
    RUN_TEST(test_input_source_vu_initial_minus90);
    RUN_TEST(test_input_source_gain_linear_unity);
    RUN_TEST(test_get_input_source_after_deinit);

    // Section 4: Gain
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

    // Section 5: Volume (8-bit)
    RUN_TEST(test_volume_0_percent_is_mute);
    RUN_TEST(test_volume_100_percent_is_0db);
    RUN_TEST(test_volume_50_percent_midrange);
    RUN_TEST(test_volume_all_channels_equal);

    // Section 6: Per-channel volume
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
