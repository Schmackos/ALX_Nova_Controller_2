#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline HAL implementation for native testing =====
// Tests don't compile src/ directly (test_build_src = no)
// Include the HAL headers — they are header-only or we inline the .cpp

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"

// Inline the .cpp files for native testing
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// ===== Test Device Implementation =====
class TestDevice : public HalDevice {
public:
    bool probeResult;
    bool initResult;
    bool healthResult;
    int  initCallCount;
    int  probeCallCount;
    int  healthCallCount;
    int  deinitCallCount;
    bool dumpConfigCalled;

    TestDevice()
        : probeResult(true), initResult(true), healthResult(true),
          initCallCount(0), probeCallCount(0), healthCallCount(0),
          deinitCallCount(0), dumpConfigCalled(false) {
        strncpy(_descriptor.compatible, "default", 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = HAL_DEV_NONE;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    TestDevice(const char* compatible, HalDeviceType type, uint16_t priority = HAL_PRIORITY_HARDWARE)
        : probeResult(true), initResult(true), healthResult(true),
          initCallCount(0), probeCallCount(0), healthCallCount(0),
          deinitCallCount(0), dumpConfigCalled(false) {
        strncpy(_descriptor.compatible, compatible, 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = type;
        _initPriority = priority;
    }

    bool probe() override { probeCallCount++; return probeResult; }
    HalInitResult init() override {
        initCallCount++;
        return initResult ? hal_init_ok() : hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override { deinitCallCount++; }
    void dumpConfig() override { dumpConfigCalled = true; }
    bool healthCheck() override { healthCallCount++; return healthResult; }
};

// ===== Test Fixtures =====
static HalDeviceManager* mgr;

void setUp() {
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
}

void tearDown() {}

// ===== Test 1: Register and retrieve device =====
void test_register_and_get_device() {
    TestDevice dev("ti,pcm5102a", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);
    TEST_ASSERT_EQUAL_PTR(&dev, mgr->getDevice(slot));
    TEST_ASSERT_EQUAL(1, mgr->getCount());
    TEST_ASSERT_EQUAL(HAL_DISC_BUILTIN, dev.getDiscovery());
    TEST_ASSERT_EQUAL(slot, dev.getSlot());
}

// ===== Test 2: Find by compatible string =====
void test_find_by_compatible() {
    TestDevice dev1("ti,pcm5102a", HAL_DEV_DAC);
    TestDevice dev2("evergrande,es8311", HAL_DEV_CODEC);
    mgr->registerDevice(&dev1, HAL_DISC_BUILTIN);
    mgr->registerDevice(&dev2, HAL_DISC_BUILTIN);

    HalDevice* found = mgr->findByCompatible("evergrande,es8311");
    TEST_ASSERT_EQUAL_PTR(&dev2, found);

    TEST_ASSERT_NULL(mgr->findByCompatible("nonexistent,device"));
    TEST_ASSERT_NULL(mgr->findByCompatible(nullptr));
}

// ===== Test 3: Find by type (with nth parameter) =====
void test_find_by_type() {
    TestDevice dac1("ti,pcm5102a", HAL_DEV_DAC);
    TestDevice codec("evergrande,es8311", HAL_DEV_CODEC);
    TestDevice dac2("ess,es9038q2m", HAL_DEV_DAC);
    mgr->registerDevice(&dac1, HAL_DISC_BUILTIN);
    mgr->registerDevice(&codec, HAL_DISC_BUILTIN);
    mgr->registerDevice(&dac2, HAL_DISC_EEPROM);

    TEST_ASSERT_EQUAL_PTR(&dac1, mgr->findByType(HAL_DEV_DAC, 0));
    TEST_ASSERT_EQUAL_PTR(&dac2, mgr->findByType(HAL_DEV_DAC, 1));
    TEST_ASSERT_NULL(mgr->findByType(HAL_DEV_DAC, 2));
    TEST_ASSERT_EQUAL_PTR(&codec, mgr->findByType(HAL_DEV_CODEC, 0));
    TEST_ASSERT_NULL(mgr->findByType(HAL_DEV_AMP, 0));
}

// ===== Test 4: Remove device =====
void test_remove_device() {
    TestDevice dev("ti,pcm5102a", HAL_DEV_DAC);
    dev._ready = true;
    dev._state = HAL_STATE_AVAILABLE;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    TEST_ASSERT_TRUE(mgr->removeDevice(slot));
    TEST_ASSERT_NULL(mgr->getDevice(slot));
    TEST_ASSERT_EQUAL(0, mgr->getCount());
    TEST_ASSERT_FALSE(dev._ready);
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, dev._state);

    // Remove again — should fail
    TEST_ASSERT_FALSE(mgr->removeDevice(slot));
}

// ===== Test 5: Max devices limit =====
void test_max_devices_limit() {
    TestDevice devs[HAL_MAX_DEVICES + 1];
    for (int i = 0; i < HAL_MAX_DEVICES + 1; i++) {
        char name[16];
        snprintf(name, sizeof(name), "dev%d", i);
        devs[i] = TestDevice(name, HAL_DEV_DAC);
    }

    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        TEST_ASSERT_GREATER_OR_EQUAL(0, mgr->registerDevice(&devs[i], HAL_DISC_BUILTIN));
    }
    TEST_ASSERT_EQUAL(HAL_MAX_DEVICES, mgr->getCount());

    // (HAL_MAX_DEVICES+1)th registration should fail
    TEST_ASSERT_EQUAL(-1, mgr->registerDevice(&devs[HAL_MAX_DEVICES], HAL_DISC_BUILTIN));
}

// ===== Test 6: State transitions =====
void test_state_transitions() {
    TestDevice dev("ti,pcm5102a", HAL_DEV_DAC);
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Initial state
    TEST_ASSERT_EQUAL(HAL_STATE_UNKNOWN, dev._state);
    TEST_ASSERT_FALSE(dev._ready);

    // Simulate discovery → configuring → available
    dev._state = HAL_STATE_DETECTED;
    TEST_ASSERT_EQUAL(HAL_STATE_DETECTED, dev._state);

    dev._state = HAL_STATE_CONFIGURING;
    TEST_ASSERT_EQUAL(HAL_STATE_CONFIGURING, dev._state);

    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev._state);
    TEST_ASSERT_TRUE(dev._ready);

    // Health check fail → unavailable
    dev._state = HAL_STATE_UNAVAILABLE;
    dev._ready = false;
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, dev._state);
    TEST_ASSERT_FALSE(dev._ready);

    // Error
    dev._state = HAL_STATE_ERROR;
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, dev._state);

    // Manual
    dev._state = HAL_STATE_MANUAL;
    TEST_ASSERT_EQUAL(HAL_STATE_MANUAL, dev._state);

    // Removed
    dev._state = HAL_STATE_REMOVED;
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, dev._state);
}

// ===== Test 7: Pin claim, conflict, release =====
void test_pin_claim_and_release() {
    // Claim a pin
    TEST_ASSERT_TRUE(mgr->claimPin(48, HAL_BUS_I2C, 0, 0));
    TEST_ASSERT_TRUE(mgr->isPinClaimed(48));

    // Conflict — same pin, different device
    TEST_ASSERT_FALSE(mgr->claimPin(48, HAL_BUS_I2C, 0, 1));

    // Release and reclaim
    TEST_ASSERT_TRUE(mgr->releasePin(48));
    TEST_ASSERT_FALSE(mgr->isPinClaimed(48));
    TEST_ASSERT_TRUE(mgr->claimPin(48, HAL_BUS_I2C, 1, 2));

    // Invalid GPIO
    TEST_ASSERT_FALSE(mgr->claimPin(-1, HAL_BUS_GPIO, 0, 0));
    TEST_ASSERT_FALSE(mgr->releasePin(99));
}

// ===== Test 8: Priority-sorted initAll =====
void test_priority_sorted_init() {
    // Create devices with different priorities
    TestDevice late("sensor", HAL_DEV_SENSOR, HAL_PRIORITY_LATE);      // 100
    TestDevice hw("dac", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);          // 800
    TestDevice bus("bus", HAL_DEV_NONE, HAL_PRIORITY_BUS);             // 1000

    mgr->registerDevice(&late, HAL_DISC_BUILTIN);
    mgr->registerDevice(&hw, HAL_DISC_BUILTIN);
    mgr->registerDevice(&bus, HAL_DISC_BUILTIN);

    mgr->initAll();

    // All should have been initialized
    TEST_ASSERT_EQUAL(1, bus.initCallCount);
    TEST_ASSERT_EQUAL(1, hw.initCallCount);
    TEST_ASSERT_EQUAL(1, late.initCallCount);

    // All should be AVAILABLE
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, bus._state);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, hw._state);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, late._state);
    TEST_ASSERT_TRUE(bus._ready);
    TEST_ASSERT_TRUE(hw._ready);
    TEST_ASSERT_TRUE(late._ready);
}

// ===== Test 9: initAll handles init failure =====
void test_init_failure_sets_error() {
    TestDevice good("good", HAL_DEV_DAC);
    TestDevice bad("bad", HAL_DEV_DAC);
    bad.initResult = false;

    mgr->registerDevice(&good, HAL_DISC_BUILTIN);
    mgr->registerDevice(&bad, HAL_DISC_BUILTIN);

    mgr->initAll();

    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, good._state);
    TEST_ASSERT_TRUE(good._ready);
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, bad._state);
    TEST_ASSERT_FALSE(bad._ready);
}

// ===== Test 10: healthCheckAll transitions =====
void test_health_check_transitions() {
    TestDevice dev("ti,pcm5102a", HAL_DEV_DAC);
    dev._state = HAL_STATE_AVAILABLE;
    dev._ready = true;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Health OK — stays available
    dev.healthResult = true;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev._state);
    TEST_ASSERT_TRUE(dev._ready);

    // Health fail → unavailable
    dev.healthResult = false;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(HAL_STATE_UNAVAILABLE, dev._state);
    TEST_ASSERT_FALSE(dev._ready);

    // Health recovers → available again (retry after 1s delay)
    dev.healthResult = true;
    dev.initResult = true;
    ArduinoMock::mockMillis = 1000; // Advance past retry delay
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev._state);
    TEST_ASSERT_TRUE(dev._ready);
}

// ===== Test 11: forEach iteration =====
void test_forEach_iterates_all() {
    TestDevice dev1("dev1", HAL_DEV_DAC);
    TestDevice dev2("dev2", HAL_DEV_ADC);
    mgr->registerDevice(&dev1, HAL_DISC_BUILTIN);
    mgr->registerDevice(&dev2, HAL_DISC_BUILTIN);

    int count = 0;
    mgr->forEach([](HalDevice* dev, void* ctx) {
        (*static_cast<int*>(ctx))++;
    }, &count);

    TEST_ASSERT_EQUAL(2, count);
}

// ===== Test 12: Driver registry by compatible =====
void test_registry_find_by_compatible() {
    HalDriverEntry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.compatible, "ti,pcm5102a", 31);
    entry.type = HAL_DEV_DAC;
    entry.legacyId = 0x0001;
    entry.factory = nullptr;

    TEST_ASSERT_TRUE(hal_registry_register(entry));
    TEST_ASSERT_EQUAL(1, hal_registry_count());

    const HalDriverEntry* found = hal_registry_find("ti,pcm5102a");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("ti,pcm5102a", found->compatible);
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, found->type);

    TEST_ASSERT_NULL(hal_registry_find("nonexistent"));
}

// ===== Test 13: Driver registry by legacy ID =====
void test_registry_find_by_legacy_id() {
    HalDriverEntry e1, e2;
    memset(&e1, 0, sizeof(e1));
    memset(&e2, 0, sizeof(e2));
    strncpy(e1.compatible, "ti,pcm5102a", 31);
    e1.type = HAL_DEV_DAC;
    e1.legacyId = 0x0001;

    strncpy(e2.compatible, "evergrande,es8311", 31);
    e2.type = HAL_DEV_CODEC;
    e2.legacyId = 0x0004;

    hal_registry_register(e1);
    hal_registry_register(e2);

    const HalDriverEntry* found = hal_registry_find_by_legacy_id(0x0004);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("evergrande,es8311", found->compatible);

    TEST_ASSERT_NULL(hal_registry_find_by_legacy_id(0x0000));
    TEST_ASSERT_NULL(hal_registry_find_by_legacy_id(0x9999));
}

// ===== Test 14: Registry rejects duplicates and empty compatible =====
void test_registry_rejects_invalid() {
    HalDriverEntry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.compatible, "ti,pcm5102a", 31);
    entry.type = HAL_DEV_DAC;

    TEST_ASSERT_TRUE(hal_registry_register(entry));
    // Duplicate
    TEST_ASSERT_FALSE(hal_registry_register(entry));

    // Empty compatible
    HalDriverEntry empty;
    memset(&empty, 0, sizeof(empty));
    TEST_ASSERT_FALSE(hal_registry_register(empty));
}

// ===== Test 15: Register 20 devices — no crash, all present =====
void test_register_20_devices() {
    TestDevice devs[20];
    for (int i = 0; i < 20; i++) {
        char name[16];
        snprintf(name, sizeof(name), "dev%d", i);
        devs[i] = TestDevice(name, HAL_DEV_DAC);
        int slot = mgr->registerDevice(&devs[i], HAL_DISC_BUILTIN);
        TEST_ASSERT_GREATER_OR_EQUAL(0, slot);
    }
    TEST_ASSERT_EQUAL(20, mgr->getCount());

    // Verify all 20 are retrievable
    int found = 0;
    mgr->forEach([](HalDevice* dev, void* ctx) {
        (*static_cast<int*>(ctx))++;
    }, &found);
    TEST_ASSERT_EQUAL(20, found);
}

// ===== Test 16: Register 25 exceeds HAL_MAX_DEVICES (24) — 25th rejected =====
void test_register_25_exceeds_max() {
    // HAL_MAX_DEVICES is 24 — we can register 24 but the 25th should fail
    TestDevice devs[25];
    for (int i = 0; i < 25; i++) {
        char name[16];
        snprintf(name, sizeof(name), "scale%d", i);
        devs[i] = TestDevice(name, HAL_DEV_DAC);
    }

    // Register 24 — should all succeed
    for (int i = 0; i < 24; i++) {
        int slot = mgr->registerDevice(&devs[i], HAL_DISC_BUILTIN);
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, slot,
            "Devices 0-23 should register successfully");
    }
    TEST_ASSERT_EQUAL(24, mgr->getCount());

    // 25th registration should fail with -1
    int slot25 = mgr->registerDevice(&devs[24], HAL_DISC_BUILTIN);
    TEST_ASSERT_EQUAL(-1, slot25);
    TEST_ASSERT_EQUAL(24, mgr->getCount());
}

// ===== Test 17: Remove and re-register uses freed slot =====
void test_remove_and_reregister_reclaims_slot() {
    TestDevice dev1("dev1", HAL_DEV_DAC);
    TestDevice dev2("dev2", HAL_DEV_ADC);
    TestDevice dev3("dev3", HAL_DEV_CODEC);

    int slot1 = mgr->registerDevice(&dev1, HAL_DISC_BUILTIN);
    int slot2 = mgr->registerDevice(&dev2, HAL_DISC_BUILTIN);
    TEST_ASSERT_EQUAL(2, mgr->getCount());

    // Remove slot 0
    mgr->removeDevice(slot1);
    TEST_ASSERT_EQUAL(1, mgr->getCount());
    TEST_ASSERT_NULL(mgr->getDevice(slot1));

    // Register a new device — should reuse the freed slot
    int slot3 = mgr->registerDevice(&dev3, HAL_DISC_BUILTIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot3);
    TEST_ASSERT_EQUAL(2, mgr->getCount());
    TEST_ASSERT_EQUAL_PTR(&dev3, mgr->getDevice(slot3));
}

// ===== Test 18: High GPIO numbers (53, 54) claim and conflict =====
void test_pin_claim_high_gpio() {
    TEST_ASSERT_TRUE(mgr->claimPin(53, HAL_BUS_GPIO, 0, 0));
    TEST_ASSERT_TRUE(mgr->isPinClaimed(53));
    TEST_ASSERT_TRUE(mgr->claimPin(54, HAL_BUS_I2C, 0, 1));
    TEST_ASSERT_TRUE(mgr->isPinClaimed(54));

    // Conflict detection works at high GPIO numbers
    TEST_ASSERT_FALSE(mgr->claimPin(53, HAL_BUS_GPIO, 0, 2));
    TEST_ASSERT_FALSE(mgr->claimPin(54, HAL_BUS_I2C, 0, 3));

    // Release and verify
    TEST_ASSERT_TRUE(mgr->releasePin(53));
    TEST_ASSERT_FALSE(mgr->isPinClaimed(53));
    TEST_ASSERT_TRUE(mgr->releasePin(54));
    TEST_ASSERT_FALSE(mgr->isPinClaimed(54));
}

// ===== Test 19: GPIO upper-bound validation rejects invalid values =====
void test_pin_claim_rejects_out_of_range() {
    // GPIO > HAL_GPIO_MAX (54) should be rejected
    TEST_ASSERT_FALSE(mgr->claimPin(55, HAL_BUS_GPIO, 0, 0));
    TEST_ASSERT_FALSE(mgr->claimPin(100, HAL_BUS_GPIO, 0, 0));
    TEST_ASSERT_FALSE(mgr->claimPin(127, HAL_BUS_GPIO, 0, 0));

    // Rejected pins are not tracked
    TEST_ASSERT_FALSE(mgr->isPinClaimed(55));
    TEST_ASSERT_FALSE(mgr->isPinClaimed(100));

    // Negative GPIO still rejected
    TEST_ASSERT_FALSE(mgr->claimPin(-1, HAL_BUS_GPIO, 0, 0));
    TEST_ASSERT_FALSE(mgr->claimPin(-128, HAL_BUS_GPIO, 0, 0));
}

// ===== Test 20: Fill all valid GPIOs (0-54) and verify tracking =====
void test_pin_table_exhaustion() {
    // Claim all 55 valid GPIOs (0-54)
    for (int i = 0; i <= HAL_GPIO_MAX; i++) {
        TEST_ASSERT_TRUE_MESSAGE(
            mgr->claimPin(static_cast<int8_t>(i), HAL_BUS_GPIO, 0, 0),
            "Should claim all valid GPIOs 0-54"
        );
    }

    // Verify all 55 are tracked
    for (int i = 0; i <= HAL_GPIO_MAX; i++) {
        TEST_ASSERT_TRUE(mgr->isPinClaimed(static_cast<int8_t>(i)));
    }

    // Release one and verify reuse
    TEST_ASSERT_TRUE(mgr->releasePin(0));
    TEST_ASSERT_FALSE(mgr->isPinClaimed(0));
    TEST_ASSERT_TRUE(mgr->claimPin(0, HAL_BUS_GPIO, 0, 1));
    TEST_ASSERT_TRUE(mgr->isPinClaimed(0));
}

// ===== Test Runner =====
int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_register_and_get_device);
    RUN_TEST(test_find_by_compatible);
    RUN_TEST(test_find_by_type);
    RUN_TEST(test_remove_device);
    RUN_TEST(test_max_devices_limit);
    RUN_TEST(test_state_transitions);
    RUN_TEST(test_pin_claim_and_release);
    RUN_TEST(test_priority_sorted_init);
    RUN_TEST(test_init_failure_sets_error);
    RUN_TEST(test_health_check_transitions);
    RUN_TEST(test_forEach_iterates_all);
    RUN_TEST(test_registry_find_by_compatible);
    RUN_TEST(test_registry_find_by_legacy_id);
    RUN_TEST(test_registry_rejects_invalid);

    // Scaling tests
    RUN_TEST(test_register_20_devices);
    RUN_TEST(test_register_25_exceeds_max);
    RUN_TEST(test_remove_and_reregister_reclaims_slot);

    // Pin tracking — high GPIO, bounds, exhaustion
    RUN_TEST(test_pin_claim_high_gpio);
    RUN_TEST(test_pin_claim_rejects_out_of_range);
    RUN_TEST(test_pin_table_exhaustion);

    return UNITY_END();
}
