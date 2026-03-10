// test_hal_es8311.cpp
// Tests a mock HalEs8311 class that exercises the same HAL interface patterns
// as the real ES8311 codec driver:
//   - I2C probe (ACK/NACK via Wire mock)
//   - Chip-ID verification during init
//   - Register writes for reset, volume, and mute
//   - State transitions (HAL_STATE_AVAILABLE / HAL_STATE_REMOVED)
//   - Descriptor / capability inspection
//
// The real hal_es8311.cpp is tested via this mock until the platform file
// compiles cleanly in the native environment.

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Wire.h"
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_audio_device.h"

// ===== Inline capability flags not yet in hal_types.h =====
#ifndef HAL_CAP_CODEC
#define HAL_CAP_CODEC        (1 << 7)
#endif
#ifndef HAL_CAP_PGA_CONTROL
#define HAL_CAP_PGA_CONTROL  (1 << 5)
#endif
#ifndef HAL_CAP_HPF_CONTROL
#define HAL_CAP_HPF_CONTROL  (1 << 6)
#endif

// ===== buildSink() test helpers =====
// Static device pointer for mock isReady callback (mirrors real static-table pattern)
static HalAudioDevice* _mockEs8311SinkDev = nullptr;

static void _mock_es8311_write_stub(const int32_t* buf, int stereoFrames) {
    (void)buf; (void)stereoFrames;
}

static bool _mock_es8311_ready_cb(void) {
    return _mockEs8311SinkDev && _mockEs8311SinkDev->_ready;
}

// ===== Mock ES8311 — implements HalAudioDevice over the Wire mock =====
//
// Register map used (subset of real ES8311):
//   0x00 — reset register  (write 0x1C to reset)
//   0x31 — DAC mute        (bit 3 = mute)
//   0x32 — DAC volume      (0x00 = min, 0xBF = max)
//
// Chip-ID check at init: reg[0]=0x80, reg[1]=0x45
class HalEs8311Mock : public HalAudioDevice {
public:
    static const uint8_t kI2cAddr = 0x18;

    HalEs8311Mock() {
        strncpy(_descriptor.compatible, "everest-semi,es8311", 31);
        _descriptor.compatible[31] = '\0';
        strncpy(_descriptor.name, "ES8311 Codec", 32);
        _descriptor.name[32] = '\0';
        _descriptor.type        = HAL_DEV_CODEC;
        _descriptor.i2cAddr     = kI2cAddr;
        _descriptor.channelCount = 1;
        _descriptor.capabilities =
            HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_CODEC |
            HAL_CAP_ADC_PATH  | HAL_CAP_DAC_PATH;
    }

    bool probe() override {
        Wire.beginTransmission(kI2cAddr);
        return Wire.endTransmission() == 0;
    }

    HalInitResult init() override {
        if (!probe()) return hal_init_fail(DIAG_HAL_INIT_FAILED, "probe failed");

        // Verify chip ID
        Wire.requestFrom(kI2cAddr, (uint8_t)2);
        uint8_t id0 = (uint8_t)Wire.read();
        uint8_t id1 = (uint8_t)Wire.read();
        if (id0 != 0x80 || id1 != 0x45) return hal_init_fail(DIAG_HAL_INIT_FAILED, "bad chip ID");

        // Reset codec
        Wire.beginTransmission(kI2cAddr);
        Wire.write((uint8_t)0x00);
        Wire.write((uint8_t)0x1C);
        Wire.endTransmission();

        _state = HAL_STATE_AVAILABLE;
        _ready = true;
        return hal_init_ok();
    }

    void deinit() override {
        _ready = false;
        _state = HAL_STATE_REMOVED;
    }

    void dumpConfig() override {}

    bool healthCheck() override { return probe(); }

    bool configure(uint32_t rate, uint8_t bits) override {
        // Supported rates: 44100, 48000, 96000
        if (rate != 44100 && rate != 48000 && rate != 96000) return false;
        // Supported depths: 16, 24, 32
        if (bits != 16 && bits != 24 && bits != 32) return false;
        _sampleRate = rate;
        _bitDepth   = bits;
        return true;
    }

    bool setVolume(uint8_t pct) override {
        _volume = pct;
        uint8_t reg = (uint8_t)(((uint16_t)pct * 0xBF) / 100);
        Wire.beginTransmission(kI2cAddr);
        Wire.write((uint8_t)0x32);
        Wire.write(reg);
        Wire.endTransmission();
        return true;
    }

    bool setMute(bool mute) override {
        _muted = mute;
        Wire.beginTransmission(kI2cAddr);
        Wire.write((uint8_t)0x31);
        Wire.write(mute ? (uint8_t)0x08 : (uint8_t)0x00);
        Wire.endTransmission();
        return true;
    }

    // buildSink() — populates AudioOutputSink with callbacks
    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override {
        if (!out) return false;
        if (sinkSlot >= AUDIO_OUT_MAX_SINKS) return false;

        *out = AUDIO_OUTPUT_SINK_INIT;
        out->name         = _descriptor.name;
        out->firstChannel = (uint8_t)(sinkSlot * 2);
        out->channelCount = _descriptor.channelCount;
        out->halSlot      = _slot;
        out->write        = _mock_es8311_write_stub;
        out->isReady      = _mock_es8311_ready_cb;
        out->ctx          = this;

        _mockEs8311SinkDev = this;
        return true;
    }

    uint8_t  _volume     = 0;
    bool     _muted      = false;
    uint32_t _sampleRate = 48000;
    uint8_t  _bitDepth   = 16;
};

// ===== Fixtures =====

static HalEs8311Mock* codec;

void setUp(void) {
    WireMock::reset();
    // Pre-register ES8311 at 0x18 on ONBOARD bus (index 1) with chip-ID bytes
    uint8_t chipId[] = {0x80, 0x45};
    WireMock::registerDevice(0x18, 1, chipId, 2);
    codec = new HalEs8311Mock();
}

void tearDown(void) {
    delete codec;
    codec = nullptr;
}

// ===== Tests =====

// ----- 1. probe() ACKs when device is registered -----
void test_probe_success_when_device_present(void) {
    TEST_ASSERT_TRUE(codec->probe());
}

// ----- 2. probe() NACKs when device is absent -----
void test_probe_fails_when_nack(void) {
    WireMock::reset();   // Removes all registered devices
    TEST_ASSERT_FALSE(codec->probe());
}

// ----- 3. init() succeeds with correct chip ID → state = AVAILABLE -----
void test_init_success_with_valid_chip_id(void) {
    TEST_ASSERT_TRUE(codec->init().success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)codec->_state);
    TEST_ASSERT_TRUE(codec->_ready);
}

// ----- 4. init() fails when chip ID bytes are wrong -----
void test_init_fails_with_wrong_chip_id(void) {
    WireMock::reset();
    uint8_t bad[] = {0xFF, 0xFF};
    WireMock::registerDevice(0x18, 1, bad, 2);
    TEST_ASSERT_FALSE(codec->init().success);
}

// ----- 5. init() writes 0x1C to reset register 0x00 -----
void test_init_writes_reset_register(void) {
    codec->init();
    TEST_ASSERT_EQUAL(0x1C, WireMock::registerMap[0x18][0x00]);
}

// ----- 6. healthCheck() passes when device still ACKs -----
void test_health_check_passes_when_present(void) {
    codec->init();
    TEST_ASSERT_TRUE(codec->healthCheck());
}

// ----- 7. healthCheck() fails after device is removed -----
void test_health_check_fails_when_unplugged(void) {
    codec->init();
    WireMock::reset();   // Simulate device removal (NACK)
    TEST_ASSERT_FALSE(codec->healthCheck());
}

// ----- 8. setVolume(0) writes 0x00 to reg 0x32 -----
void test_set_volume_0_writes_0x00(void) {
    codec->init();
    codec->setVolume(0);
    TEST_ASSERT_EQUAL(0x00, WireMock::registerMap[0x18][0x32]);
}

// ----- 9. setVolume(100) writes 0xBF to reg 0x32 -----
void test_set_volume_100_writes_0xBF(void) {
    codec->init();
    codec->setVolume(100);
    TEST_ASSERT_EQUAL(0xBF, WireMock::registerMap[0x18][0x32]);
}

// ----- 10. setVolume(50) writes a value near the midpoint of the scale -----
void test_set_volume_50_writes_midpoint(void) {
    codec->init();
    codec->setVolume(50);
    uint8_t v = WireMock::registerMap[0x18][0x32];
    // (50 * 0xBF) / 100 = 95 = 0x5F; allow ±1 for integer rounding
    TEST_ASSERT_TRUE(v >= 0x5E && v <= 0x60);
}

// ----- 11. setMute(true) sets bit 3 in reg 0x31 -----
void test_set_mute_true_sets_mute_bit(void) {
    codec->init();
    codec->setMute(true);
    TEST_ASSERT_EQUAL(0x08, WireMock::registerMap[0x18][0x31]);
}

// ----- 12. setMute(false) after setMute(true) clears the mute bit -----
void test_set_mute_false_clears_mute_bit(void) {
    codec->init();
    codec->setMute(true);
    codec->setMute(false);
    TEST_ASSERT_EQUAL(0x00, WireMock::registerMap[0x18][0x31]);
}

// ----- 13. configure(48000, 16) is accepted -----
void test_configure_48khz_16bit_ok(void) {
    TEST_ASSERT_TRUE(codec->configure(48000, 16));
    TEST_ASSERT_EQUAL(48000u, codec->_sampleRate);
    TEST_ASSERT_EQUAL(16, codec->_bitDepth);
}

// ----- 14. configure(96000, 24) is accepted -----
void test_configure_96khz_24bit_ok(void) {
    TEST_ASSERT_TRUE(codec->configure(96000, 24));
}

// ----- 15. configure(192000, 16) is rejected (unsupported rate) -----
void test_configure_192khz_rejected(void) {
    TEST_ASSERT_FALSE(codec->configure(192000, 16));
}

// ----- 16. descriptor type is HAL_DEV_CODEC -----
void test_descriptor_type_is_codec(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_CODEC, (int)codec->getDescriptor().type);
}

// ----- 17. capabilities include HAL_CAP_HW_VOLUME and HAL_CAP_MUTE -----
void test_capabilities_include_volume_mute(void) {
    uint8_t caps = codec->getDescriptor().capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(caps & HAL_CAP_MUTE);
}

// ----- 18. deinit() sets state to REMOVED and _ready to false -----
void test_deinit_removes_device(void) {
    codec->init();
    codec->deinit();
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)codec->_state);
    TEST_ASSERT_FALSE(codec->_ready);
}

// ----- 19. buildSink() populates struct with valid callbacks -----
void test_es8311_buildSink_populates_struct(void) {
    codec->_state = HAL_STATE_AVAILABLE;
    codec->_ready = true;

    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    bool ok = codec->buildSink(0, &sink);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_NOT_NULL(sink.write);
    TEST_ASSERT_NOT_NULL(sink.isReady);
    TEST_ASSERT_EQUAL(codec->getSlot(), sink.halSlot);
    TEST_ASSERT_EQUAL_STRING("ES8311 Codec", sink.name);
    TEST_ASSERT_EQUAL(0, sink.firstChannel);
    TEST_ASSERT_EQUAL(1, sink.channelCount);
    TEST_ASSERT_EQUAL_PTR(codec, sink.ctx);
}

// ----- 20. buildSink() with null output returns false -----
void test_es8311_buildSink_null_returns_false(void) {
    TEST_ASSERT_FALSE(codec->buildSink(0, nullptr));
}

// ----- 21. buildSink() isReady callback reflects device _ready flag -----
void test_es8311_buildSink_ready_callback(void) {
    codec->_ready = true;

    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    codec->buildSink(0, &sink);

    // isReady should return true when device is ready
    TEST_ASSERT_TRUE(sink.isReady());

    codec->_ready = false;
    TEST_ASSERT_FALSE(sink.isReady());
}

// ----- 22. buildSink() with out-of-range slot returns false -----
void test_es8311_buildSink_invalid_slot_returns_false(void) {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_FALSE(codec->buildSink(AUDIO_OUT_MAX_SINKS, &sink));
    TEST_ASSERT_FALSE(codec->buildSink(255, &sink));
}

// ===== Main =====
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_probe_success_when_device_present);
    RUN_TEST(test_probe_fails_when_nack);
    RUN_TEST(test_init_success_with_valid_chip_id);
    RUN_TEST(test_init_fails_with_wrong_chip_id);
    RUN_TEST(test_init_writes_reset_register);
    RUN_TEST(test_health_check_passes_when_present);
    RUN_TEST(test_health_check_fails_when_unplugged);
    RUN_TEST(test_set_volume_0_writes_0x00);
    RUN_TEST(test_set_volume_100_writes_0xBF);
    RUN_TEST(test_set_volume_50_writes_midpoint);
    RUN_TEST(test_set_mute_true_sets_mute_bit);
    RUN_TEST(test_set_mute_false_clears_mute_bit);
    RUN_TEST(test_configure_48khz_16bit_ok);
    RUN_TEST(test_configure_96khz_24bit_ok);
    RUN_TEST(test_configure_192khz_rejected);
    RUN_TEST(test_descriptor_type_is_codec);
    RUN_TEST(test_capabilities_include_volume_mute);
    RUN_TEST(test_deinit_removes_device);
    RUN_TEST(test_es8311_buildSink_populates_struct);
    RUN_TEST(test_es8311_buildSink_null_returns_false);
    RUN_TEST(test_es8311_buildSink_ready_callback);
    RUN_TEST(test_es8311_buildSink_invalid_slot_returns_false);

    return UNITY_END();
}
