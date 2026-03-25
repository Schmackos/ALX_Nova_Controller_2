// test_hal_es9033q.cpp
// Tests a mock HalEs9033Q class modelling the ESS ES9033Q I2C DAC
// with integrated 2Vrms ground-centered line output drivers.
//
// The ES9033Q is a compact ESS SABRE DAC (28-pin QFN) with I2C control:
//   - I2C address 0x48 (default DAC base address, configurable via address pins)
//   - 2-channel (stereo) DAC, 122dB DNR, Hyperstream II
//   - Supported sample rates: 44.1kHz, 48kHz, 96kHz, 192kHz, 384kHz, 768kHz
//   - Hardware volume (attenuation) via I2C register (0x00=0dB, 0xFF=mute, 0.5dB/step)
//   - 8 digital filter presets (0-7)
//   - Mute via I2C attenuation register
//   - Integrated 2Vrms ground-centered line drivers (no external op-amp needed)
//   - Line driver output impedance selectable: ~75Ω to ~600Ω
//   - Chip ID: 0x88
//
// Key behaviours:
//   - probe() returns true (NATIVE_TEST stub)
//   - init()  succeeds, sets _ready=true, _state=AVAILABLE, line drivers enabled
//   - setVolume(): 0-100% maps to 0xFF-0x00 attenuation (inverted)
//   - setMute(): toggles mute via full attenuation
//   - configure(): accepts 44100/48000/96000/192000/384000/768000 with 16/24/32 bit
//   - setFilterPreset(): accepts 0-7; rejects >=8
//   - setLineDriverEnabled(): toggles integrated line output stage
//   - type = HAL_DEV_DAC
//   - bus = HAL_BUS_I2C
//   - capabilities include HAL_CAP_DAC_PATH, HAL_CAP_HW_VOLUME, HAL_CAP_FILTERS,
//                          HAL_CAP_MUTE, HAL_CAP_LINE_DRIVER
//   - NO ADC path, NO MQA
//   - channelCount = 2 (stereo)
//   - healthCheck() returns _ready state
//   - deinit() sets state = REMOVED, _ready = false, disables line drivers

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

// ===== Mock ES9033Q DAC device =====
//
// Models the ESS ES9033Q stereo DAC with integrated line output drivers.
// Volume is attenuation-based: 0% percent → 0xFF attenuation, 100% → 0x00.
// Filter presets 0-7 valid.
// Supported rates: 44100, 48000, 96000, 192000, 384000, 768000 Hz.
//
class HalEs9033QMock : public HalAudioDevice, public HalAudioDacInterface {
public:
    static const uint8_t kI2cAddr = 0x48;
    static const uint8_t kChipId  = 0x88;

    uint32_t _sampleRate        = 48000;
    uint8_t  _bitDepth          = 32;
    uint8_t  _filterPreset      = 0;
    uint8_t  _volume            = 100;    // 0-100 percent
    bool     _muted             = false;
    bool     _lineDriverEnabled = true;   // Integrated line drivers enabled by default

    HalEs9033QMock() {
        strncpy(_descriptor.compatible, "ess,es9033q", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "ES9033Q", 32);
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
        _descriptor.capabilities = HAL_CAP_DAC_PATH  | HAL_CAP_HW_VOLUME |
                                   HAL_CAP_FILTERS   | HAL_CAP_MUTE | HAL_CAP_LINE_DRIVER;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    // ----- HalDevice lifecycle -----

    bool probe() override { return true; }  // NATIVE_TEST stub

    HalInitResult init() override {
        _state = HAL_STATE_AVAILABLE;
        _ready = true;
        _lineDriverEnabled = true;  // Line drivers enabled by default at init
        return hal_init_ok();
    }

    void deinit() override {
        _ready = false;
        _lineDriverEnabled = false;  // Power down output stage on deinit
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

    // ----- ES9033Q-specific: filter preset -----

    bool setFilterPreset(uint8_t preset) {
        if (preset >= 8) return false;
        _filterPreset = preset;
        return true;
    }

    uint8_t getFilterPreset() const { return _filterPreset; }

    // ----- ES9033Q-specific: integrated line driver control -----

    bool setLineDriverEnabled(bool enable) {
        _lineDriverEnabled = enable;
        return true;
    }
};

// ===== Fixtures =====
static HalEs9033QMock* dac;

void setUp(void) {
    WireMock::reset();
    dac = new HalEs9033QMock();
}

void tearDown(void) {
    delete dac;
    dac = nullptr;
}

// ==========================================================================
// Section 1: Descriptor tests
// ==========================================================================

// ----- 1. descriptor compatible string is "ess,es9033q" -----
void test_descriptor_compatible_string(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9033q", dac->getDescriptor().compatible);
}

// ----- 2. descriptor type is HAL_DEV_DAC -----
void test_descriptor_type_is_dac(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, (int)dac->getDescriptor().type);
}

// ----- 3. capabilities include DAC_PATH, HW_VOLUME, FILTERS, MUTE, LINE_DRIVER -----
void test_descriptor_capabilities(void) {
    uint32_t caps = dac->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(caps & HAL_CAP_FILTERS);
    TEST_ASSERT_TRUE(caps & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(caps & HAL_CAP_LINE_DRIVER);
    // DAC-only: no ADC path or MQA
    TEST_ASSERT_FALSE(caps & HAL_CAP_ADC_PATH);
    TEST_ASSERT_FALSE(caps & HAL_CAP_MQA);
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

// ----- 10. init() enables line drivers by default -----
void test_init_enables_line_drivers(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->_lineDriverEnabled);
}

// ----- 11. deinit() clears _ready, disables line drivers, sets state=REMOVED -----
void test_deinit_clears_ready_and_line_drivers(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->_lineDriverEnabled);
    dac->deinit();
    TEST_ASSERT_FALSE(dac->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)dac->_state);
    TEST_ASSERT_FALSE(dac->_lineDriverEnabled);
}

// ----- 12. healthCheck() returns true after init -----
void test_health_check_after_init(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->healthCheck());
}

// ----- 13. healthCheck() returns false before init -----
void test_health_check_before_init(void) {
    TEST_ASSERT_FALSE(dac->healthCheck());
}

// ==========================================================================
// Section 3: Volume tests
// ==========================================================================

// ----- 14. setVolume(100) succeeds (0 dB, no attenuation) -----
void test_volume_100_percent(void) {
    dac->_volume = 0;
    TEST_ASSERT_TRUE(dac->setVolume(100));
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ----- 15. setVolume(0) succeeds (full attenuation) -----
void test_volume_0_percent(void) {
    TEST_ASSERT_TRUE(dac->setVolume(0));
    TEST_ASSERT_EQUAL(0, dac->_volume);
}

// ----- 16. setVolume(50) succeeds (mid-range) -----
void test_volume_50_percent(void) {
    TEST_ASSERT_TRUE(dac->setVolume(50));
    TEST_ASSERT_EQUAL(50, dac->_volume);
}

// ----- 17. setVolume(>100) clamps to 100 -----
void test_volume_clamped_above_100(void) {
    TEST_ASSERT_TRUE(dac->setVolume(200));
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ----- 18. dacSetVolume() delegates to setVolume() -----
void test_dac_set_volume_delegates(void) {
    dac->dacSetVolume(60);
    TEST_ASSERT_EQUAL(60, dac->dacGetVolume());
}

// ==========================================================================
// Section 4: Mute tests
// ==========================================================================

// ----- 19. setMute(true) engages mute -----
void test_mute_on(void) {
    TEST_ASSERT_FALSE(dac->_muted);
    TEST_ASSERT_TRUE(dac->setMute(true));
    TEST_ASSERT_TRUE(dac->_muted);
    TEST_ASSERT_TRUE(dac->dacIsMuted());
}

// ----- 20. setMute(false) disengages mute -----
void test_mute_off(void) {
    dac->_muted = true;
    TEST_ASSERT_TRUE(dac->setMute(false));
    TEST_ASSERT_FALSE(dac->_muted);
    TEST_ASSERT_FALSE(dac->dacIsMuted());
}

// ----- 21. mute/unmute cycle restores volume field -----
void test_mute_unmute_cycle(void) {
    dac->setVolume(85);
    dac->setMute(true);
    TEST_ASSERT_TRUE(dac->dacIsMuted());
    dac->setMute(false);
    TEST_ASSERT_FALSE(dac->dacIsMuted());
    TEST_ASSERT_EQUAL(85, dac->dacGetVolume());
}

// ==========================================================================
// Section 5: Configure tests
// ==========================================================================

// ----- 22. configure(48000, 32) succeeds -----
void test_configure_48k_32bit(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);
}

// ----- 23. configure(44100, 24) succeeds -----
void test_configure_44k1_24bit(void) {
    TEST_ASSERT_TRUE(dac->configure(44100, 24));
    TEST_ASSERT_EQUAL(44100u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);
}

// ----- 24. configure(96000, 16) succeeds -----
void test_configure_96k_16bit(void) {
    TEST_ASSERT_TRUE(dac->configure(96000, 16));
    TEST_ASSERT_EQUAL(96000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(16, dac->_bitDepth);
}

// ----- 25. configure(192000, 32) succeeds -----
void test_configure_192k_32bit(void) {
    TEST_ASSERT_TRUE(dac->configure(192000, 32));
    TEST_ASSERT_EQUAL(192000u, dac->_sampleRate);
}

// ----- 26. configure(384000, 32) succeeds -----
void test_configure_384k_32bit(void) {
    TEST_ASSERT_TRUE(dac->configure(384000, 32));
    TEST_ASSERT_EQUAL(384000u, dac->_sampleRate);
}

// ----- 27. configure(768000, 32) succeeds -----
void test_configure_768k_32bit(void) {
    TEST_ASSERT_TRUE(dac->configure(768000, 32));
    TEST_ASSERT_EQUAL(768000u, dac->_sampleRate);
}

// ----- 28. configure with unsupported rate fails -----
void test_configure_unsupported_rate(void) {
    TEST_ASSERT_FALSE(dac->configure(8000, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
}

// ----- 29. configure with unsupported bit depth fails -----
void test_configure_unsupported_bits(void) {
    TEST_ASSERT_FALSE(dac->configure(48000, 8));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
}

// ----- 30. sequential reconfigure updates rate and depth -----
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

// ----- 31. dacSetSampleRate() delegates to configure() -----
void test_dac_set_sample_rate_delegates(void) {
    TEST_ASSERT_TRUE(dac->dacSetSampleRate(96000));
    TEST_ASSERT_EQUAL(96000u, dac->dacGetSampleRate());
}

// ==========================================================================
// Section 6: Filter preset tests
// ==========================================================================

// ----- 32. setFilterPreset(0-7) all succeed -----
void test_filter_preset_valid(void) {
    for (uint8_t i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(dac->setFilterPreset(i));
        TEST_ASSERT_EQUAL(i, dac->getFilterPreset());
    }
}

// ----- 33. setFilterPreset(8) returns false -----
void test_filter_preset_invalid(void) {
    TEST_ASSERT_FALSE(dac->setFilterPreset(8));
    TEST_ASSERT_FALSE(dac->setFilterPreset(255));
    TEST_ASSERT_EQUAL(0, dac->getFilterPreset());
}

// ----- 34. filter preset persists across multiple sets -----
void test_filter_preset_persistence(void) {
    dac->setFilterPreset(5);
    dac->setFilterPreset(7);
    TEST_ASSERT_EQUAL(7, dac->getFilterPreset());
    dac->setFilterPreset(2);
    TEST_ASSERT_EQUAL(2, dac->getFilterPreset());
}

// ==========================================================================
// Section 7: DacInterface delegation tests
// ==========================================================================

// ----- 35. dacGetVolume() returns current volume -----
void test_dac_get_volume(void) {
    dac->setVolume(42);
    TEST_ASSERT_EQUAL(42, dac->dacGetVolume());
}

// ----- 36. dacGetSampleRate() returns current rate -----
void test_dac_get_sample_rate(void) {
    dac->configure(384000, 32);
    TEST_ASSERT_EQUAL(384000u, dac->dacGetSampleRate());
}

// ----- 37. dacSetBitDepth() accepts valid depths -----
void test_dac_set_bit_depth_valid(void) {
    TEST_ASSERT_TRUE(dac->dacSetBitDepth(16));
    TEST_ASSERT_EQUAL(16, dac->_bitDepth);
    TEST_ASSERT_TRUE(dac->dacSetBitDepth(24));
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);
    TEST_ASSERT_TRUE(dac->dacSetBitDepth(32));
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);
}

// ----- 38. dacSetBitDepth() rejects invalid depths -----
void test_dac_set_bit_depth_invalid(void) {
    TEST_ASSERT_FALSE(dac->dacSetBitDepth(8));
    TEST_ASSERT_FALSE(dac->dacSetBitDepth(20));
}

// ==========================================================================
// Section 8: Descriptor string tests
// ==========================================================================

// ----- 39. descriptor name is "ES9033Q" -----
void test_descriptor_name(void) {
    TEST_ASSERT_EQUAL_STRING("ES9033Q", dac->getDescriptor().name);
}

// ----- 40. descriptor manufacturer is "ESS Technology" -----
void test_descriptor_manufacturer(void) {
    TEST_ASSERT_EQUAL_STRING("ESS Technology", dac->getDescriptor().manufacturer);
}

// ----- 41. init priority is HAL_PRIORITY_HARDWARE -----
void test_init_priority(void) {
    TEST_ASSERT_EQUAL(HAL_PRIORITY_HARDWARE, dac->getInitPriority());
}

// ==========================================================================
// Section 9: State machine tests
// ==========================================================================

// ----- 42. state is UNKNOWN before init -----
void test_state_unknown_before_init(void) {
    TEST_ASSERT_EQUAL(HAL_STATE_UNKNOWN, (int)dac->_state);
}

// ----- 43. state is AVAILABLE after init -----
void test_state_available_after_init(void) {
    dac->init();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)dac->_state);
}

// ----- 44. state is REMOVED after deinit -----
void test_state_removed_after_deinit(void) {
    dac->init();
    dac->deinit();
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)dac->_state);
}

// ==========================================================================
// Section 10: Integrated line driver tests
// ==========================================================================

// ----- 45. line driver is enabled by default after construction -----
void test_line_driver_enabled_by_default(void) {
    TEST_ASSERT_TRUE(dac->_lineDriverEnabled);
}

// ----- 46. setLineDriverEnabled(false) disables line drivers -----
void test_line_driver_disable(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->_lineDriverEnabled);
    TEST_ASSERT_TRUE(dac->setLineDriverEnabled(false));
    TEST_ASSERT_FALSE(dac->_lineDriverEnabled);
}

// ----- 47. setLineDriverEnabled(true) re-enables line drivers -----
void test_line_driver_enable(void) {
    dac->init();
    dac->setLineDriverEnabled(false);
    TEST_ASSERT_FALSE(dac->_lineDriverEnabled);
    TEST_ASSERT_TRUE(dac->setLineDriverEnabled(true));
    TEST_ASSERT_TRUE(dac->_lineDriverEnabled);
}

// ----- 48. line driver enable/disable cycle -----
void test_line_driver_enable_disable_cycle(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->_lineDriverEnabled);   // Enabled after init

    dac->setLineDriverEnabled(false);
    TEST_ASSERT_FALSE(dac->_lineDriverEnabled);

    dac->setLineDriverEnabled(true);
    TEST_ASSERT_TRUE(dac->_lineDriverEnabled);

    dac->setLineDriverEnabled(false);
    TEST_ASSERT_FALSE(dac->_lineDriverEnabled);
}

// ----- 49. deinit disables line drivers regardless of prior state -----
void test_deinit_disables_line_drivers(void) {
    dac->init();
    dac->setLineDriverEnabled(true);
    TEST_ASSERT_TRUE(dac->_lineDriverEnabled);
    dac->deinit();
    TEST_ASSERT_FALSE(dac->_lineDriverEnabled);
}

// ----- 50. line driver state does not affect volume or mute -----
void test_line_driver_independent_of_volume(void) {
    dac->init();
    dac->setVolume(70);
    dac->setLineDriverEnabled(false);
    // Volume should remain unchanged after toggling line drivers
    TEST_ASSERT_EQUAL(70, dac->dacGetVolume());
    TEST_ASSERT_FALSE(dac->dacIsMuted());
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
    RUN_TEST(test_init_enables_line_drivers);
    RUN_TEST(test_deinit_clears_ready_and_line_drivers);
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

    // Section 10: Integrated line driver
    RUN_TEST(test_line_driver_enabled_by_default);
    RUN_TEST(test_line_driver_disable);
    RUN_TEST(test_line_driver_enable);
    RUN_TEST(test_line_driver_enable_disable_cycle);
    RUN_TEST(test_deinit_disables_line_drivers);
    RUN_TEST(test_line_driver_independent_of_volume);

    return UNITY_END();
}
