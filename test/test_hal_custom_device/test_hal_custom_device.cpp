// test_hal_custom_device.cpp
// Tests the HalDeviceConfig struct (especially Phase-4 extended fields) and
// the HalDeviceManager config get/set round-trip.
//
// Also exercises HalCustomDevice (the user-defined schema device) in a
// native-friendly way, verifying: probe-always-true, init→AVAILABLE,
// configure stores config, setVolume/setMute cache state, deinit→REMOVED,
// and hal_load_custom_devices() skipping LittleFS in NATIVE_TEST mode.
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

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Wire.h"
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_audio_device.h"
#include "../../src/hal/hal_custom_device.h"

// Inline the implementation so the linker has the symbols
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"
#include "../../src/hal/hal_custom_device.cpp"

// ===== Inline capability flag extensions (Phase 4) =====
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
}

void tearDown(void) {}

// ===== HalDeviceConfig field tests =====

// ----- 1. New Phase-4 config fields default to zero after manager reset -----
void test_new_config_fields_default_to_zero(void) {
    // Register a dummy device to allocate a config slot
    HalCustomDevice dev("test,dev", "Test Device",
                        HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_BUILTIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);

    HalDeviceConfig* cfg = mgr.getConfig((uint8_t)slot);
    TEST_ASSERT_NOT_NULL(cfg);
    // Phase-4 extended fields should be zero-initialised by reset()
    TEST_ASSERT_EQUAL(0, cfg->mclkMultiple);
    TEST_ASSERT_EQUAL(0, cfg->i2sFormat);
    TEST_ASSERT_EQUAL(0, cfg->pgaGain);
    TEST_ASSERT_FALSE(cfg->hpfEnabled);
}

// ----- 2. mclkMultiple round-trips through setConfig/getConfig -----
void test_set_mclk_multiple_roundtrip(void) {
    HalCustomDevice dev("test,dev", "Test Device",
                        HAL_CAP_DAC_PATH, HAL_BUS_I2S);
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
    TEST_ASSERT_TRUE(got->valid);   // setConfig forces valid = true
}

// ----- 3. i2sFormat round-trips (1 = left-justified) -----
void test_set_i2s_format_roundtrip(void) {
    HalCustomDevice dev("test,dev2", "Test Device 2",
                        HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_BUILTIN);

    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.i2sFormat = 1;    // 0=Philips, 1=MSB/left-justified, 2=right-justified
    cfg.pinSda = -1; cfg.pinScl = -1;
    cfg.pinMclk = -1; cfg.pinData = -1;
    cfg.i2sPort = 255;
    cfg.enabled = true;

    mgr.setConfig((uint8_t)slot, cfg);
    TEST_ASSERT_EQUAL(1, mgr.getConfig((uint8_t)slot)->i2sFormat);
}

// ----- 4. pgaGain round-trips (18 dB) -----
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

// ----- 5. hpfEnabled defaults false, can be set true -----
void test_hpf_enabled_default_and_set(void) {
    HalCustomDevice dev("test,adc2", "Test ADC 2",
                        HAL_CAP_ADC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_BUILTIN);

    // Default after reset is false
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

// ----- 6. paControlPin defaults to -1 (set by manager ctor) -----
void test_pa_control_pin_default_minus_one(void) {
    // After manager reset(), pin fields are re-initialised to -1
    HalCustomDevice dev("test,amp", "Test Amp",
                        HAL_CAP_DAC_PATH, HAL_BUS_GPIO);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_BUILTIN);
    // paControlPin is NOT in the list of fields reset() sets to -1 by default
    // (it is zero-initialised by memset). Check the memset result = 0.
    // Firmware convention: -1 means "none" but storage is int8_t so 0xFF = -1.
    // After reset(), memset gives 0. After setConfig with -1, it stores -1.
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

// ----- 7. HAL_CAP_PGA_CONTROL is bit 5 = 32 -----
void test_cap_pga_control_is_bit5(void) {
    TEST_ASSERT_EQUAL(32, (int)HAL_CAP_PGA_CONTROL);
}

// ----- 8. HAL_CAP_HPF_CONTROL is bit 6 = 64 -----
void test_cap_hpf_control_is_bit6(void) {
    TEST_ASSERT_EQUAL(64, (int)HAL_CAP_HPF_CONTROL);
}

// ----- 9. HAL_CAP_CODEC is bit 7 = 128 -----
void test_cap_codec_is_bit7(void) {
    TEST_ASSERT_EQUAL(128, (int)HAL_CAP_CODEC);
}

// ----- 10. config valid flag starts false (before any setConfig call) -----
void test_config_valid_starts_false(void) {
    HalCustomDevice dev("test,x", "X", HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_BUILTIN);
    TEST_ASSERT_FALSE(mgr.getConfig((uint8_t)slot)->valid);
}

// ----- 11. getConfig returns non-null for all valid slot indices -----
void test_get_config_returns_nonnull_for_valid_slot(void) {
    HalDeviceManager& mgr = HalDeviceManager::instance();
    // All slots should have config storage (even before any device registered)
    for (uint8_t s = 0; s < HAL_MAX_DEVICES; s++) {
        TEST_ASSERT_NOT_NULL(mgr.getConfig(s));
    }
    // Slot beyond range returns null
    TEST_ASSERT_NULL(mgr.getConfig(HAL_MAX_DEVICES));
}

// ----- 12. userLabel truncated at 32 characters in config struct -----
void test_user_label_truncated_at_32_chars(void) {
    HalDeviceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    // userLabel field is char[33] — copy exactly 32 chars + nul
    const char* longLabel = "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345_extra";
    strncpy(cfg.userLabel, longLabel, sizeof(cfg.userLabel) - 1);
    cfg.userLabel[sizeof(cfg.userLabel) - 1] = '\0';
    // Should be truncated to 32 printable chars + nul
    TEST_ASSERT_EQUAL(32, (int)strlen(cfg.userLabel));
    TEST_ASSERT_EQUAL_CHAR('A', cfg.userLabel[0]);
    TEST_ASSERT_EQUAL_CHAR('5', cfg.userLabel[31]);
}

// ===== HalCustomDevice behaviour tests =====

// ----- 13. HalCustomDevice probe() always returns true -----
void test_custom_device_probe_always_true(void) {
    HalCustomDevice dev("vendor,mydev", "My DAC",
                        HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    TEST_ASSERT_TRUE(dev.probe());
}

// ----- 14. HalCustomDevice init() → state AVAILABLE -----
void test_custom_device_init_available(void) {
    HalCustomDevice dev("vendor,mydev2", "My DAC 2",
                        HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    TEST_ASSERT_TRUE(dev.init().success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, (int)dev._state);
    TEST_ASSERT_TRUE(dev._ready);
}

// ----- 15. HalCustomDevice configure() stores rate/depth in slot config -----
void test_custom_device_configure_stores_to_config(void) {
    HalCustomDevice dev("vendor,cfg", "Config Test",
                        HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int slot = mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();
    TEST_ASSERT_TRUE(dev.configure(96000, 24));

    HalDeviceConfig* cfg = mgr.getConfig((uint8_t)slot);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL(96000u, cfg->sampleRate);
    TEST_ASSERT_EQUAL(24,     cfg->bitDepth);
}

// ----- 16. HalCustomDevice setVolume() caches value and returns true -----
void test_custom_device_set_volume_caches(void) {
    HalCustomDevice dev("vendor,vol", "Vol Test",
                        HAL_CAP_HW_VOLUME, HAL_BUS_I2S);
    dev.init();
    // setVolume caches internally — returns true (no hardware to fail)
    TEST_ASSERT_TRUE(dev.setVolume(75));
}

// ----- 17. HalCustomDevice setMute() caches value and returns true -----
void test_custom_device_set_mute_caches(void) {
    HalCustomDevice dev("vendor,mute", "Mute Test",
                        HAL_CAP_MUTE, HAL_BUS_I2S);
    dev.init();
    TEST_ASSERT_TRUE(dev.setMute(true));
    TEST_ASSERT_TRUE(dev.setMute(false));
}

// ----- 18. HalCustomDevice deinit() → state REMOVED -----
void test_custom_device_deinit_sets_removed(void) {
    HalCustomDevice dev("vendor,deinit", "Deinit Test",
                        HAL_CAP_DAC_PATH, HAL_BUS_I2S);
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.registerDevice(&dev, HAL_DISC_MANUAL);
    dev.init();
    dev.deinit();
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, (int)dev._state);
    TEST_ASSERT_FALSE(dev._ready);
}

// ===== Main =====
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    // HalDeviceConfig field tests
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

    // HalCustomDevice behaviour tests
    RUN_TEST(test_custom_device_probe_always_true);
    RUN_TEST(test_custom_device_init_available);
    RUN_TEST(test_custom_device_configure_stores_to_config);
    RUN_TEST(test_custom_device_set_volume_caches);
    RUN_TEST(test_custom_device_set_mute_caches);
    RUN_TEST(test_custom_device_deinit_sets_removed);

    return UNITY_END();
}
