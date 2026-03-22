// test_hal_es9069q.cpp
// Tests a mock HalEs9069Q class modelling the ESS ES9069Q I2C DAC
// with integrated MQA hardware renderer.
//
// The ES9069Q is a high-performance ESS SABRE DAC with I2C control:
//   - I2C address 0x48 (default DAC base address, configurable via address pins)
//   - 2-channel (stereo) DAC
//   - Supported sample rates: 44.1kHz, 48kHz, 96kHz, 192kHz, 384kHz, 768kHz
//   - Hardware volume (attenuation) via I2C register (0x00=0dB, 0xFF=mute, 0.5dB/step)
//   - 8 digital filter presets (0-7)
//   - Mute via I2C attenuation register
//   - Integrated MQA hardware renderer (PCM → MQA unfold in silicon)
//   - DSD1024 support
//   - Chip ID: 0x94
//
// Key behaviours:
//   - probe() returns true (NATIVE_TEST stub)
//   - init()  succeeds, sets _ready=true, _state=AVAILABLE
//   - setVolume(): 0-100% maps to 0xFF-0x00 attenuation (inverted)
//   - setMute(): toggles mute via full attenuation
//   - configure(): accepts 44100/48000/96000/192000/384000/768000 with 16/24/32 bit
//   - setFilterPreset(): accepts 0-7; rejects >=8
//   - setMqaEnabled(): toggles MQA hardware renderer enable bit
//   - isMqaActive(): reads MQA decode status bits (simulated in mock)
//   - type = HAL_DEV_DAC
//   - bus = HAL_BUS_I2C
//   - capabilities include HAL_CAP_DAC_PATH, HAL_CAP_HW_VOLUME, HAL_CAP_FILTERS,
//                          HAL_CAP_MUTE, HAL_CAP_MQA
//   - NO ADC path
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

// ===== Inline capability flags (guard against missing definitions) =====
#ifndef HAL_CAP_MQA
#define HAL_CAP_MQA          (1 << 8)
#endif
#ifndef HAL_CAP_LINE_DRIVER
#define HAL_CAP_LINE_DRIVER  (1 << 9)
#endif

// ===== MQA decode status constants (mirrors es9069q_regs.h) =====
#define MOCK_MQA_STATUS_NONE    0x00
#define MOCK_MQA_STATUS_CORE    0x01
#define MOCK_MQA_STATUS_STUDIO  0x02

// ===== Mock ES9069Q DAC device =====
//
// Models the ESS ES9069Q stereo DAC with MQA hardware renderer.
// Volume is attenuation-based: 0% percent → 0xFF attenuation, 100% → 0x00.
// Filter presets 0-7 valid.
// Supported rates: 44100, 48000, 96000, 192000, 384000, 768000 Hz.
//
class HalEs9069QMock : public HalAudioDevice, public HalAudioDacInterface {
public:
    static const uint8_t kI2cAddr = 0x48;
    static const uint8_t kChipId  = 0x94;

    uint32_t _sampleRate    = 48000;
    uint8_t  _bitDepth      = 32;
    uint8_t  _filterPreset  = 0;
    uint8_t  _volume        = 100;    // 0-100 percent
    bool     _muted         = false;
    bool     _mqaEnabled    = false;
    uint8_t  _mqaStatus     = MOCK_MQA_STATUS_NONE;  // Simulated hardware decode status

    HalEs9069QMock() {
        strncpy(_descriptor.compatible, "ess,es9069q", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "ES9069Q", 32);
        _descriptor.name[32] = '\0';
        strncpy(_descriptor.manufacturer, "ESS Technology", 32);
        _descriptor.manufacturer[32] = '\0';
        _descriptor.type         = HAL_DEV_DAC;
        _descriptor.i2cAddr      = kI2cAddr;
        _descriptor.channelCount = 2;    // Stereo DAC
        _descriptor.bus.type     = HAL_BUS_I2C;
        _descriptor.bus.index    = HAL_I2C_BUS_EXP;
        _descriptor.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K  | HAL_RATE_96K |
                                      HAL_RATE_192K  | HAL_RATE_384K | HAL_RATE_768K;
        _descriptor.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME |
                                   HAL_CAP_FILTERS  | HAL_CAP_MUTE | HAL_CAP_MQA;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    // ----- HalDevice lifecycle -----

    bool probe() override { return true; }  // NATIVE_TEST stub

    HalInitResult init() override {
        _state = HAL_STATE_AVAILABLE;
        _ready = true;
        return hal_init_ok();
    }

    void deinit() override {
        _ready = false;
        _mqaEnabled = false;
        _mqaStatus  = MOCK_MQA_STATUS_NONE;
        _state = HAL_STATE_REMOVED;
    }

    void dumpConfig() override {}

    bool healthCheck() override { return _ready; }

    // ----- HalAudioDevice -----

    bool configure(uint32_t rate, uint8_t bits) override {
        const uint32_t supported[] = { 44100, 48000, 96000, 192000, 384000, 768000 };
        bool valid = false;
        for (uint8_t i = 0; i < 6; i++) {
            if (rate == supported[i]) { valid = true; break; }
        }
        if (!valid) return false;
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

    bool     dacSetVolume(uint8_t pct) override   { return setVolume(pct); }
    bool     dacSetMute(bool m) override          { return setMute(m); }
    uint8_t  dacGetVolume() const override        { return _volume; }
    bool     dacIsMuted() const override          { return _muted; }
    bool     dacSetSampleRate(uint32_t hz) override {
        return configure(hz, _bitDepth);
    }
    bool     dacSetBitDepth(uint8_t bits) override {
        return configure(_sampleRate, bits);
    }
    uint32_t dacGetSampleRate() const override    { return _sampleRate; }

    // ----- ES9069Q-specific: filter preset -----

    bool setFilterPreset(uint8_t preset) {
        if (preset >= 8) return false;
        _filterPreset = preset;
        return true;
    }

    uint8_t getFilterPreset() const { return _filterPreset; }

    // ----- ES9069Q-specific: MQA hardware renderer -----

    bool setMqaEnabled(bool enable) {
        _mqaEnabled = enable;
        // Simulate: enabling MQA eventually transitions status to CORE
        // (In real hardware, status depends on the input stream content)
        if (!enable) {
            _mqaStatus = MOCK_MQA_STATUS_NONE;
        }
        return true;
    }

    bool isMqaActive() const {
        return (_mqaStatus != MOCK_MQA_STATUS_NONE);
    }

    // Test helper: simulate hardware detecting MQA stream
    void simulateMqaDetect(uint8_t status) {
        _mqaStatus = status;
    }
};

// ===== Fixtures =====
static HalEs9069QMock* dac;

void setUp(void) {
    WireMock::reset();
    dac = new HalEs9069QMock();
}

void tearDown(void) {
    delete dac;
    dac = nullptr;
}

// ==========================================================================
// Section 1: Descriptor tests
// ==========================================================================

// ----- 1. descriptor compatible string is "ess,es9069q" -----
void test_descriptor_compatible_string(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9069q", dac->getDescriptor().compatible);
}

// ----- 2. descriptor type is HAL_DEV_DAC -----
void test_descriptor_type_is_dac(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, (int)dac->getDescriptor().type);
}

// ----- 3. capabilities include DAC_PATH, HW_VOLUME, FILTERS, MUTE, MQA -----
void test_descriptor_capabilities(void) {
    uint16_t caps = dac->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(caps & HAL_CAP_FILTERS);
    TEST_ASSERT_TRUE(caps & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(caps & HAL_CAP_MQA);
    // DAC-only: no ADC path
    TEST_ASSERT_FALSE(caps & HAL_CAP_ADC_PATH);
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

// ----- 7. sample rates mask includes 44K1, 48K, 96K, 192K, 384K, 768K -----
void test_descriptor_sample_rates_mask(void) {
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

// ----- 10. deinit() clears _ready, disables MQA, sets state=REMOVED -----
void test_deinit_clears_ready_and_mqa(void) {
    dac->init();
    dac->_mqaEnabled = true;
    dac->_mqaStatus  = MOCK_MQA_STATUS_CORE;
    dac->deinit();
    TEST_ASSERT_FALSE(dac->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)dac->_state);
    TEST_ASSERT_FALSE(dac->_mqaEnabled);
    TEST_ASSERT_FALSE(dac->isMqaActive());
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

// ==========================================================================
// Section 3: Volume tests
// ==========================================================================

// ----- 13. setVolume(100) succeeds (0 dB, no attenuation) -----
void test_volume_100_percent(void) {
    dac->_volume = 0;
    TEST_ASSERT_TRUE(dac->setVolume(100));
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ----- 14. setVolume(0) succeeds (full attenuation) -----
void test_volume_0_percent(void) {
    TEST_ASSERT_TRUE(dac->setVolume(0));
    TEST_ASSERT_EQUAL(0, dac->_volume);
}

// ----- 15. setVolume(50) succeeds (mid-range) -----
void test_volume_50_percent(void) {
    TEST_ASSERT_TRUE(dac->setVolume(50));
    TEST_ASSERT_EQUAL(50, dac->_volume);
}

// ----- 16. setVolume(>100) clamps to 100 -----
void test_volume_clamped_above_100(void) {
    TEST_ASSERT_TRUE(dac->setVolume(200));
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ----- 17. dacSetVolume() delegates to setVolume() -----
void test_dac_set_volume_delegates(void) {
    dac->dacSetVolume(75);
    TEST_ASSERT_EQUAL(75, dac->dacGetVolume());
}

// ==========================================================================
// Section 4: Mute tests
// ==========================================================================

// ----- 18. setMute(true) engages mute -----
void test_mute_on(void) {
    TEST_ASSERT_FALSE(dac->_muted);
    TEST_ASSERT_TRUE(dac->setMute(true));
    TEST_ASSERT_TRUE(dac->_muted);
    TEST_ASSERT_TRUE(dac->dacIsMuted());
}

// ----- 19. setMute(false) disengages mute -----
void test_mute_off(void) {
    dac->_muted = true;
    TEST_ASSERT_TRUE(dac->setMute(false));
    TEST_ASSERT_FALSE(dac->_muted);
    TEST_ASSERT_FALSE(dac->dacIsMuted());
}

// ----- 20. mute/unmute cycle restores volume field -----
void test_mute_unmute_cycle(void) {
    dac->setVolume(80);
    dac->setMute(true);
    TEST_ASSERT_TRUE(dac->dacIsMuted());
    dac->setMute(false);
    TEST_ASSERT_FALSE(dac->dacIsMuted());
    TEST_ASSERT_EQUAL(80, dac->dacGetVolume());
}

// ==========================================================================
// Section 5: Configure tests
// ==========================================================================

// ----- 21. configure(48000, 32) succeeds -----
void test_configure_48k_32bit(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);
}

// ----- 22. configure(44100, 24) succeeds -----
void test_configure_44k1_24bit(void) {
    TEST_ASSERT_TRUE(dac->configure(44100, 24));
    TEST_ASSERT_EQUAL(44100u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);
}

// ----- 23. configure(96000, 16) succeeds -----
void test_configure_96k_16bit(void) {
    TEST_ASSERT_TRUE(dac->configure(96000, 16));
    TEST_ASSERT_EQUAL(96000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(16, dac->_bitDepth);
}

// ----- 24. configure(192000, 32) succeeds -----
void test_configure_192k_32bit(void) {
    TEST_ASSERT_TRUE(dac->configure(192000, 32));
    TEST_ASSERT_EQUAL(192000u, dac->_sampleRate);
}

// ----- 25. configure(384000, 32) succeeds -----
void test_configure_384k_32bit(void) {
    TEST_ASSERT_TRUE(dac->configure(384000, 32));
    TEST_ASSERT_EQUAL(384000u, dac->_sampleRate);
}

// ----- 26. configure(768000, 32) succeeds -----
void test_configure_768k_32bit(void) {
    TEST_ASSERT_TRUE(dac->configure(768000, 32));
    TEST_ASSERT_EQUAL(768000u, dac->_sampleRate);
}

// ----- 27. configure with unsupported rate fails -----
void test_configure_unsupported_rate(void) {
    TEST_ASSERT_FALSE(dac->configure(8000, 32));
    // State should remain at defaults
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
}

// ----- 28. configure with unsupported bit depth fails -----
void test_configure_unsupported_bits(void) {
    TEST_ASSERT_FALSE(dac->configure(48000, 8));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
}

// ----- 29. sequential reconfigure updates rate and depth -----
void test_sequential_reconfigure(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 24));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);

    TEST_ASSERT_TRUE(dac->configure(768000, 32));
    TEST_ASSERT_EQUAL(768000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);

    TEST_ASSERT_TRUE(dac->configure(44100, 16));
    TEST_ASSERT_EQUAL(44100u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(16, dac->_bitDepth);
}

// ----- 30. dacSetSampleRate() delegates to configure() -----
void test_dac_set_sample_rate_delegates(void) {
    TEST_ASSERT_TRUE(dac->dacSetSampleRate(96000));
    TEST_ASSERT_EQUAL(96000u, dac->dacGetSampleRate());
}

// ==========================================================================
// Section 6: Filter preset tests
// ==========================================================================

// ----- 31. setFilterPreset(0-7) all succeed -----
void test_filter_preset_valid(void) {
    for (uint8_t i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(dac->setFilterPreset(i));
        TEST_ASSERT_EQUAL(i, dac->getFilterPreset());
    }
}

// ----- 32. setFilterPreset(8) returns false -----
void test_filter_preset_invalid(void) {
    TEST_ASSERT_FALSE(dac->setFilterPreset(8));
    TEST_ASSERT_FALSE(dac->setFilterPreset(255));
    // Should remain at default (0)
    TEST_ASSERT_EQUAL(0, dac->getFilterPreset());
}

// ----- 33. filter preset persists across multiple sets -----
void test_filter_preset_persistence(void) {
    dac->setFilterPreset(3);
    dac->setFilterPreset(7);
    TEST_ASSERT_EQUAL(7, dac->getFilterPreset());
    dac->setFilterPreset(1);
    TEST_ASSERT_EQUAL(1, dac->getFilterPreset());
}

// ==========================================================================
// Section 7: DacInterface delegation tests
// ==========================================================================

// ----- 34. dacGetVolume() returns current volume -----
void test_dac_get_volume(void) {
    dac->setVolume(65);
    TEST_ASSERT_EQUAL(65, dac->dacGetVolume());
}

// ----- 35. dacGetSampleRate() returns current rate -----
void test_dac_get_sample_rate(void) {
    dac->configure(192000, 32);
    TEST_ASSERT_EQUAL(192000u, dac->dacGetSampleRate());
}

// ----- 36. dacSetBitDepth() accepts valid depths -----
void test_dac_set_bit_depth_valid(void) {
    TEST_ASSERT_TRUE(dac->dacSetBitDepth(16));
    TEST_ASSERT_EQUAL(16, dac->_bitDepth);
    TEST_ASSERT_TRUE(dac->dacSetBitDepth(24));
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);
    TEST_ASSERT_TRUE(dac->dacSetBitDepth(32));
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);
}

// ----- 37. dacSetBitDepth() rejects invalid depths -----
void test_dac_set_bit_depth_invalid(void) {
    TEST_ASSERT_FALSE(dac->dacSetBitDepth(8));
    TEST_ASSERT_FALSE(dac->dacSetBitDepth(20));
}

// ==========================================================================
// Section 8: Descriptor string tests
// ==========================================================================

// ----- 38. descriptor name is "ES9069Q" -----
void test_descriptor_name(void) {
    TEST_ASSERT_EQUAL_STRING("ES9069Q", dac->getDescriptor().name);
}

// ----- 39. descriptor manufacturer is "ESS Technology" -----
void test_descriptor_manufacturer(void) {
    TEST_ASSERT_EQUAL_STRING("ESS Technology", dac->getDescriptor().manufacturer);
}

// ----- 40. init priority is HAL_PRIORITY_HARDWARE -----
void test_init_priority(void) {
    TEST_ASSERT_EQUAL(HAL_PRIORITY_HARDWARE, dac->getInitPriority());
}

// ==========================================================================
// Section 9: State machine tests
// ==========================================================================

// ----- 41. state is UNKNOWN before init -----
void test_state_unknown_before_init(void) {
    TEST_ASSERT_EQUAL(HAL_STATE_UNKNOWN, (int)dac->_state);
}

// ----- 42. state is AVAILABLE after init -----
void test_state_available_after_init(void) {
    dac->init();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)dac->_state);
}

// ----- 43. state is REMOVED after deinit -----
void test_state_removed_after_deinit(void) {
    dac->init();
    dac->deinit();
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)dac->_state);
}

// ==========================================================================
// Section 10: MQA hardware renderer tests
// ==========================================================================

// ----- 44. setMqaEnabled(true) sets _mqaEnabled -----
void test_mqa_enable(void) {
    TEST_ASSERT_FALSE(dac->_mqaEnabled);
    TEST_ASSERT_TRUE(dac->setMqaEnabled(true));
    TEST_ASSERT_TRUE(dac->_mqaEnabled);
}

// ----- 45. setMqaEnabled(false) clears _mqaEnabled and resets status -----
void test_mqa_disable(void) {
    dac->_mqaEnabled = true;
    dac->simulateMqaDetect(MOCK_MQA_STATUS_CORE);
    TEST_ASSERT_TRUE(dac->isMqaActive());

    TEST_ASSERT_TRUE(dac->setMqaEnabled(false));
    TEST_ASSERT_FALSE(dac->_mqaEnabled);
    TEST_ASSERT_FALSE(dac->isMqaActive());
}

// ----- 46. isMqaActive() returns false when status is NONE -----
void test_mqa_inactive_by_default(void) {
    TEST_ASSERT_FALSE(dac->isMqaActive());
}

// ----- 47. isMqaActive() returns true when status is CORE -----
void test_mqa_active_core(void) {
    dac->simulateMqaDetect(MOCK_MQA_STATUS_CORE);
    TEST_ASSERT_TRUE(dac->isMqaActive());
}

// ----- 48. isMqaActive() returns true when status is STUDIO -----
void test_mqa_active_studio(void) {
    dac->simulateMqaDetect(MOCK_MQA_STATUS_STUDIO);
    TEST_ASSERT_TRUE(dac->isMqaActive());
}

// ----- 49. MQA enable/disable cycle returns to inactive -----
void test_mqa_enable_disable_cycle(void) {
    dac->setMqaEnabled(true);
    dac->simulateMqaDetect(MOCK_MQA_STATUS_STUDIO);
    TEST_ASSERT_TRUE(dac->isMqaActive());

    dac->setMqaEnabled(false);
    TEST_ASSERT_FALSE(dac->isMqaActive());
    TEST_ASSERT_FALSE(dac->_mqaEnabled);
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
    RUN_TEST(test_descriptor_sample_rates_mask);

    // Section 2: Lifecycle
    RUN_TEST(test_probe_returns_true);
    RUN_TEST(test_init_success);
    RUN_TEST(test_deinit_clears_ready_and_mqa);
    RUN_TEST(test_health_check_after_init);
    RUN_TEST(test_health_check_before_init);

    // Section 3: Volume
    RUN_TEST(test_volume_100_percent);
    RUN_TEST(test_volume_0_percent);
    RUN_TEST(test_volume_50_percent);
    RUN_TEST(test_volume_clamped_above_100);
    RUN_TEST(test_dac_set_volume_delegates);

    // Section 4: Mute
    RUN_TEST(test_mute_on);
    RUN_TEST(test_mute_off);
    RUN_TEST(test_mute_unmute_cycle);

    // Section 5: Configure
    RUN_TEST(test_configure_48k_32bit);
    RUN_TEST(test_configure_44k1_24bit);
    RUN_TEST(test_configure_96k_16bit);
    RUN_TEST(test_configure_192k_32bit);
    RUN_TEST(test_configure_384k_32bit);
    RUN_TEST(test_configure_768k_32bit);
    RUN_TEST(test_configure_unsupported_rate);
    RUN_TEST(test_configure_unsupported_bits);
    RUN_TEST(test_sequential_reconfigure);
    RUN_TEST(test_dac_set_sample_rate_delegates);

    // Section 6: Filter preset
    RUN_TEST(test_filter_preset_valid);
    RUN_TEST(test_filter_preset_invalid);
    RUN_TEST(test_filter_preset_persistence);

    // Section 7: DacInterface delegation
    RUN_TEST(test_dac_get_volume);
    RUN_TEST(test_dac_get_sample_rate);
    RUN_TEST(test_dac_set_bit_depth_valid);
    RUN_TEST(test_dac_set_bit_depth_invalid);

    // Section 8: Descriptor strings
    RUN_TEST(test_descriptor_name);
    RUN_TEST(test_descriptor_manufacturer);
    RUN_TEST(test_init_priority);

    // Section 9: State machine
    RUN_TEST(test_state_unknown_before_init);
    RUN_TEST(test_state_available_after_init);
    RUN_TEST(test_state_removed_after_deinit);

    // Section 10: MQA hardware renderer
    RUN_TEST(test_mqa_enable);
    RUN_TEST(test_mqa_disable);
    RUN_TEST(test_mqa_inactive_by_default);
    RUN_TEST(test_mqa_active_core);
    RUN_TEST(test_mqa_active_studio);
    RUN_TEST(test_mqa_enable_disable_cycle);

    return UNITY_END();
}
