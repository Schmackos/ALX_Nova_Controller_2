// test_hal_custom_device.cpp
// Tests HalCustomDevice (user-defined schema device):
//   Phase 3 additions — real I2C probe, Tier 2 init register sequences,
//   buildSink, getInputSource, capability parsing, type mapping,
//   compatible auto-generation, and SDIO guard.
//   Also retains Phase-4 HalDeviceConfig round-trip tests.
//
// Capability flag values tested:
//   HAL_CAP_HW_VOLUME  = 1 << 0 = 1
//   HAL_CAP_FILTERS    = 1 << 1 = 2
//   HAL_CAP_MUTE       = 1 << 2 = 4
//   HAL_CAP_ADC_PATH   = 1 << 3 = 8
//   HAL_CAP_DAC_PATH   = 1 << 4 = 16
//   HAL_CAP_PGA_CONTROL= 1 << 5 = 32
//   HAL_CAP_HPF_CONTROL= 1 << 6 = 64
//   HAL_CAP_CODEC      = 1 << 7 = 128

#include <unity.h>
#include <cstring>
#include <cstdio>
#include <cctype>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Wire.h"
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_audio_device.h"
#include "../../src/hal/hal_custom_device.h"
#include "../../src/audio_output_sink.h"

// Inline the implementation so the linker has the symbols
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"
// hal_i2c_bus.cpp must come before hal_custom_device.cpp (and hal_wifi_sdio_active
// stub must be defined before this include — see line below)
#include "../../src/hal/hal_i2c_bus.cpp"
#include "../../src/hal/hal_custom_device.cpp"

// Inline discovery just for hal_get_unmatched_addresses / hal_wifi_sdio_active stubs
// (not pulling the full implementation — we use the static storage directly)
#include "../../src/hal/hal_discovery.h"

// Native stub for hal_wifi_sdio_active (overrides the one in hal_discovery.cpp)
// Tests can set this to simulate WiFi active / inactive.
static bool s_wifiSdioActive = false;
bool hal_wifi_sdio_active() { return s_wifiSdioActive; }

// Native stubs for hal_get_unmatched_addresses / unmatched table
// (discovery.cpp is NOT included — only the stubs needed here)
static HalUnmatchedAddr s_unmatchedBuf[HAL_UNMATCHED_MAX];
static int              s_unmatchedCount = 0;
int hal_get_unmatched_addresses(HalUnmatchedAddr* out, int maxOut) {
    if (!out || maxOut <= 0) return 0;
    int n = (s_unmatchedCount < maxOut) ? s_unmatchedCount : maxOut;
    for (int i = 0; i < n; i++) out[i] = s_unmatchedBuf[i];
    return n;
}

// Note: i2s_port_* stubs are provided by i2s_audio.h NATIVE_TEST inline section.
// audio_pipeline and sink_write_utils stubs are provided by the respective headers.

// Native stubs for audio_pipeline functions used by write callback
float  audio_pipeline_get_sink_volume(uint8_t) { return 1.0f; }
bool   audio_pipeline_is_sink_muted(uint8_t)   { return false; }

// Native stubs for sink_write_utils (not linked in native test)
void sink_apply_volume(float*, size_t, float) {}
void sink_apply_mute_ramp(float*, size_t, float*, bool) {}
void sink_float_to_i2s_int32(const float*, int32_t*, size_t) {}

// The custom DAC slot device table is static in hal_custom_device.cpp.
// Expose a reset helper so setUp() can clear dangling pointers to stack devices.
// Without this, the process crashes on teardown when static destructors fire
// and encounter dangling pointers from stack-allocated test devices.
extern HalCustomDevice* _custom_dac_slot_dev[AUDIO_OUT_MAX_SINKS];

// ===== Inline capability flag extensions =====
#ifndef HAL_CAP_PGA_CONTROL
#define HAL_CAP_PGA_CONTROL  (1 << 5)
#endif
#ifndef HAL_CAP_HPF_CONTROL
#define HAL_CAP_HPF_CONTROL  (1 << 6)
#endif
#ifndef HAL_CAP_CODEC
#define HAL_CAP_CODEC        (1 << 7)
#endif

// ===== Fixtures =====
void setUp(void) {
    HalDeviceManager::instance().reset();
    hal_registry_reset();
    WireMock::reset();
    s_wifiSdioActive = false;
    s_unmatchedCount = 0;
    // Clear the static DAC slot table to remove dangling pointers from previous tests
    for (int i = 0; i < AUDIO_OUT_MAX_SINKS; i++) _custom_dac_slot_dev[i] = nullptr;
}

void tearDown(void) {}

// ==========================================================================
// Section 1: HalDeviceConfig field round-trips (retained from original tests)
// ==========================================================================

void test_new_config_fields_default_to_zero(void) {
    HalCustomDevice dev("test,dev", "Test Device", HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_BUILTIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);

    HalDeviceConfig* cfg = mgr.getConfig((uint8_t)slot);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL(0, cfg->mclkMultiple);
    TEST_ASSERT_EQUAL(0, cfg->i2sFormat);
    TEST_ASSERT_EQUAL(0, cfg->pgaGain);
    TEST_ASSERT_FALSE(cfg->hpfEnabled);
}

void test_set_mclk_multiple_roundtrip(void) {
    HalCustomDevice dev("test,dev", "Test Device", HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_BUILTIN);

    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mclkMultiple = 384;
    cfg.enabled = true;
    cfg.pinSda = -1; cfg.pinScl = -1;
    cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255;

    TEST_ASSERT_TRUE(mgr.setConfig((uint8_t)slot, cfg));
    HalDeviceConfig* got = mgr.getConfig((uint8_t)slot);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL(384, got->mclkMultiple);
    TEST_ASSERT_TRUE(got->valid);
}

void test_set_i2s_format_roundtrip(void) {
    HalCustomDevice dev("test,dev2", "Test Device 2", HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_BUILTIN);

    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.i2sFormat = 1;
    cfg.pinSda = -1; cfg.pinScl = -1;
    cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255;
    cfg.enabled = true;

    mgr.setConfig((uint8_t)slot, cfg);
    TEST_ASSERT_EQUAL(1, mgr.getConfig((uint8_t)slot)->i2sFormat);
}

void test_set_pga_gain_roundtrip(void) {
    HalCustomDevice dev("test,adc", "Test ADC",
                        HAL_CAP_ADC_PATH | HAL_CAP_PGA_CONTROL, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_BUILTIN);

    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.pgaGain = 18;
    cfg.pinSda = -1; cfg.pinScl = -1;
    cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255;
    cfg.enabled = true;

    mgr.setConfig((uint8_t)slot, cfg);
    TEST_ASSERT_EQUAL(18, mgr.getConfig((uint8_t)slot)->pgaGain);
}

void test_hpf_enabled_default_and_set(void) {
    HalCustomDevice dev("test,adc2", "Test ADC 2", HAL_CAP_ADC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_BUILTIN);

    TEST_ASSERT_FALSE(mgr.getConfig((uint8_t)slot)->hpfEnabled);

    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.hpfEnabled = true;
    cfg.pinSda = -1; cfg.pinScl = -1;
    cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255;
    cfg.enabled = true;

    mgr.setConfig((uint8_t)slot, cfg);
    TEST_ASSERT_TRUE(mgr.getConfig((uint8_t)slot)->hpfEnabled);
}

void test_pa_control_pin_default_minus_one(void) {
    HalCustomDevice dev("test,amp", "Test Amp", HAL_CAP_DAC_PATH, HAL_BUS_GPIO);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_BUILTIN);
    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.paControlPin = -1;
    cfg.pinSda = -1; cfg.pinScl = -1;
    cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255;
    cfg.enabled = true;
    mgr.setConfig((uint8_t)slot, cfg);
    TEST_ASSERT_EQUAL(-1, mgr.getConfig((uint8_t)slot)->paControlPin);
}

void test_cap_pga_control_is_bit5(void) {
    TEST_ASSERT_EQUAL(32, (int)HAL_CAP_PGA_CONTROL);
}

void test_cap_hpf_control_is_bit6(void) {
    TEST_ASSERT_EQUAL(64, (int)HAL_CAP_HPF_CONTROL);
}

void test_cap_codec_is_bit7(void) {
    TEST_ASSERT_EQUAL(128, (int)HAL_CAP_CODEC);
}

void test_config_valid_starts_false(void) {
    HalCustomDevice dev("test,x", "X", HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_BUILTIN);
    TEST_ASSERT_FALSE(mgr.getConfig((uint8_t)slot)->valid);
}

void test_get_config_returns_nonnull_for_valid_slot(void) {
    HalDeviceManager& mgr = HalDeviceManager::instance();
    for (uint8_t s = 0; s < HAL_MAX_DEVICES; s++) {
        TEST_ASSERT_NOT_NULL(mgr.getConfig(s));
    }
    TEST_ASSERT_NULL(mgr.getConfig(HAL_MAX_DEVICES));
}

void test_user_label_truncated_at_32_chars(void) {
    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    const char* longLabel = "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345_extra";
    strncpy(cfg.userLabel, longLabel, sizeof(cfg.userLabel) - 1);
    cfg.userLabel[sizeof(cfg.userLabel) - 1] = '\0';
    TEST_ASSERT_EQUAL(32, (int)strlen(cfg.userLabel));
    TEST_ASSERT_EQUAL_CHAR('A', cfg.userLabel[0]);
    TEST_ASSERT_EQUAL_CHAR('5', cfg.userLabel[31]);
}

// ==========================================================================
// Section 2: Retained lifecycle tests
// ==========================================================================

void test_custom_device_init_available(void) {
    HalCustomDevice dev("vendor,mydev2", "My DAC 2", HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    TEST_ASSERT_TRUE(dev.init().success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)dev._state);
    TEST_ASSERT_TRUE(dev._ready);
}

void test_custom_device_configure_stores_to_config(void) {
    HalCustomDevice dev("vendor,cfg", "Config Test", HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();
    TEST_ASSERT_TRUE(dev.configure(96000, 24));
    HalDeviceConfig* cfg = mgr.getConfig((uint8_t)slot);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL(96000u, cfg->sampleRate);
    TEST_ASSERT_EQUAL(24, cfg->bitDepth);
}

void test_custom_device_set_volume_caches(void) {
    HalCustomDevice dev("vendor,vol", "Vol Test", HAL_CAP_HW_VOLUME, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();
    TEST_ASSERT_TRUE(dev.setVolume(75));
}

void test_custom_device_set_mute_caches(void) {
    HalCustomDevice dev("vendor,mute", "Mute Test", HAL_CAP_MUTE, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();
    TEST_ASSERT_TRUE(dev.setMute(true));
    TEST_ASSERT_TRUE(dev.setMute(false));
}

void test_custom_device_deinit_sets_removed(void) {
    HalCustomDevice dev("vendor,deinit", "Deinit Test", HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();
    dev.deinit();
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)dev._state);
    TEST_ASSERT_FALSE(dev._ready);
}

// ==========================================================================
// Section 3: Phase 3 — I2C probe tests
// ==========================================================================

// ----- 1. I2C probe success — Wire mock ACKs the address -----
void test_custom_device_i2c_probe_success(void) {
    // Register the device in WireMock so it ACKs
    WireMock::registerDevice(0x48, 2);

    HalCustomDevice dev("custom,my-dac", "My DAC", HAL_CAP_DAC_PATH, HAL_BUS_I2C);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.setI2cConfig(0x48, 2);

    TEST_ASSERT_TRUE(dev.probe());
    TEST_ASSERT_EQUAL_STRING("", dev.getLastError());
    (void)slot;
}

// ----- 2. I2C probe failure — Wire mock returns NACK -----
void test_custom_device_i2c_probe_failure(void) {
    // Address 0x31 is NOT registered in WireMock — will NACK
    HalCustomDevice dev("custom,absent", "Absent DAC", HAL_CAP_DAC_PATH, HAL_BUS_I2C);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.setI2cConfig(0x31, 2);

    TEST_ASSERT_FALSE(dev.probe());
    // setLastError must have been called with a non-empty message
    TEST_ASSERT_TRUE(strlen(dev.getLastError()) > 0);
}

// ----- 3. Bus 0 SDIO guard — probe returns false when WiFi active -----
void test_custom_device_bus0_sdio_guard(void) {
    s_wifiSdioActive = true;

    HalCustomDevice dev("custom,sdio-guard", "SDIO Guard", HAL_CAP_DAC_PATH, HAL_BUS_I2C);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.setI2cConfig(0x48, HAL_I2C_BUS_EXT);  // Bus 0

    TEST_ASSERT_FALSE(dev.probe());
    TEST_ASSERT_TRUE(strlen(dev.getLastError()) > 0);
}

// ----- 4. I2S-only device: probe() always returns true (no Wire calls) -----
void test_custom_device_passthrough_no_i2c(void) {
    // Don't register anything in WireMock — no I2C probe should happen
    HalCustomDevice dev("custom,i2s-passthrough", "I2S Passthrough",
                        HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);

    TEST_ASSERT_TRUE(dev.probe());
    // Wire must not have been touched
    TEST_ASSERT_EQUAL(0, (int)WireMock::currentAddr);
}

// ==========================================================================
// Section 4: Phase 3 — Init register sequence (Tier 2)
// ==========================================================================

// ----- 5. Init sequence writes registers in order -----
void test_custom_device_init_sequence_writes_registers(void) {
    // Register the device at 0x48 in WireMock
    WireMock::registerDevice(0x48, 2);

    HalCustomDevice dev("custom,seq-dac", "Seq DAC", HAL_CAP_DAC_PATH, HAL_BUS_I2C);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.setI2cConfig(0x48, 2);

    HalInitRegPair seq[] = {
        {0x00, 0xAB},
        {0x01, 0xCD},
        {0x10, 0xFF},
    };
    dev.setInitSequence(seq, 3);

    HalInitResult result = dev.init();
    TEST_ASSERT_TRUE(result.success);

    // Verify all three registers were written to the mock register map
    TEST_ASSERT_EQUAL_HEX8(0xAB, WireMock::registerMap[0x48][0x00]);
    TEST_ASSERT_EQUAL_HEX8(0xCD, WireMock::registerMap[0x48][0x01]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, WireMock::registerMap[0x48][0x10]);
}

// ----- 6. Init sequence failure on write error -----
// The Wire mock returns NACK if the address is not registered.
// To simulate a write failure mid-sequence, we'll unregister the device
// after the sequence is set — causing the write to NACK on the first pair.
void test_custom_device_init_sequence_failure_on_write_error(void) {
    // NOTE: address NOT registered — any write will NACK (err=2)
    HalCustomDevice dev("custom,fail-seq", "Fail Seq", HAL_CAP_DAC_PATH, HAL_BUS_I2C);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.setI2cConfig(0x55, 2);

    HalInitRegPair seq[] = {{0x00, 0x01}, {0x01, 0x02}, {0x02, 0x03}};
    dev.setInitSequence(seq, 3);

    HalInitResult result = dev.init();
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_TRUE(strlen(dev.getLastError()) > 0);
}

// ----- 7. setInitSequence clamps to HAL_CUSTOM_MAX_INIT_REGS -----
void test_custom_device_max_init_sequence_clamped(void) {
    HalCustomDevice dev("custom,clamp", "Clamp Test", HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);

    // Provide 40 pairs — 8 more than the 32-pair limit
    HalInitRegPair bigSeq[40];
    for (int i = 0; i < 40; i++) {
        bigSeq[i].reg = (uint8_t)i;
        bigSeq[i].val = (uint8_t)(i + 1);
    }
    dev.setInitSequence(bigSeq, 40);

    // After clamping, init sequence should succeed (I2S bus, no I2C writes needed)
    HalInitResult result = dev.init();
    TEST_ASSERT_TRUE(result.success);
    // We can't inspect _initSeqCount directly since it's private,
    // but we verify the device still initialises correctly.
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)dev._state);
}

// ----- 8. Empty init sequence succeeds without any I2C writes -----
void test_custom_device_empty_init_sequence_succeeds(void) {
    HalCustomDevice dev("custom,empty-seq", "Empty Seq", HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);

    // No init sequence set
    HalInitResult result = dev.init();
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)dev._state);
}

// ==========================================================================
// Section 5: Phase 3 — buildSink (DAC path)
// ==========================================================================

// ----- 9. buildSink returns true for DAC-capable device -----
void test_custom_device_build_sink_dac(void) {
    HalCustomDevice dev("custom,dac-sink", "DAC Sink",
                        HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();

    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_TRUE(dev.buildSink(0, &out));
    TEST_ASSERT_NOT_NULL(out.name);
    TEST_ASSERT_EQUAL(2, out.channelCount);
    TEST_ASSERT_EQUAL(0, out.firstChannel);  // slot 0 -> ch 0
}

// ----- 10. buildSink returns false for non-DAC device -----
void test_custom_device_build_sink_adc_returns_false(void) {
    HalCustomDevice dev("custom,adc-no-sink", "ADC No Sink",
                        HAL_CAP_ADC_PATH, HAL_BUS_I2S, HAL_DEV_ADC);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();

    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_FALSE(dev.buildSink(0, &out));
}

// ----- 11. buildSink with null out returns false -----
void test_custom_device_build_sink_null_out(void) {
    HalCustomDevice dev("custom,null-sink", "Null Sink", HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();
    TEST_ASSERT_FALSE(dev.buildSink(0, nullptr));
}

// ----- 12. buildSink with out-of-range slot returns false -----
void test_custom_device_build_sink_slot_overflow(void) {
    HalCustomDevice dev("custom,overflow-sink", "Overflow Sink",
                        HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();

    AudioOutputSink out = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_FALSE(dev.buildSink(AUDIO_OUT_MAX_SINKS, &out));
}

// ==========================================================================
// Section 6: Phase 3 — getInputSource (ADC path)
// ==========================================================================

// ----- 13. ADC device provides an input source after init -----
void test_custom_device_adc_provides_input_source(void) {
    HalCustomDevice dev("custom,adc-src", "ADC Source",
                        HAL_CAP_ADC_PATH, HAL_BUS_I2S, HAL_DEV_ADC);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();

    const AudioInputSource* src = dev.getInputSource();
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_NOT_NULL(src->name);
}

// ----- 14. DAC-only device returns nullptr for input source -----
void test_custom_device_dac_no_input_source(void) {
    HalCustomDevice dev("custom,dac-only", "DAC Only",
                        HAL_CAP_DAC_PATH, HAL_BUS_I2S, HAL_DEV_DAC);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();

    TEST_ASSERT_NULL(dev.getInputSource());
}

// ----- 15. Input source is cleared after deinit -----
void test_custom_device_input_source_cleared_after_deinit(void) {
    HalCustomDevice dev("custom,adc-deinit", "ADC Deinit",
                        HAL_CAP_ADC_PATH, HAL_BUS_I2S, HAL_DEV_ADC);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();
    TEST_ASSERT_NOT_NULL(dev.getInputSource());
    dev.deinit();
    TEST_ASSERT_NULL(dev.getInputSource());
}

// ==========================================================================
// Section 7: Phase 3 — Capability parsing
// ==========================================================================

// ----- 16. dac_path capability sets HAL_CAP_DAC_PATH -----
void test_custom_device_capability_dac_path(void) {
    uint16_t caps = HAL_CAP_DAC_PATH;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_FALSE(caps & HAL_CAP_ADC_PATH);

    HalCustomDevice dev("custom,cap-dac", "Cap DAC", caps, HAL_BUS_I2S);
    TEST_ASSERT_TRUE(dev.getDescriptor().capabilities & HAL_CAP_DAC_PATH);
}

// ----- 17. adc_path capability sets HAL_CAP_ADC_PATH -----
void test_custom_device_capability_adc_path(void) {
    uint16_t caps = HAL_CAP_ADC_PATH | HAL_CAP_PGA_CONTROL;
    HalCustomDevice dev("custom,cap-adc", "Cap ADC", caps, HAL_BUS_I2S, HAL_DEV_ADC);
    uint16_t got = dev.getDescriptor().capabilities;
    TEST_ASSERT_TRUE(got & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(got & HAL_CAP_PGA_CONTROL);
    TEST_ASSERT_FALSE(got & HAL_CAP_DAC_PATH);
}

// ----- 18. Combined volume_control + mute capabilities -----
void test_custom_device_capability_volume_and_mute(void) {
    uint16_t caps = HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE;
    HalCustomDevice dev("custom,cap-vm", "Cap Vol+Mute", caps, HAL_BUS_I2S);
    uint16_t got = dev.getDescriptor().capabilities;
    TEST_ASSERT_TRUE(got & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(got & HAL_CAP_MUTE);
}

// ==========================================================================
// Section 8: Phase 3 — Type mapping
// ==========================================================================

// ----- 19. HAL_DEV_DAC type -----
void test_custom_device_type_dac(void) {
    HalCustomDevice dev("custom,type-dac", "Type DAC",
                        HAL_CAP_DAC_PATH, HAL_BUS_I2S, HAL_DEV_DAC);
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, (int)dev.getDescriptor().type);
}

// ----- 20. HAL_DEV_ADC type -----
void test_custom_device_type_adc(void) {
    HalCustomDevice dev("custom,type-adc", "Type ADC",
                        HAL_CAP_ADC_PATH, HAL_BUS_I2S, HAL_DEV_ADC);
    TEST_ASSERT_EQUAL(HAL_DEV_ADC, (int)dev.getDescriptor().type);
}

// ----- 21. HAL_DEV_CODEC type -----
void test_custom_device_type_codec(void) {
    HalCustomDevice dev("custom,type-codec", "Type Codec",
                        HAL_CAP_DAC_PATH | HAL_CAP_ADC_PATH, HAL_BUS_I2C, HAL_DEV_CODEC);
    TEST_ASSERT_EQUAL(HAL_DEV_CODEC, (int)dev.getDescriptor().type);
}

// ==========================================================================
// Section 9: Phase 3 — Compatible string auto-generation
// ==========================================================================

// Helper: slugify a name the same way hal_api.cpp does
static void slugify(const char* name, char* out, size_t outLen) {
    size_t si = 0;
    for (size_t ci = 0; name[ci] && si < outLen - 1; ci++) {
        char c = name[ci];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
            out[si++] = c;
        } else if (c == ' ' || c == '_') {
            out[si++] = '-';
        }
    }
    out[si] = '\0';
}

// ----- 22. "My DIY DAC" -> slug "my-diy-dac" -> compatible "custom,my-diy-dac" -----
void test_custom_device_compatible_auto_generation(void) {
    char slug[30];
    slugify("My DIY DAC", slug, sizeof(slug));
    TEST_ASSERT_EQUAL_STRING("my-diy-dac", slug);

    char compatible[36];
    snprintf(compatible, sizeof(compatible), "custom,%s", slug);
    TEST_ASSERT_EQUAL_STRING("custom,my-diy-dac", compatible);
}

// ----- 23. "AudioDAC_V2" -> slug "audiodac-v2" -> compatible "custom,audiodac-v2" -----
void test_custom_device_compatible_underscore_handling(void) {
    char slug[30];
    slugify("AudioDAC_V2", slug, sizeof(slug));
    TEST_ASSERT_EQUAL_STRING("audiodac-v2", slug);

    char compatible[36];
    snprintf(compatible, sizeof(compatible), "custom,%s", slug);
    TEST_ASSERT_EQUAL_STRING("custom,audiodac-v2", compatible);
}

// ==========================================================================
// Section 10: Phase 3 — Unmatched address API
// ==========================================================================

// ----- 24. hal_get_unmatched_addresses returns 0 initially -----
void test_unmatched_addresses_empty_initially(void) {
    HalUnmatchedAddr buf[HAL_UNMATCHED_MAX];
    int n = hal_get_unmatched_addresses(buf, HAL_UNMATCHED_MAX);
    TEST_ASSERT_EQUAL(0, n);
}

// ----- 25. hal_get_unmatched_addresses returns stored entries -----
void test_unmatched_addresses_returns_stored_entries(void) {
    s_unmatchedBuf[0].addr = 0x20;
    s_unmatchedBuf[0].bus  = 2;
    s_unmatchedBuf[1].addr = 0x30;
    s_unmatchedBuf[1].bus  = 1;
    s_unmatchedCount = 2;

    HalUnmatchedAddr out[HAL_UNMATCHED_MAX];
    int n = hal_get_unmatched_addresses(out, HAL_UNMATCHED_MAX);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL_HEX8(0x20, out[0].addr);
    TEST_ASSERT_EQUAL(2, out[0].bus);
    TEST_ASSERT_EQUAL_HEX8(0x30, out[1].addr);
    TEST_ASSERT_EQUAL(1, out[1].bus);
}

// ----- 26. hal_get_unmatched_addresses honours maxOut -----
void test_unmatched_addresses_honours_max_out(void) {
    for (int i = 0; i < 5; i++) {
        s_unmatchedBuf[i].addr = (uint8_t)(0x10 + i);
        s_unmatchedBuf[i].bus  = 0;
    }
    s_unmatchedCount = 5;

    HalUnmatchedAddr out[3];
    int n = hal_get_unmatched_addresses(out, 3);
    TEST_ASSERT_EQUAL(3, n);  // capped at maxOut
    TEST_ASSERT_EQUAL_HEX8(0x10, out[0].addr);
    TEST_ASSERT_EQUAL_HEX8(0x12, out[2].addr);
}

// ----- 27. hal_get_unmatched_addresses returns 0 on null out -----
void test_unmatched_addresses_null_out(void) {
    s_unmatchedCount = 2;
    int n = hal_get_unmatched_addresses(nullptr, 10);
    TEST_ASSERT_EQUAL(0, n);
}

// ==========================================================================
// Main
// ==========================================================================
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    // Section 1: HalDeviceConfig field round-trips
    RUN_TEST(test_new_config_fields_default_to_zero);
    RUN_TEST(test_set_mclk_multiple_roundtrip);
    RUN_TEST(test_set_i2s_format_roundtrip);
    RUN_TEST(test_set_pga_gain_roundtrip);
    RUN_TEST(test_hpf_enabled_default_and_set);
    RUN_TEST(test_pa_control_pin_default_minus_one);
    RUN_TEST(test_cap_pga_control_is_bit5);
    RUN_TEST(test_cap_hpf_control_is_bit6);
    RUN_TEST(test_cap_codec_is_bit7);
    RUN_TEST(test_config_valid_starts_false);
    RUN_TEST(test_get_config_returns_nonnull_for_valid_slot);
    RUN_TEST(test_user_label_truncated_at_32_chars);

    // Section 2: Retained lifecycle tests
    RUN_TEST(test_custom_device_init_available);
    RUN_TEST(test_custom_device_configure_stores_to_config);
    RUN_TEST(test_custom_device_set_volume_caches);
    RUN_TEST(test_custom_device_set_mute_caches);
    RUN_TEST(test_custom_device_deinit_sets_removed);

    // Section 3: I2C probe
    RUN_TEST(test_custom_device_i2c_probe_success);
    RUN_TEST(test_custom_device_i2c_probe_failure);
    RUN_TEST(test_custom_device_bus0_sdio_guard);
    RUN_TEST(test_custom_device_passthrough_no_i2c);

    // Section 4: Init register sequence
    RUN_TEST(test_custom_device_init_sequence_writes_registers);
    RUN_TEST(test_custom_device_init_sequence_failure_on_write_error);
    RUN_TEST(test_custom_device_max_init_sequence_clamped);
    RUN_TEST(test_custom_device_empty_init_sequence_succeeds);

    // Section 5: buildSink
    RUN_TEST(test_custom_device_build_sink_dac);
    RUN_TEST(test_custom_device_build_sink_adc_returns_false);
    RUN_TEST(test_custom_device_build_sink_null_out);
    RUN_TEST(test_custom_device_build_sink_slot_overflow);

    // Section 6: getInputSource
    RUN_TEST(test_custom_device_adc_provides_input_source);
    RUN_TEST(test_custom_device_dac_no_input_source);
    RUN_TEST(test_custom_device_input_source_cleared_after_deinit);

    // Section 7: Capability parsing
    RUN_TEST(test_custom_device_capability_dac_path);
    RUN_TEST(test_custom_device_capability_adc_path);
    RUN_TEST(test_custom_device_capability_volume_and_mute);

    // Section 8: Type mapping
    RUN_TEST(test_custom_device_type_dac);
    RUN_TEST(test_custom_device_type_adc);
    RUN_TEST(test_custom_device_type_codec);

    // Section 9: Compatible auto-generation
    RUN_TEST(test_custom_device_compatible_auto_generation);
    RUN_TEST(test_custom_device_compatible_underscore_handling);

    // Section 10: Unmatched address API
    RUN_TEST(test_unmatched_addresses_empty_initially);
    RUN_TEST(test_unmatched_addresses_returns_stored_entries);
    RUN_TEST(test_unmatched_addresses_honours_max_out);
    RUN_TEST(test_unmatched_addresses_null_out);

    return UNITY_END();
}
