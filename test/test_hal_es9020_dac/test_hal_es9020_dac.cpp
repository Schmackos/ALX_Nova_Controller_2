// test_hal_es9020_dac.cpp
// Tests a mock HalEs9020Dac class modelling the ESS ES9020 DAC.
//
// The ES9020 is a high-performance ESS SABRE DAC with I2C control:
//   - I2C address 0x48 (default, configurable via address pins)
//   - 2-channel (stereo) DAC
//   - Supported sample rates: 44.1kHz, 48kHz, 96kHz, 192kHz
//   - 8-bit attenuation volume (0.5 dB/step, 0x00=0dB, 0xFF=muted)
//   - 8 digital filter presets (0-7)
//   - Mute via full attenuation register write
//   - Integrated APLL for BCK clock recovery
//   - Hyperstream IV architecture, 122 dB DNR
//   - buildSink() populates AudioOutputSink struct
//
// Key behaviours:
//   - probe() returns true (NATIVE_TEST stub)
//   - init() succeeds, sets _ready=true, _state=AVAILABLE
//   - setVolume(): 0-100% maps to 8-bit attenuation (0.5 dB/step)
//   - setMute(true) writes full attenuation; setMute(false) restores volume
//   - setFilterPreset(): accepts 0-7; rejects >=8
//   - configure(): accepts 44100/48000/96000/192000; rejects others
//   - type = HAL_DEV_DAC
//   - bus = HAL_BUS_I2C
//   - capabilities include HAL_CAP_DAC_PATH, HAL_CAP_HW_VOLUME, HAL_CAP_MUTE,
//                          HAL_CAP_FILTERS, HAL_CAP_APLL
//   - NO ADC path
//   - channelCount = 2 (stereo)
//   - healthCheck() returns _initialized state
//   - deinit() sets state = REMOVED, _ready = false
//   - setApllEnabled(true/false) toggles APLL enable flag
//   - isApllLocked() reads APLL lock status (false in native test — no hardware)
//
// Section layout:
//   1. Descriptor tests
//   2. Lifecycle tests
//   3. Volume tests
//   4. Mute tests
//   5. Filter preset tests
//   6. Sample rate / configure tests
//   7. Configure tests
//   8. DAC interface tests (dacSetVolume, dacGetVolume, dacIsMuted, dacSetSampleRate)
//   9. buildSink tests
//  10. APLL tests

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
#include "../../src/drivers/ess_sabre_common.h"

// Guard needed: audio_output_sink.h defines AUDIO_OUT_MAX_SINKS but not the matrix size
#ifndef AUDIO_PIPELINE_MATRIX_SIZE
#define AUDIO_PIPELINE_MATRIX_SIZE 16
#endif

// ===== Inline capability flag guards =====
#ifndef HAL_CAP_DAC_PATH
#define HAL_CAP_DAC_PATH     (1 << 4)
#endif
#ifndef HAL_CAP_APLL
#define HAL_CAP_APLL         (1 << 10)
#endif
#ifndef HAL_CAP_MUTE
#define HAL_CAP_MUTE         (1 << 2)
#endif
#ifndef HAL_CAP_FILTERS
#define HAL_CAP_FILTERS      (1 << 1)
#endif
#ifndef HAL_CAP_HW_VOLUME
#define HAL_CAP_HW_VOLUME    (1 << 0)
#endif

// ===== ES9020 register constants (duplicated for test isolation) =====
#define MOCK_ES9020_CHIP_ID          0x86
#define MOCK_ES9020_I2C_ADDR         0x48
#define MOCK_ES9020_VOL_0DB          0x00
#define MOCK_ES9020_VOL_MUTE         0xFF
#define MOCK_ES9020_APLL_ENABLE_BIT  0x01
#define MOCK_ES9020_APLL_LOCK_BIT    0x10

// ===== Mock ES9020 DAC device =====
//
// Models the ESS ES9020 stereo DAC with I2C control interface and APLL.
// Volume is 8-bit attenuation (0.5 dB/step). Filter presets 0-7.
// Supported rates: 44100, 48000, 96000, 192000 Hz.
//
class HalEs9020DacMock : public HalAudioDevice, public HalAudioDacInterface {
public:
    static const uint8_t kI2cAddr    = MOCK_ES9020_I2C_ADDR;
    static const uint8_t kChipId     = MOCK_ES9020_CHIP_ID;

    uint8_t  _i2cAddr      = kI2cAddr;
    uint8_t  _i2cBusIndex  = 2;
    int8_t   _sdaPin       = 28;
    int8_t   _sclPin       = 29;
    uint32_t _sampleRate   = 48000;
    uint8_t  _bitDepth     = 32;
    uint8_t  _volume       = 100;    // 0-100 percent
    bool     _muted        = false;
    uint8_t  _filterPreset = 0;      // 0-7
    bool     _initialized  = false;
    bool     _i2sTxEnabled = false;

    // APLL internal state
    bool     _apllEnabled  = false;
    bool     _apllLocked   = false;  // Hardware-only; always false in native test

    HalEs9020DacMock() {
        strncpy(_descriptor.compatible, "ess,es9020-dac", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "ES9020", 32);
        _descriptor.name[32] = '\0';
        strncpy(_descriptor.manufacturer, "ESS Technology", 32);
        _descriptor.manufacturer[32] = '\0';
        _descriptor.type            = HAL_DEV_DAC;
        _descriptor.i2cAddr         = kI2cAddr;
        _descriptor.channelCount    = 2;    // Stereo DAC
        _descriptor.bus.type        = HAL_BUS_I2C;
        _descriptor.bus.index       = HAL_I2C_BUS_EXP;
        _descriptor.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
        _descriptor.capabilities    = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME |
                                      HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_APLL;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    // ----- HalDevice lifecycle -----

    bool probe() override { return true; }  // NATIVE_TEST stub

    HalInitResult init() override {
        _i2sTxEnabled = true;  // Simulates _enableI2sTx()
        _initialized  = true;
        _state        = HAL_STATE_AVAILABLE;
        _ready        = true;
        return hal_init_ok();
    }

    void deinit() override {
        _ready        = false;
        _initialized  = false;
        _i2sTxEnabled = false;
        _state        = HAL_STATE_REMOVED;
    }

    void dumpConfig() override {}

    bool healthCheck() override { return _initialized; }

    // ----- HalAudioDevice -----

    bool configure(uint32_t rate, uint8_t bits) override {
        const uint32_t kSupported[] = { 44100, 48000, 96000, 192000 };
        bool valid = false;
        for (uint8_t i = 0; i < 4; i++) {
            if (rate == kSupported[i]) { valid = true; break; }
        }
        if (!valid) return false;
        _sampleRate = rate;
        _bitDepth   = bits;
        return true;
    }

    bool setVolume(uint8_t pct) override {
        if (!_initialized) return false;
        if (pct > 100) pct = 100;
        _volume = pct;
        return true;
    }

    bool setMute(bool mute) override {
        if (!_initialized) return false;
        _muted = mute;
        return true;
    }

    // ----- Filter preset -----

    bool setFilterPreset(uint8_t preset) {
        if (preset >= ESS_SABRE_FILTER_COUNT) return false;
        _filterPreset = preset;
        return true;
    }

    // ----- HalAudioDacInterface -----

    bool     dacSetVolume(uint8_t pct) override  { return setVolume(pct); }
    bool     dacSetMute(bool m) override         { return setMute(m); }
    uint8_t  dacGetVolume() const override       { return _volume; }
    bool     dacIsMuted() const override         { return _muted; }
    bool     dacSetSampleRate(uint32_t hz) override { return configure(hz, _bitDepth); }
    bool     dacSetBitDepth(uint8_t bits) override  { return configure(_sampleRate, bits); }
    uint32_t dacGetSampleRate() const override   { return _sampleRate; }

    // ----- ES9020-specific: APLL clock recovery -----

    bool setApllEnabled(bool enable) {
        if (!_initialized) return false;
        _apllEnabled = enable;
        if (!enable) _apllLocked = false;  // Lock is lost when APLL disabled
        return true;
    }

    bool isApllLocked() const {
        // In real hardware: read REG_APLL_CTRL bit4. In native test: always false.
        return _apllLocked;
    }

    // ----- buildSink (simplified for test: checks field population) -----

    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) {
        if (!out) return false;
        if (sinkSlot >= AUDIO_OUT_MAX_SINKS) return false;
        uint8_t fc = (uint8_t)(sinkSlot * 2);
        if (fc + _descriptor.channelCount > AUDIO_PIPELINE_MATRIX_SIZE) return false;

        *out = AUDIO_OUTPUT_SINK_INIT;
        out->name         = _descriptor.name;
        out->firstChannel = fc;
        out->channelCount = _descriptor.channelCount;
        out->halSlot      = _slot;
        out->ctx          = this;
        // write/isReady are function pointers; set to nullptr in mock (no static table)
        out->write   = nullptr;
        out->isReady = nullptr;
        return true;
    }
};

// ===== Fixtures =====
static HalEs9020DacMock* dac;

void setUp(void) {
    WireMock::reset();
    dac = new HalEs9020DacMock();
}

void tearDown(void) {
    delete dac;
    dac = nullptr;
}

// ==========================================================================
// Section 1: Descriptor tests
// ==========================================================================

// ----- 1. descriptor compatible string is "ess,es9020-dac" -----
void test_descriptor_compatible_string(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9020-dac", dac->getDescriptor().compatible);
}

// ----- 2. descriptor type is HAL_DEV_DAC -----
void test_descriptor_type_is_dac(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, (int)dac->getDescriptor().type);
}

// ----- 3. capabilities include DAC_PATH, HW_VOLUME, MUTE, FILTERS, APLL -----
void test_descriptor_capabilities(void) {
    uint32_t caps = dac->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(caps & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(caps & HAL_CAP_FILTERS);
    TEST_ASSERT_TRUE(caps & HAL_CAP_APLL);
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

// ----- 6. I2C address is 0x48 (ESS DAC base address) -----
void test_descriptor_i2c_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0x48, dac->getDescriptor().i2cAddr);
}

// ----- 7. sample rate mask includes 44K1, 48K, 96K, 192K -----
void test_descriptor_sample_rate_mask(void) {
    uint32_t mask = dac->getDescriptor().sampleRatesMask;
    TEST_ASSERT_TRUE(mask & HAL_RATE_44K1);
    TEST_ASSERT_TRUE(mask & HAL_RATE_48K);
    TEST_ASSERT_TRUE(mask & HAL_RATE_96K);
    TEST_ASSERT_TRUE(mask & HAL_RATE_192K);
    // 8K and 16K are not in the supported set
    TEST_ASSERT_FALSE(mask & HAL_RATE_8K);
    TEST_ASSERT_FALSE(mask & HAL_RATE_16K);
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

// ----- 10. init() enables the I2S TX path -----
void test_init_enables_i2s_tx(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->_i2sTxEnabled);
}

// ----- 11. deinit() clears _ready and sets state=REMOVED -----
void test_deinit_clears_ready(void) {
    dac->init();
    dac->deinit();
    TEST_ASSERT_FALSE(dac->_ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)dac->_state);
}

// ----- 12. deinit() disables I2S TX -----
void test_deinit_disables_i2s_tx(void) {
    dac->init();
    dac->deinit();
    TEST_ASSERT_FALSE(dac->_i2sTxEnabled);
}

// ----- 13. healthCheck() returns true after init -----
void test_health_check_after_init(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->healthCheck());
}

// ----- 14. healthCheck() returns false before init -----
void test_health_check_before_init(void) {
    TEST_ASSERT_FALSE(dac->healthCheck());
}

// ==========================================================================
// Section 3: Volume tests
// ==========================================================================

// ----- 15. setVolume(100) succeeds (0 dB, full output) -----
void test_volume_100_percent(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setVolume(100));
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ----- 16. setVolume(50) succeeds (mid-range) -----
void test_volume_50_percent(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setVolume(50));
    TEST_ASSERT_EQUAL(50, dac->_volume);
}

// ----- 17. setVolume(0) succeeds (full attenuation) -----
void test_volume_0_percent(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setVolume(0));
    TEST_ASSERT_EQUAL(0, dac->_volume);
}

// ----- 18. setVolume() before init returns false -----
void test_volume_before_init_fails(void) {
    TEST_ASSERT_FALSE(dac->setVolume(50));
}

// ----- 19. setVolume() clamps values above 100 to 100 -----
void test_volume_clamp_above_100(void) {
    dac->init();
    dac->_volume = 0;
    TEST_ASSERT_TRUE(dac->setVolume(200));
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ----- 20. volume default is 100 on construction -----
void test_volume_default_is_100(void) {
    TEST_ASSERT_EQUAL(100, dac->_volume);
}

// ==========================================================================
// Section 4: Mute tests
// ==========================================================================

// ----- 21. setMute(true) sets muted flag -----
void test_mute_on(void) {
    dac->init();
    TEST_ASSERT_FALSE(dac->_muted);
    TEST_ASSERT_TRUE(dac->setMute(true));
    TEST_ASSERT_TRUE(dac->_muted);
}

// ----- 22. setMute(false) clears muted flag -----
void test_mute_off(void) {
    dac->init();
    dac->setMute(true);
    TEST_ASSERT_TRUE(dac->setMute(false));
    TEST_ASSERT_FALSE(dac->_muted);
}

// ----- 23. setMute() before init returns false -----
void test_mute_before_init_fails(void) {
    TEST_ASSERT_FALSE(dac->setMute(true));
}

// ----- 24. mute default is false on construction -----
void test_mute_default_is_false(void) {
    TEST_ASSERT_FALSE(dac->_muted);
}

// ----- 25. mute then unmute preserves original volume -----
void test_mute_preserves_volume(void) {
    dac->init();
    dac->setVolume(75);
    TEST_ASSERT_EQUAL(75, dac->_volume);
    dac->setMute(true);
    TEST_ASSERT_EQUAL(75, dac->_volume);  // Volume field unchanged
    dac->setMute(false);
    TEST_ASSERT_EQUAL(75, dac->_volume);  // Volume field still 75
}

// ==========================================================================
// Section 5: Filter preset tests
// ==========================================================================

// ----- 26. setFilterPreset(0-7) all succeed -----
void test_filter_preset_all_valid(void) {
    for (uint8_t i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(dac->setFilterPreset(i));
        TEST_ASSERT_EQUAL(i, dac->_filterPreset);
    }
}

// ----- 27. setFilterPreset(8) returns false -----
void test_filter_preset_8_rejected(void) {
    TEST_ASSERT_FALSE(dac->setFilterPreset(8));
    TEST_ASSERT_EQUAL(0, dac->_filterPreset);  // Unchanged from default
}

// ----- 28. setFilterPreset(255) returns false -----
void test_filter_preset_255_rejected(void) {
    TEST_ASSERT_FALSE(dac->setFilterPreset(255));
}

// ----- 29. filter preset default is 0 -----
void test_filter_preset_default_is_0(void) {
    TEST_ASSERT_EQUAL(0, dac->_filterPreset);
}

// ==========================================================================
// Section 6: Sample rate tests
// ==========================================================================

// ----- 30. configure(44100, 32) succeeds -----
void test_sample_rate_44k1(void) {
    TEST_ASSERT_TRUE(dac->configure(44100, 32));
    TEST_ASSERT_EQUAL(44100u, dac->_sampleRate);
}

// ----- 31. configure(48000, 32) succeeds -----
void test_sample_rate_48k(void) {
    TEST_ASSERT_TRUE(dac->configure(48000, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
}

// ----- 32. configure(96000, 32) succeeds -----
void test_sample_rate_96k(void) {
    TEST_ASSERT_TRUE(dac->configure(96000, 32));
    TEST_ASSERT_EQUAL(96000u, dac->_sampleRate);
}

// ----- 33. configure(192000, 32) succeeds -----
void test_sample_rate_192k(void) {
    TEST_ASSERT_TRUE(dac->configure(192000, 32));
    TEST_ASSERT_EQUAL(192000u, dac->_sampleRate);
}

// ----- 34. configure(8000, 32) returns false -----
void test_sample_rate_8k_rejected(void) {
    TEST_ASSERT_FALSE(dac->configure(8000, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);  // Unchanged
}

// ----- 35. configure(384000, 32) returns false (not in ES9020 spec) -----
void test_sample_rate_384k_rejected(void) {
    TEST_ASSERT_FALSE(dac->configure(384000, 32));
}

// ==========================================================================
// Section 7: Configure tests
// ==========================================================================

// ----- 36. configure updates both sampleRate and bitDepth -----
void test_configure_updates_both_fields(void) {
    TEST_ASSERT_TRUE(dac->configure(96000, 24));
    TEST_ASSERT_EQUAL(96000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);
}

// ----- 37. configure rejects invalid rate, leaves state unchanged -----
void test_configure_invalid_rate_leaves_state(void) {
    dac->configure(48000, 32);
    TEST_ASSERT_FALSE(dac->configure(22050, 32));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);
}

// ----- 38. sequential reconfigure updates correctly -----
void test_sequential_reconfigure(void) {
    TEST_ASSERT_TRUE(dac->configure(44100, 16));
    TEST_ASSERT_EQUAL(44100u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(16, dac->_bitDepth);

    TEST_ASSERT_TRUE(dac->configure(192000, 32));
    TEST_ASSERT_EQUAL(192000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(32, dac->_bitDepth);

    TEST_ASSERT_TRUE(dac->configure(48000, 24));
    TEST_ASSERT_EQUAL(48000u, dac->_sampleRate);
    TEST_ASSERT_EQUAL(24, dac->_bitDepth);
}

// ==========================================================================
// Section 8: DAC interface tests
// ==========================================================================

// ----- 39. dacSetVolume() delegates to setVolume() -----
void test_dac_set_volume(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->dacSetVolume(60));
    TEST_ASSERT_EQUAL(60, dac->dacGetVolume());
}

// ----- 40. dacGetVolume() returns current volume -----
void test_dac_get_volume_after_set(void) {
    dac->init();
    dac->setVolume(80);
    TEST_ASSERT_EQUAL(80, dac->dacGetVolume());
}

// ----- 41. dacSetMute() and dacIsMuted() are consistent -----
void test_dac_set_mute_query(void) {
    dac->init();
    TEST_ASSERT_FALSE(dac->dacIsMuted());
    TEST_ASSERT_TRUE(dac->dacSetMute(true));
    TEST_ASSERT_TRUE(dac->dacIsMuted());
    TEST_ASSERT_TRUE(dac->dacSetMute(false));
    TEST_ASSERT_FALSE(dac->dacIsMuted());
}

// ----- 42. dacSetSampleRate() delegates to configure() -----
void test_dac_set_sample_rate(void) {
    TEST_ASSERT_TRUE(dac->dacSetSampleRate(96000));
    TEST_ASSERT_EQUAL(96000u, dac->dacGetSampleRate());
}

// ----- 43. dacSetSampleRate() with unsupported rate fails -----
void test_dac_set_sample_rate_unsupported(void) {
    TEST_ASSERT_FALSE(dac->dacSetSampleRate(11025));
    TEST_ASSERT_EQUAL(48000u, dac->dacGetSampleRate());
}

// ==========================================================================
// Section 9: buildSink tests
// ==========================================================================

// ----- 44. buildSink(slot=0) populates AudioOutputSink -----
void test_build_sink_slot0(void) {
    dac->init();
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_TRUE(dac->buildSink(0, &sink));
    TEST_ASSERT_EQUAL_STRING("ES9020", sink.name);
    TEST_ASSERT_EQUAL(0, sink.firstChannel);
    TEST_ASSERT_EQUAL(2, sink.channelCount);
    TEST_ASSERT_EQUAL_PTR(dac, sink.ctx);
}

// ----- 45. buildSink(slot=1) assigns firstChannel=2 -----
void test_build_sink_slot1_first_channel(void) {
    dac->init();
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_TRUE(dac->buildSink(1, &sink));
    TEST_ASSERT_EQUAL(2, sink.firstChannel);
    TEST_ASSERT_EQUAL(2, sink.channelCount);
}

// ----- 46. buildSink with null pointer returns false -----
void test_build_sink_null_returns_false(void) {
    dac->init();
    TEST_ASSERT_FALSE(dac->buildSink(0, nullptr));
}

// ----- 47. buildSink with slot >= AUDIO_OUT_MAX_SINKS returns false -----
void test_build_sink_overflow_slot_returns_false(void) {
    dac->init();
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_FALSE(dac->buildSink(AUDIO_OUT_MAX_SINKS, &sink));
}

// ==========================================================================
// Section 10: APLL tests
// ==========================================================================

// ----- 48. setApllEnabled(true) after init succeeds -----
void test_apll_enable(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setApllEnabled(true));
    TEST_ASSERT_TRUE(dac->_apllEnabled);
}

// ----- 49. setApllEnabled(false) disables APLL -----
void test_apll_disable(void) {
    dac->init();
    dac->setApllEnabled(true);
    TEST_ASSERT_TRUE(dac->setApllEnabled(false));
    TEST_ASSERT_FALSE(dac->_apllEnabled);
}

// ----- 50. setApllEnabled() before init returns false -----
void test_apll_before_init_fails(void) {
    TEST_ASSERT_FALSE(dac->setApllEnabled(true));
    TEST_ASSERT_FALSE(dac->_apllEnabled);
}

// ----- 51. disabling APLL clears lock state -----
void test_apll_disable_clears_lock(void) {
    dac->init();
    dac->setApllEnabled(true);
    dac->_apllLocked = true;  // Simulate hardware lock
    dac->setApllEnabled(false);
    TEST_ASSERT_FALSE(dac->_apllLocked);
}

// ----- 52. isApllLocked() returns false in native test (no hardware) -----
void test_apll_lock_false_native(void) {
    dac->init();
    dac->setApllEnabled(true);
    // APLL lock requires hardware; always false in native test
    TEST_ASSERT_FALSE(dac->isApllLocked());
}

// ----- 53. isApllLocked() returns false before init -----
void test_apll_lock_before_init(void) {
    TEST_ASSERT_FALSE(dac->isApllLocked());
}

// ----- 54. APLL default is disabled on construction -----
void test_apll_default_disabled(void) {
    TEST_ASSERT_FALSE(dac->_apllEnabled);
    TEST_ASSERT_FALSE(dac->_apllLocked);
}

// ----- 55. toggle APLL on/off/on is idempotent -----
void test_apll_toggle_idempotent(void) {
    dac->init();
    TEST_ASSERT_TRUE(dac->setApllEnabled(true));
    TEST_ASSERT_TRUE(dac->_apllEnabled);
    TEST_ASSERT_TRUE(dac->setApllEnabled(true));   // Enable again — no error
    TEST_ASSERT_TRUE(dac->_apllEnabled);
    TEST_ASSERT_TRUE(dac->setApllEnabled(false));
    TEST_ASSERT_FALSE(dac->_apllEnabled);
    TEST_ASSERT_TRUE(dac->setApllEnabled(false));  // Disable again — no error
    TEST_ASSERT_FALSE(dac->_apllEnabled);
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
    RUN_TEST(test_init_enables_i2s_tx);
    RUN_TEST(test_deinit_clears_ready);
    RUN_TEST(test_deinit_disables_i2s_tx);
    RUN_TEST(test_health_check_after_init);
    RUN_TEST(test_health_check_before_init);

    // Section 3: Volume
    RUN_TEST(test_volume_100_percent);
    RUN_TEST(test_volume_50_percent);
    RUN_TEST(test_volume_0_percent);
    RUN_TEST(test_volume_before_init_fails);
    RUN_TEST(test_volume_clamp_above_100);
    RUN_TEST(test_volume_default_is_100);

    // Section 4: Mute
    RUN_TEST(test_mute_on);
    RUN_TEST(test_mute_off);
    RUN_TEST(test_mute_before_init_fails);
    RUN_TEST(test_mute_default_is_false);
    RUN_TEST(test_mute_preserves_volume);

    // Section 5: Filter preset
    RUN_TEST(test_filter_preset_all_valid);
    RUN_TEST(test_filter_preset_8_rejected);
    RUN_TEST(test_filter_preset_255_rejected);
    RUN_TEST(test_filter_preset_default_is_0);

    // Section 6: Sample rate
    RUN_TEST(test_sample_rate_44k1);
    RUN_TEST(test_sample_rate_48k);
    RUN_TEST(test_sample_rate_96k);
    RUN_TEST(test_sample_rate_192k);
    RUN_TEST(test_sample_rate_8k_rejected);
    RUN_TEST(test_sample_rate_384k_rejected);

    // Section 7: Configure
    RUN_TEST(test_configure_updates_both_fields);
    RUN_TEST(test_configure_invalid_rate_leaves_state);
    RUN_TEST(test_sequential_reconfigure);

    // Section 8: DAC interface
    RUN_TEST(test_dac_set_volume);
    RUN_TEST(test_dac_get_volume_after_set);
    RUN_TEST(test_dac_set_mute_query);
    RUN_TEST(test_dac_set_sample_rate);
    RUN_TEST(test_dac_set_sample_rate_unsupported);

    // Section 9: buildSink
    RUN_TEST(test_build_sink_slot0);
    RUN_TEST(test_build_sink_slot1_first_channel);
    RUN_TEST(test_build_sink_null_returns_false);
    RUN_TEST(test_build_sink_overflow_slot_returns_false);

    // Section 10: APLL
    RUN_TEST(test_apll_enable);
    RUN_TEST(test_apll_disable);
    RUN_TEST(test_apll_before_init_fails);
    RUN_TEST(test_apll_disable_clears_lock);
    RUN_TEST(test_apll_lock_false_native);
    RUN_TEST(test_apll_lock_before_init);
    RUN_TEST(test_apll_default_disabled);
    RUN_TEST(test_apll_toggle_idempotent);

    return UNITY_END();
}
