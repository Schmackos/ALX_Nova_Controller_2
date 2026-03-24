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
    bool        probeResult;
    bool        initResult;
    bool        healthResult;
    int         initCallCount;
    int         probeCallCount;
    int         healthCallCount;
    int         deinitCallCount;
    bool        dumpConfigCalled;
    const char* initReason;  // Custom failure reason (nullptr = default "test fail")

    TestDevice()
        : probeResult(true), initResult(true), healthResult(true),
          initCallCount(0), probeCallCount(0), healthCallCount(0),
          deinitCallCount(0), dumpConfigCalled(false), initReason(nullptr) {
        strncpy(_descriptor.compatible, "default", 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = HAL_DEV_NONE;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    TestDevice(const char* compatible, HalDeviceType type, uint16_t priority = HAL_PRIORITY_HARDWARE)
        : probeResult(true), initResult(true), healthResult(true),
          initCallCount(0), probeCallCount(0), healthCallCount(0),
          deinitCallCount(0), dumpConfigCalled(false), initReason(nullptr) {
        strncpy(_descriptor.compatible, compatible, 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = type;
        _initPriority = priority;
    }

    bool probe() override { probeCallCount++; return probeResult; }
    HalInitResult init() override {
        initCallCount++;
        if (initResult) return hal_init_ok();
        const char* reason = initReason ? initReason : "test fail";
        return hal_init_fail(DIAG_HAL_INIT_FAILED, reason);
    }
    void deinit() override { deinitCallCount++; }
    void dumpConfig() override { dumpConfigCalled = true; }
    bool healthCheck() override { healthCallCount++; return healthResult; }
};

// ===== Test Fixtures =====
static HalDeviceManager* mgr;

void setUp() {
    ArduinoMock::reset();  // Resets mockMillis to 0 between tests
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

    // Disabled
    dev._state = HAL_STATE_DISABLED;
    TEST_ASSERT_EQUAL(HAL_STATE_DISABLED, dev._state);

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

// ===== Test 16: Register 33 exceeds HAL_MAX_DEVICES (32) — 33rd rejected =====
void test_register_33_exceeds_max() {
    // HAL_MAX_DEVICES is 32 — we can register 32 but the 33rd should fail
    TestDevice devs[33];
    for (int i = 0; i < 33; i++) {
        char name[16];
        snprintf(name, sizeof(name), "scale%d", i);
        devs[i] = TestDevice(name, HAL_DEV_DAC);
    }

    // Register 32 — should all succeed
    for (int i = 0; i < 32; i++) {
        int slot = mgr->registerDevice(&devs[i], HAL_DISC_BUILTIN);
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, slot,
            "Devices 0-31 should register successfully");
    }
    TEST_ASSERT_EQUAL(32, mgr->getCount());

    // 33rd registration should fail with -1
    int slot33 = mgr->registerDevice(&devs[32], HAL_DISC_BUILTIN);
    TEST_ASSERT_EQUAL(-1, slot33);
    TEST_ASSERT_EQUAL(32, mgr->getCount());
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

// ===== Test 20: Register null device when slots full — no crash, returns -1 =====
void test_register_null_device_returns_negative_one() {
    // Null device should always return -1, regardless of slot availability
    TEST_ASSERT_EQUAL(-1, mgr->registerDevice(nullptr, HAL_DISC_BUILTIN));
    TEST_ASSERT_EQUAL(0, mgr->getCount());
}

// ===== Test 21: Slot full emits diagnostic and returns -1 =====
void test_register_device_slot_full_returns_negative_one() {
    // Fill all 32 slots
    TestDevice devs[HAL_MAX_DEVICES];
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        char name[16];
        snprintf(name, sizeof(name), "fill%d", i);
        devs[i] = TestDevice(name, HAL_DEV_DAC);
        int slot = mgr->registerDevice(&devs[i], HAL_DISC_BUILTIN);
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, slot, "All 32 slots should register");
    }
    TEST_ASSERT_EQUAL(HAL_MAX_DEVICES, mgr->getCount());

    // Attempt to register one more non-null device — should fail with -1
    // (registerDevice emits DIAG_HAL_SLOT_FULL internally when device != nullptr)
    TestDevice overflow("overflow", HAL_DEV_CODEC);
    int result = mgr->registerDevice(&overflow, HAL_DISC_BUILTIN);
    TEST_ASSERT_EQUAL(-1, result);
    TEST_ASSERT_EQUAL(HAL_MAX_DEVICES, mgr->getCount());

    // The overflow device should NOT have its slot set
    // (it was rejected, so it should still have default values)
    TEST_ASSERT_EQUAL(HAL_STATE_UNKNOWN, overflow._state);
    TEST_ASSERT_FALSE(overflow._ready);
}

// ===== Test 22: hal_registry_max() returns HAL_MAX_DRIVERS =====
void test_registry_max_returns_correct_value() {
    TEST_ASSERT_EQUAL(HAL_MAX_DRIVERS, hal_registry_max());
    TEST_ASSERT_EQUAL(48, hal_registry_max());
}

// ===== Test 23: Registry overflow — fill to HAL_MAX_DRIVERS, 33rd rejected =====
void test_registry_overflow_at_max_drivers() {
    // Fill the registry to its maximum capacity (32)
    for (int i = 0; i < HAL_MAX_DRIVERS; i++) {
        HalDriverEntry entry;
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.compatible, 31, "test,driver%d", i);
        entry.type = HAL_DEV_DAC;
        entry.legacyId = static_cast<uint16_t>(i + 1);
        TEST_ASSERT_TRUE_MESSAGE(hal_registry_register(entry),
            "Should register up to HAL_MAX_DRIVERS entries");
    }
    TEST_ASSERT_EQUAL(HAL_MAX_DRIVERS, hal_registry_count());

    // The next registration should fail (registry full)
    // (hal_registry_register emits DIAG_HAL_REGISTRY_FULL internally)
    HalDriverEntry overflow;
    memset(&overflow, 0, sizeof(overflow));
    strncpy(overflow.compatible, "test,overflow", 31);
    overflow.type = HAL_DEV_CODEC;
    TEST_ASSERT_FALSE(hal_registry_register(overflow));

    // Count should remain at max
    TEST_ASSERT_EQUAL(HAL_MAX_DRIVERS, hal_registry_count());

    // Previously registered entries should still be findable
    TEST_ASSERT_NOT_NULL(hal_registry_find("test,driver0"));
    TEST_ASSERT_NOT_NULL(hal_registry_find("test,driver31"));

    // The overflow entry should NOT be findable
    TEST_ASSERT_NULL(hal_registry_find("test,overflow"));
}

// ===== Test 24: Remove device from full manager, then register succeeds =====
void test_slot_full_then_remove_allows_new_registration() {
    // Fill all 32 slots
    TestDevice devs[HAL_MAX_DEVICES];
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        char name[16];
        snprintf(name, sizeof(name), "full%d", i);
        devs[i] = TestDevice(name, HAL_DEV_DAC);
        mgr->registerDevice(&devs[i], HAL_DISC_BUILTIN);
    }
    TEST_ASSERT_EQUAL(HAL_MAX_DEVICES, mgr->getCount());

    // Overflow attempt fails
    TestDevice extra("extra", HAL_DEV_ADC);
    TEST_ASSERT_EQUAL(-1, mgr->registerDevice(&extra, HAL_DISC_BUILTIN));

    // Remove one device to free a slot
    mgr->removeDevice(10);
    TEST_ASSERT_EQUAL(HAL_MAX_DEVICES - 1, mgr->getCount());

    // Now registration should succeed again
    TestDevice replacement("replacement", HAL_DEV_CODEC);
    int slot = mgr->registerDevice(&replacement, HAL_DISC_BUILTIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);
    TEST_ASSERT_EQUAL(HAL_MAX_DEVICES, mgr->getCount());
    TEST_ASSERT_EQUAL_PTR(&replacement, mgr->getDevice(slot));
}

// ===== Test 25: Fill all valid GPIOs (0-54) and verify tracking =====
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

// ===== Test 26: initAll failure stores reason on device =====
void test_init_failure_stores_reason_on_device() {
    TestDevice dev("bad,dev", HAL_DEV_DAC);
    dev.initResult = false;
    dev.initReason = "register write timed out";

    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);
    mgr->initAll();

    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, dev._state);
    TEST_ASSERT_EQUAL_STRING("register write timed out", dev.getLastError());
}

// ===== Test 27: initAll success clears last error =====
void test_init_success_clears_last_error() {
    TestDevice dev("good,dev", HAL_DEV_DAC);

    // Pre-set a stale error to verify it is cleared on successful init
    dev.setLastError("stale error from previous attempt");

    dev.initResult = true;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);
    mgr->initAll();

    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev._state);
    TEST_ASSERT_TRUE(dev._ready);
    TEST_ASSERT_EQUAL_CHAR('\0', dev.getLastError()[0]);
}

// ===== Test 28: probe failure path sets last error (simulates API layer behaviour) =====
void test_probe_failure_sets_last_error() {
    TestDevice dev("probe,fail", HAL_DEV_DAC);
    // Simulate the API path: probe() returns false → build a failure result and store it
    HalInitResult failResult = hal_init_fail(DIAG_HAL_INIT_FAILED, "I2C probe failed (no device response)");
    dev.setLastError(failResult);

    TEST_ASSERT_EQUAL_STRING("I2C probe failed (no device response)", dev.getLastError());
}

// ===== Test 29: Fault count initial zero for all slots =====
void test_fault_count_initial_zero() {
    // After reset, all fault counts should be 0
    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, mgr->getFaultCount(i));
    }
}

// Helper: runs a device through a full fault cycle (health fail -> 3 retries -> ERROR).
// Uses wide time gaps to ensure we always exceed backoff windows.
static void drive_to_error_via_retries(HalDeviceManager* m, TestDevice& dev) {
    dev.healthResult = false;
    dev.initResult = false;

    // Step 1: AVAILABLE -> UNAVAILABLE (health check fail), retryState.nextRetryMs = now+1000
    ArduinoMock::mockMillis += 0;  // Use current time
    m->healthCheckAll();

    // Step 2: Retry 1 — must be past nextRetryMs (now+1000). Use +2000 for safety.
    ArduinoMock::mockMillis += 2000;
    m->healthCheckAll();

    // Step 3: Retry 2 — backoff is 1s<<1=2s from retry 1 time. Use +4000 for safety.
    ArduinoMock::mockMillis += 4000;
    m->healthCheckAll();

    // Step 4: Retry 3 — backoff is 1s<<2=4s from retry 2 time. Use +6000 for safety.
    // This exhausts retries: count becomes 3 >= HAL_MAX_RETRIES -> ERROR + faultCount++
    ArduinoMock::mockMillis += 6000;
    m->healthCheckAll();
}

// ===== Test 30: Fault count increments on retry exhaustion =====
void test_fault_count_increments_on_retry_exhaustion() {
    ArduinoMock::mockMillis = 0;

    TestDevice dev("fault,counter", HAL_DEV_DAC);
    dev.initResult = true;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);

    mgr->initAll();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev._state);
    TEST_ASSERT_EQUAL_UINT8(0, mgr->getFaultCount(static_cast<uint8_t>(slot)));

    drive_to_error_via_retries(mgr, dev);

    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, dev._state);
    TEST_ASSERT_EQUAL_UINT8(1, mgr->getFaultCount(static_cast<uint8_t>(slot)));
}

// ===== Test 31: Fault count getter returns correct value =====
void test_fault_count_getter_returns_correct_value() {
    ArduinoMock::mockMillis = 0;

    TestDevice dev("getter,test", HAL_DEV_ADC);
    dev.initResult = true;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);
    mgr->initAll();
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev._state);
    TEST_ASSERT_EQUAL_UINT8(0, mgr->getFaultCount(static_cast<uint8_t>(slot)));

    drive_to_error_via_retries(mgr, dev);

    TEST_ASSERT_EQUAL_UINT8(1, mgr->getFaultCount(static_cast<uint8_t>(slot)));
}

// ===== Test 32: Fault count bounds check — out-of-range returns 0 =====
void test_fault_count_bounds_check() {
    TEST_ASSERT_EQUAL_UINT8(0, mgr->getFaultCount(HAL_MAX_DEVICES));
    TEST_ASSERT_EQUAL_UINT8(0, mgr->getFaultCount(HAL_MAX_DEVICES + 1));
    TEST_ASSERT_EQUAL_UINT8(0, mgr->getFaultCount(255));
}

// ===== Test 33: Fault count resets on manager reset =====
void test_fault_count_clears_on_reset() {
    ArduinoMock::mockMillis = 0;

    TestDevice dev("reset,fault", HAL_DEV_DAC);
    dev.initResult = true;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);
    mgr->initAll();

    drive_to_error_via_retries(mgr, dev);
    TEST_ASSERT_EQUAL_UINT8(1, mgr->getFaultCount(static_cast<uint8_t>(slot)));

    // Reset the manager — fault count should be cleared
    mgr->reset();
    hal_registry_reset();

    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, mgr->getFaultCount(i));
    }
}

// ===== Test 34: Exhausted retries stay at ERROR — no further increments =====
void test_fault_count_no_further_increment_after_exhaustion() {
    ArduinoMock::mockMillis = 0;

    TestDevice dev("multi,fault", HAL_DEV_CODEC);
    dev.initResult = true;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);
    mgr->initAll();

    drive_to_error_via_retries(mgr, dev);
    TEST_ASSERT_EQUAL_UINT8(1, mgr->getFaultCount(static_cast<uint8_t>(slot)));
    TEST_ASSERT_EQUAL(HAL_STATE_ERROR, dev._state);

    // Additional healthCheckAll calls should NOT increment fault count
    // (device is in ERROR with retries exhausted — healthCheckAll skips it)
    ArduinoMock::mockMillis += 20000;
    mgr->healthCheckAll();
    TEST_ASSERT_EQUAL_UINT8(1, mgr->getFaultCount(static_cast<uint8_t>(slot)));
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
    RUN_TEST(test_register_33_exceeds_max);
    RUN_TEST(test_remove_and_reregister_reclaims_slot);

    // Pin tracking — high GPIO, bounds, exhaustion
    RUN_TEST(test_pin_claim_high_gpio);
    RUN_TEST(test_pin_claim_rejects_out_of_range);
    RUN_TEST(test_pin_table_exhaustion);

    // Capacity exhaustion — diagnostic emission and recovery
    RUN_TEST(test_register_null_device_returns_negative_one);
    RUN_TEST(test_register_device_slot_full_returns_negative_one);
    RUN_TEST(test_registry_max_returns_correct_value);
    RUN_TEST(test_registry_overflow_at_max_drivers);
    RUN_TEST(test_slot_full_then_remove_allows_new_registration);

    // lastError propagation through device manager
    RUN_TEST(test_init_failure_stores_reason_on_device);
    RUN_TEST(test_init_success_clears_last_error);
    RUN_TEST(test_probe_failure_sets_last_error);

    // Fault counter tests
    RUN_TEST(test_fault_count_initial_zero);
    RUN_TEST(test_fault_count_increments_on_retry_exhaustion);
    RUN_TEST(test_fault_count_getter_returns_correct_value);
    RUN_TEST(test_fault_count_bounds_check);
    RUN_TEST(test_fault_count_clears_on_reset);
    RUN_TEST(test_fault_count_no_further_increment_after_exhaustion);

    return UNITY_END();
}
