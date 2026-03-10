// test_hal_mcp4725.cpp
// Tests a mock HalMcp4725 class modelling the Microchip MCP4725 12-bit I2C DAC.
//
// Key behaviours:
//   - probe()          returns true when I2C ACK received (simulated via mock)
//   - init()           sets code to 0, state = AVAILABLE, _ready = true
//   - deinit()         sets code to 0, state = REMOVED, _ready = false
//   - healthCheck()    returns _ready
//   - setVolume(0)     maps to code 0
//   - setVolume(100)   maps to code 4095
//   - setVolume(50)    maps to code 2047
//   - setVoltageCode() clamps to 4095 max
//   - type             = HAL_DEV_DAC
//   - channelCount     = 1
//   - capabilities     = HAL_CAP_HW_VOLUME

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Wire.h"
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_audio_device.h"

// ===== buildSink() test helpers =====
// Static device pointer for mock isReady callback (mirrors real static-table pattern)
static HalAudioDevice* _mockMcp4725SinkDev = nullptr;

static void _mock_mcp4725_write_stub(const int32_t* buf, int stereoFrames) {
    (void)buf; (void)stereoFrames;
}

static bool _mock_mcp4725_ready_cb(void) {
    return _mockMcp4725SinkDev && _mockMcp4725SinkDev->_ready;
}

// ===== Mock MCP4725 device (no hardware I/O) =====
class HalMcp4725Mock : public HalAudioDevice {
public:
    uint8_t  _i2cAddr;
    uint8_t  _busIndex;
    uint16_t _code;
    bool     _muted;

    explicit HalMcp4725Mock(uint8_t addr = 0x60, uint8_t bus = 2)
        : _i2cAddr(addr), _busIndex(bus), _code(0), _muted(false)
    {
        memset(&_descriptor, 0, sizeof(_descriptor));
        strncpy(_descriptor.compatible, "microchip,mcp4725", 31);
        strncpy(_descriptor.name, "MCP4725", 32);
        strncpy(_descriptor.manufacturer, "Microchip Technology", 32);
        _descriptor.type         = HAL_DEV_DAC;
        _descriptor.bus.type     = HAL_BUS_I2C;
        _descriptor.bus.index    = bus;
        _descriptor.i2cAddr      = addr;
        _descriptor.channelCount = 1;
        _descriptor.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME;
        _initPriority = HAL_PRIORITY_HARDWARE;
        _discovery    = HAL_DISC_MANUAL;
    }

    bool probe() override {
        _state = HAL_STATE_DETECTED;
        return true;
    }

    HalInitResult init() override {
        setVoltageCode(0);
        _ready = true;
        _state = HAL_STATE_AVAILABLE;
        return hal_init_ok();
    }

    void deinit() override {
        _code  = 0;
        _ready = false;
        _state = HAL_STATE_REMOVED;
    }

    void dumpConfig() override {}

    bool healthCheck() override {
        return _ready;
    }

    bool configure(uint32_t sampleRate, uint8_t bitDepth) override {
        // Voltage-output DAC — sample rate / bit depth not applicable
        (void)sampleRate; (void)bitDepth;
        return true;
    }

    bool setVolume(uint8_t percent) override {
        if (percent > 100) percent = 100;
        uint16_t code = (uint16_t)((percent * 4095UL) / 100UL);
        return setVoltageCode(code);
    }

    bool setMute(bool mute) override {
        _muted = mute;
        if (mute) {
            return setVoltageCode(0);
        }
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
        out->write        = _mock_mcp4725_write_stub;
        out->isReady      = _mock_mcp4725_ready_cb;
        out->ctx          = this;

        _mockMcp4725SinkDev = this;
        return true;
    }

    bool setVoltageCode(uint16_t code) {
        if (code > 4095) code = 4095;
        _code = code;
        return true;
    }
};

// ===== Fixtures =====
static HalMcp4725Mock* dev;

void setUp(void) {
    WireMock::reset();
    dev = new HalMcp4725Mock();
}

void tearDown(void) {
    delete dev;
    dev = nullptr;
}

// ===== Tests =====

// ----- Lifecycle -----

void test_probe_succeeds(void) {
    TEST_ASSERT_TRUE(dev->probe());
    TEST_ASSERT_EQUAL(HAL_STATE_DETECTED, (int)dev->_state);
}

void test_init_marks_available(void) {
    TEST_ASSERT_TRUE(dev->init().success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)dev->_state);
    TEST_ASSERT_TRUE(dev->_ready);
}

void test_init_zeroes_output(void) {
    dev->setVoltageCode(2000);  // Pre-set to non-zero
    dev->init();
    TEST_ASSERT_EQUAL_UINT16(0, dev->_code);
}

void test_deinit_marks_removed(void) {
    dev->init();
    dev->deinit();
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)dev->_state);
    TEST_ASSERT_FALSE(dev->_ready);
}

void test_deinit_zeroes_output(void) {
    dev->init();
    dev->setVoltageCode(3000);
    dev->deinit();
    TEST_ASSERT_EQUAL_UINT16(0, dev->_code);
}

void test_health_check_returns_ready(void) {
    dev->init();
    TEST_ASSERT_TRUE(dev->healthCheck());
}

void test_health_check_false_when_not_ready(void) {
    // Not init'd — _ready is false
    TEST_ASSERT_FALSE(dev->healthCheck());
}

// ----- setVoltageCode -----

void test_set_voltage_code_zero(void) {
    dev->init();
    TEST_ASSERT_TRUE(dev->setVoltageCode(0));
    TEST_ASSERT_EQUAL_UINT16(0, dev->_code);
}

void test_set_voltage_code_max(void) {
    dev->init();
    TEST_ASSERT_TRUE(dev->setVoltageCode(4095));
    TEST_ASSERT_EQUAL_UINT16(4095, dev->_code);
}

void test_set_voltage_code_mid(void) {
    dev->init();
    TEST_ASSERT_TRUE(dev->setVoltageCode(2048));
    TEST_ASSERT_EQUAL_UINT16(2048, dev->_code);
}

void test_set_voltage_code_clamps_overflow(void) {
    dev->init();
    TEST_ASSERT_TRUE(dev->setVoltageCode(5000));
    TEST_ASSERT_EQUAL_UINT16(4095, dev->_code);
}

// ----- setVolume -----

void test_set_volume_0_maps_to_0(void) {
    dev->init();
    TEST_ASSERT_TRUE(dev->setVolume(0));
    TEST_ASSERT_EQUAL_UINT16(0, dev->_code);
}

void test_set_volume_100_maps_to_4095(void) {
    dev->init();
    TEST_ASSERT_TRUE(dev->setVolume(100));
    TEST_ASSERT_EQUAL_UINT16(4095, dev->_code);
}

void test_set_volume_50_maps_to_approximately_2047(void) {
    dev->init();
    TEST_ASSERT_TRUE(dev->setVolume(50));
    // (50 * 4095) / 100 = 2047
    TEST_ASSERT_EQUAL_UINT16(2047, dev->_code);
}

void test_set_volume_clamps_over_100(void) {
    dev->init();
    TEST_ASSERT_TRUE(dev->setVolume(200));
    // Clamped to 100% → 4095
    TEST_ASSERT_EQUAL_UINT16(4095, dev->_code);
}

// ----- Descriptor -----

void test_descriptor_type_is_dac(void) {
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, (int)dev->getDescriptor().type);
}

void test_descriptor_channel_count_is_1(void) {
    TEST_ASSERT_EQUAL(1, dev->getDescriptor().channelCount);
}

void test_descriptor_compatible_string(void) {
    TEST_ASSERT_EQUAL_STRING("microchip,mcp4725", dev->getDescriptor().compatible);
}

void test_descriptor_has_hw_volume_capability(void) {
    TEST_ASSERT_TRUE(dev->getDescriptor().capabilities & HAL_CAP_HW_VOLUME);
}

void test_descriptor_no_adc_capability(void) {
    TEST_ASSERT_FALSE(dev->getDescriptor().capabilities & HAL_CAP_ADC_PATH);
}

void test_descriptor_i2c_address_default(void) {
    TEST_ASSERT_EQUAL_UINT8(0x60, dev->getDescriptor().i2cAddr);
}

void test_descriptor_alternate_address(void) {
    HalMcp4725Mock* dev61 = new HalMcp4725Mock(0x61, 2);
    TEST_ASSERT_EQUAL_UINT8(0x61, dev61->getDescriptor().i2cAddr);
    delete dev61;
}

void test_descriptor_bus_type_i2c(void) {
    TEST_ASSERT_EQUAL(HAL_BUS_I2C, (int)dev->getDescriptor().bus.type);
}

void test_descriptor_bus_index_expansion(void) {
    // Default bus index = 2 (HAL_I2C_BUS_EXP)
    TEST_ASSERT_EQUAL(2, dev->getDescriptor().bus.index);
}

void test_descriptor_has_dac_path_capability(void) {
    TEST_ASSERT_TRUE(dev->getDescriptor().capabilities & HAL_CAP_DAC_PATH);
}

// ----- buildSink -----

void test_mcp4725_buildSink_populates_struct(void) {
    dev->_state = HAL_STATE_AVAILABLE;
    dev->_ready = true;

    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    bool ok = dev->buildSink(0, &sink);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_NOT_NULL(sink.write);
    TEST_ASSERT_NOT_NULL(sink.isReady);
    TEST_ASSERT_EQUAL(dev->getSlot(), sink.halSlot);
    TEST_ASSERT_EQUAL_STRING("MCP4725", sink.name);
    TEST_ASSERT_EQUAL(0, sink.firstChannel);
    TEST_ASSERT_EQUAL(1, sink.channelCount);
    TEST_ASSERT_EQUAL_PTR(dev, sink.ctx);
}

void test_mcp4725_buildSink_null_returns_false(void) {
    TEST_ASSERT_FALSE(dev->buildSink(0, nullptr));
}

void test_mcp4725_buildSink_ready_callback(void) {
    dev->_ready = true;

    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    dev->buildSink(0, &sink);

    // isReady should return true when device is ready
    TEST_ASSERT_TRUE(sink.isReady());

    dev->_ready = false;
    TEST_ASSERT_FALSE(sink.isReady());
}

void test_mcp4725_buildSink_invalid_slot_returns_false(void) {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_FALSE(dev->buildSink(AUDIO_OUT_MAX_SINKS, &sink));
    TEST_ASSERT_FALSE(dev->buildSink(255, &sink));
}

// ===== Main =====
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_probe_succeeds);
    RUN_TEST(test_init_marks_available);
    RUN_TEST(test_init_zeroes_output);
    RUN_TEST(test_deinit_marks_removed);
    RUN_TEST(test_deinit_zeroes_output);
    RUN_TEST(test_health_check_returns_ready);
    RUN_TEST(test_health_check_false_when_not_ready);
    RUN_TEST(test_set_voltage_code_zero);
    RUN_TEST(test_set_voltage_code_max);
    RUN_TEST(test_set_voltage_code_mid);
    RUN_TEST(test_set_voltage_code_clamps_overflow);
    RUN_TEST(test_set_volume_0_maps_to_0);
    RUN_TEST(test_set_volume_100_maps_to_4095);
    RUN_TEST(test_set_volume_50_maps_to_approximately_2047);
    RUN_TEST(test_set_volume_clamps_over_100);
    RUN_TEST(test_descriptor_type_is_dac);
    RUN_TEST(test_descriptor_channel_count_is_1);
    RUN_TEST(test_descriptor_compatible_string);
    RUN_TEST(test_descriptor_has_hw_volume_capability);
    RUN_TEST(test_descriptor_no_adc_capability);
    RUN_TEST(test_descriptor_i2c_address_default);
    RUN_TEST(test_descriptor_alternate_address);
    RUN_TEST(test_descriptor_bus_type_i2c);
    RUN_TEST(test_descriptor_bus_index_expansion);
    RUN_TEST(test_descriptor_has_dac_path_capability);
    RUN_TEST(test_mcp4725_buildSink_populates_struct);
    RUN_TEST(test_mcp4725_buildSink_null_returns_false);
    RUN_TEST(test_mcp4725_buildSink_ready_callback);
    RUN_TEST(test_mcp4725_buildSink_invalid_slot_returns_false);

    return UNITY_END();
}
