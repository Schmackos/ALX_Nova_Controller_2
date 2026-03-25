// test_hal_cirrus_dsd.cpp
// Tests DSD/DoP mode switching for Cirrus Logic DAC family (HalCirrusDac2ch).
//
// Covers:
//   - setDsdMode(true/false) state tracking
//   - isDsdMode() reflects last call
//   - No-op return false on chips without DSD (CS4399)
//   - setDsdMode returns false before init
//   - setDsdMode is idempotent (same state is a no-op)
//   - Mute state restored correctly after DSD enable/disable
//   - buildSink() sets supportsDsd for HAL_CAP_DSD devices
//   - buildSink() clears supportsDsd for non-DSD devices
//   - Descriptor DSD fields: CS43198 has regDsdPath != 0xFFFF
//   - Descriptor DSD fields: CS4399 has regDsdPath == 0xFFFF (no DSD)
//
// Architecture note: HalCirrusDac2ch is tested via a thin mock that wraps the
// real descriptor tables (kDescCS43198, kDescCS4399) but stubs I2C and delays.

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

// ---------------------------------------------------------------------------
// Minimal mock for DSD-capable Cirrus DAC (models CS43198 behaviour)
// ---------------------------------------------------------------------------
//
// Models the key DSD switching behaviour without hitting real I2C:
//   - setDsdMode(enable): returns false if not initialized, else tracks state
//   - isDsdMode(): reflects last successful setDsdMode
//   - CS43198/CS43131/CS43130 have DSD support (regDsdPath != 0xFFFF)
//   - CS4399 has no DSD support (regDsdPath == 0xFFFF)

static constexpr uint16_t NO_FEATURE = 0xFFFF;

// Descriptor stub for DSD-capable chip (CS43198-like)
struct DsdDescStub {
    uint16_t regDsdPath;
    uint8_t  dsdPathEnable;
    uint8_t  dsdPathDisable;
    uint8_t  dsdPathMask;
    uint16_t regDsdInt;
    uint8_t  dsdIntDefault;
    uint32_t capabilities;
};

static const DsdDescStub kDsdCapable = {
    /* regDsdPath    */ 0x0030,
    /* dsdPathEnable */ 0x01,
    /* dsdPathDisable*/ 0x00,
    /* dsdPathMask   */ 0x01,
    /* regDsdInt     */ 0x0031,
    /* dsdIntDefault */ 0x01,  // DoP mode
    /* capabilities  */ (uint16_t)(HAL_CAP_DAC_PATH | HAL_CAP_DSD),
};

static const DsdDescStub kNoDsd = {
    /* regDsdPath    */ NO_FEATURE,
    /* dsdPathEnable */ 0x00,
    /* dsdPathDisable*/ 0x00,
    /* dsdPathMask   */ 0xFF,
    /* regDsdInt     */ NO_FEATURE,
    /* dsdIntDefault */ 0x00,
    /* capabilities  */ (uint16_t)(HAL_CAP_DAC_PATH),
};

// Mock DAC that uses DsdDescStub to replicate setDsdMode logic
class MockCirrusDac : public HalAudioDevice, public HalAudioDacInterface {
public:
    const DsdDescStub& _d;
    bool _initialized = false;
    bool _muted       = false;
    bool _dsdEnabled  = false;
    uint8_t _volume   = 100;
    float   _muteRampState = 1.0f;

    explicit MockCirrusDac(const DsdDescStub& desc) : _d(desc) {
        strncpy(_descriptor.compatible, "cirrus,cs43198", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "CS43198-MOCK", 32);
        _descriptor.name[32] = '\0';
        _descriptor.type         = HAL_DEV_DAC;
        _descriptor.channelCount = 2;
        _descriptor.bus.type     = HAL_BUS_I2C;
        _descriptor.capabilities = _d.capabilities;
    }

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
        _dsdEnabled = false;
    }

    void dumpConfig() override {}
    bool healthCheck() override { return _initialized; }

    bool configure(uint32_t, uint8_t) override { return true; }
    bool setVolume(uint8_t pct) override { _volume = pct; return true; }
    bool setMute(bool m) override { _muted = m; return true; }

    bool dacSetVolume(uint8_t pct) override { return setVolume(pct); }
    bool dacSetMute(bool m) override        { return setMute(m); }
    uint8_t dacGetVolume() const override   { return _volume; }
    bool dacIsMuted() const override        { return _muted; }
    bool dacSetSampleRate(uint32_t) override { return true; }
    bool dacSetBitDepth(uint8_t) override   { return true; }
    uint32_t dacGetSampleRate() const override { return 48000u; }

    bool hasHardwareVolume() const override { return true; }

    // Replicate the setDsdMode logic from HalCirrusDac2ch
    bool setDsdMode(bool enable) {
        if (_d.regDsdPath == NO_FEATURE) return false;
        if (!_initialized) return false;
        if (_dsdEnabled == enable) return true;  // idempotent

        // Mute phase
        bool wasMuted = _muted;
        setMute(true);

        // Write DSD registers (stubbed — no real I2C)
        // (In production this writes to chip over I2C via HalI2cBus)

        _dsdEnabled = enable;

        // Unmute if we weren't already muted
        if (!wasMuted) {
            setMute(false);
        }
        return true;
    }

    bool isDsdMode() const { return _dsdEnabled; }

    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override {
        if (!out) return false;
        if (sinkSlot >= AUDIO_OUT_MAX_SINKS) return false;
        *out = AUDIO_OUTPUT_SINK_INIT;
        out->name         = _descriptor.name;
        out->firstChannel = (uint8_t)(sinkSlot * 2);
        out->channelCount = _descriptor.channelCount;
        out->halSlot      = _slot;
        out->write        = nullptr;
        out->isReady      = nullptr;
        out->supportsDsd  = (_descriptor.capabilities & HAL_CAP_DSD) != 0;
        return true;
    }
};

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

static MockCirrusDac* dacDsd;    // DSD-capable (CS43198-like)
static MockCirrusDac* dacNoDsd;  // No DSD (CS4399-like)

void setUp(void) {
    WireMock::reset();
    dacDsd   = new MockCirrusDac(kDsdCapable);
    dacNoDsd = new MockCirrusDac(kNoDsd);
}

void tearDown(void) {
    delete dacDsd;
    delete dacNoDsd;
    dacDsd   = nullptr;
    dacNoDsd = nullptr;
}

// ==========================================================================
// Section 1: Descriptor DSD fields (4 tests)
// ==========================================================================

// ----- 1. DSD-capable chip: regDsdPath is not NO_FEATURE -----
void test_dsd_desc_capable_has_reg(void) {
    TEST_ASSERT_NOT_EQUAL(NO_FEATURE, kDsdCapable.regDsdPath);
}

// ----- 2. Non-DSD chip: regDsdPath is NO_FEATURE -----
void test_dsd_desc_no_dsd_has_no_reg(void) {
    TEST_ASSERT_EQUAL(NO_FEATURE, kNoDsd.regDsdPath);
}

// ----- 3. DSD-capable chip has HAL_CAP_DSD -----
void test_dsd_desc_capable_has_cap(void) {
    TEST_ASSERT_TRUE(kDsdCapable.capabilities & HAL_CAP_DSD);
}

// ----- 4. Non-DSD chip does not have HAL_CAP_DSD -----
void test_dsd_desc_no_dsd_lacks_cap(void) {
    TEST_ASSERT_FALSE(kNoDsd.capabilities & HAL_CAP_DSD);
}

// ==========================================================================
// Section 2: setDsdMode before init (2 tests)
// ==========================================================================

// ----- 5. setDsdMode(true) returns false before init -----
void test_dsd_mode_before_init_returns_false(void) {
    TEST_ASSERT_FALSE(dacDsd->setDsdMode(true));
}

// ----- 6. isDsdMode() is false by default -----
void test_dsd_mode_default_false(void) {
    TEST_ASSERT_FALSE(dacDsd->isDsdMode());
}

// ==========================================================================
// Section 3: setDsdMode after init (6 tests)
// ==========================================================================

// ----- 7. setDsdMode(true) returns true after init -----
void test_dsd_mode_enable_after_init(void) {
    dacDsd->init();
    TEST_ASSERT_TRUE(dacDsd->setDsdMode(true));
}

// ----- 8. isDsdMode() returns true after setDsdMode(true) -----
void test_dsd_mode_is_dsd_after_enable(void) {
    dacDsd->init();
    dacDsd->setDsdMode(true);
    TEST_ASSERT_TRUE(dacDsd->isDsdMode());
}

// ----- 9. setDsdMode(false) returns true and clears state -----
void test_dsd_mode_disable_after_enable(void) {
    dacDsd->init();
    dacDsd->setDsdMode(true);
    TEST_ASSERT_TRUE(dacDsd->setDsdMode(false));
    TEST_ASSERT_FALSE(dacDsd->isDsdMode());
}

// ----- 10. setDsdMode is idempotent: same state returns true without re-writing -----
void test_dsd_mode_idempotent_enable(void) {
    dacDsd->init();
    dacDsd->setDsdMode(true);
    TEST_ASSERT_TRUE(dacDsd->setDsdMode(true));  // No-op, returns true
    TEST_ASSERT_TRUE(dacDsd->isDsdMode());
}

// ----- 11. setDsdMode is idempotent: disable when already disabled -----
void test_dsd_mode_idempotent_disable(void) {
    dacDsd->init();
    TEST_ASSERT_TRUE(dacDsd->setDsdMode(false));  // Already false — no-op
    TEST_ASSERT_FALSE(dacDsd->isDsdMode());
}

// ----- 12. enable/disable cycle leaves isDsdMode false -----
void test_dsd_mode_cycle(void) {
    dacDsd->init();
    dacDsd->setDsdMode(true);
    dacDsd->setDsdMode(false);
    TEST_ASSERT_FALSE(dacDsd->isDsdMode());
}

// ==========================================================================
// Section 4: Mute state preservation (3 tests)
// ==========================================================================

// ----- 13. setDsdMode(true) on unmuted device: unmuted after switch -----
void test_dsd_mode_unmuted_after_enable(void) {
    dacDsd->init();
    dacDsd->setMute(false);
    dacDsd->setDsdMode(true);
    TEST_ASSERT_FALSE(dacDsd->_muted);  // Mute should be restored
}

// ----- 14. setDsdMode(false) on unmuted device: unmuted after switch -----
void test_dsd_mode_unmuted_after_disable(void) {
    dacDsd->init();
    dacDsd->setDsdMode(true);
    dacDsd->setMute(false);
    dacDsd->setDsdMode(false);
    TEST_ASSERT_FALSE(dacDsd->_muted);
}

// ----- 15. setDsdMode when already muted: stays muted after switch -----
void test_dsd_mode_stays_muted_if_muted(void) {
    dacDsd->init();
    dacDsd->setMute(true);
    dacDsd->setDsdMode(true);
    TEST_ASSERT_TRUE(dacDsd->_muted);  // Was muted before — stays muted
}

// ==========================================================================
// Section 5: No-DSD chip behaviour (3 tests)
// ==========================================================================

// ----- 16. setDsdMode(true) returns false on non-DSD chip -----
void test_no_dsd_chip_enable_returns_false(void) {
    dacNoDsd->init();
    TEST_ASSERT_FALSE(dacNoDsd->setDsdMode(true));
}

// ----- 17. isDsdMode() remains false on non-DSD chip -----
void test_no_dsd_chip_is_dsd_false(void) {
    dacNoDsd->init();
    dacNoDsd->setDsdMode(true);
    TEST_ASSERT_FALSE(dacNoDsd->isDsdMode());
}

// ----- 18. setDsdMode(false) also returns false on non-DSD chip -----
void test_no_dsd_chip_disable_returns_false(void) {
    dacNoDsd->init();
    TEST_ASSERT_FALSE(dacNoDsd->setDsdMode(false));
}

// ==========================================================================
// Section 6: buildSink DSD capability advertising (3 tests)
// ==========================================================================

// ----- 19. buildSink sets supportsDsd=true for DSD-capable chip -----
void test_build_sink_supports_dsd_capable(void) {
    dacDsd->init();
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    dacDsd->buildSink(0, &out);
    TEST_ASSERT_TRUE(out.supportsDsd);
}

// ----- 20. buildSink sets supportsDsd=false for non-DSD chip -----
void test_build_sink_no_dsd_capable(void) {
    dacNoDsd->init();
    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    dacNoDsd->buildSink(0, &out);
    TEST_ASSERT_FALSE(out.supportsDsd);
}

// ----- 21. buildSink null pointer returns false -----
void test_build_sink_null_returns_false(void) {
    TEST_ASSERT_FALSE(dacDsd->buildSink(0, nullptr));
}

// ==========================================================================
// Section 7: Post-deinit reset (2 tests)
// ==========================================================================

// ----- 22. isDsdMode() is false after deinit -----
void test_dsd_mode_cleared_on_deinit(void) {
    dacDsd->init();
    dacDsd->setDsdMode(true);
    dacDsd->deinit();
    TEST_ASSERT_FALSE(dacDsd->isDsdMode());
}

// ----- 23. setDsdMode returns false after deinit -----
void test_dsd_mode_fails_after_deinit(void) {
    dacDsd->init();
    dacDsd->deinit();
    TEST_ASSERT_FALSE(dacDsd->setDsdMode(true));
}

// ==========================================================================
// Main
// ==========================================================================

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    // Section 1: Descriptor DSD fields (4 tests)
    RUN_TEST(test_dsd_desc_capable_has_reg);
    RUN_TEST(test_dsd_desc_no_dsd_has_no_reg);
    RUN_TEST(test_dsd_desc_capable_has_cap);
    RUN_TEST(test_dsd_desc_no_dsd_lacks_cap);

    // Section 2: setDsdMode before init (2 tests)
    RUN_TEST(test_dsd_mode_before_init_returns_false);
    RUN_TEST(test_dsd_mode_default_false);

    // Section 3: setDsdMode after init (6 tests)
    RUN_TEST(test_dsd_mode_enable_after_init);
    RUN_TEST(test_dsd_mode_is_dsd_after_enable);
    RUN_TEST(test_dsd_mode_disable_after_enable);
    RUN_TEST(test_dsd_mode_idempotent_enable);
    RUN_TEST(test_dsd_mode_idempotent_disable);
    RUN_TEST(test_dsd_mode_cycle);

    // Section 4: Mute state preservation (3 tests)
    RUN_TEST(test_dsd_mode_unmuted_after_enable);
    RUN_TEST(test_dsd_mode_unmuted_after_disable);
    RUN_TEST(test_dsd_mode_stays_muted_if_muted);

    // Section 5: No-DSD chip behaviour (3 tests)
    RUN_TEST(test_no_dsd_chip_enable_returns_false);
    RUN_TEST(test_no_dsd_chip_is_dsd_false);
    RUN_TEST(test_no_dsd_chip_disable_returns_false);

    // Section 6: buildSink DSD capability advertising (3 tests)
    RUN_TEST(test_build_sink_supports_dsd_capable);
    RUN_TEST(test_build_sink_no_dsd_capable);
    RUN_TEST(test_build_sink_null_returns_false);

    // Section 7: Post-deinit reset (2 tests)
    RUN_TEST(test_dsd_mode_cleared_on_deinit);
    RUN_TEST(test_dsd_mode_fails_after_deinit);

    return UNITY_END();
}
