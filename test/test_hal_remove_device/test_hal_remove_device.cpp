/**
 * test_hal_remove_device.cpp
 * Tests for HalDeviceManager::removeDevice() ownership tracking and memory safety.
 */
#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"

#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// ===== Test Device =====
class TestDevice : public HalDevice {
public:
    int deinitCallCount;

    TestDevice(const char* compatible = "test,device")
        : deinitCallCount(0)
    {
        strncpy(_descriptor.compatible, compatible, 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = HAL_DEV_DAC;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    bool probe() override { return true; }
    HalInitResult init() override { return hal_init_ok(); }
    void deinit() override { deinitCallCount++; }
    void dumpConfig() override {}
    bool healthCheck() override { return true; }
};

// Tracks destructor calls via static flag
static bool gDestructorFired = false;

class TestOwnedDevice : public HalDevice {
public:
    int deinitCallCount;

    TestOwnedDevice(const char* compatible = "test,owned")
        : deinitCallCount(0)
    {
        strncpy(_descriptor.compatible, compatible, 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = HAL_DEV_DAC;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    ~TestOwnedDevice() { gDestructorFired = true; }

    bool probe() override { return true; }
    HalInitResult init() override { return hal_init_ok(); }
    void deinit() override { deinitCallCount++; }
    void dumpConfig() override {}
    bool healthCheck() override { return true; }
};

// ===== Fixtures =====
static HalDeviceManager* mgr;

void setUp() {
    ArduinoMock::reset();
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
    gDestructorFired = false;
}

void tearDown() {}

// ===== Test 1: removeDevice nulls the slot and decrements count =====
void test_removeDevice_nulls_slot() {
    TestDevice dev;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);
    TEST_ASSERT_EQUAL(1, mgr->getCount());

    bool ok = mgr->removeDevice(static_cast<uint8_t>(slot));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_NULL(mgr->getDevice(static_cast<uint8_t>(slot)));
    TEST_ASSERT_EQUAL(0, mgr->getCount());
}

// ===== Test 2: removeDevice calls deinit() on the device =====
void test_removeDevice_calls_deinit() {
    TestDevice dev;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);
    TEST_ASSERT_EQUAL(0, dev.deinitCallCount);

    mgr->removeDevice(static_cast<uint8_t>(slot));
    TEST_ASSERT_EQUAL(1, dev.deinitCallCount);
}

// ===== Test 3: removeDevice deletes heap device when takeOwnership=true =====
void test_removeDevice_deletes_owned_device() {
    TestOwnedDevice* dev = new TestOwnedDevice("test,owned-del");
    int slot = mgr->registerDevice(dev, HAL_DISC_MANUAL, true);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);
    TEST_ASSERT_FALSE(gDestructorFired);

    mgr->removeDevice(static_cast<uint8_t>(slot));
    TEST_ASSERT_TRUE(gDestructorFired);
}

// ===== Test 4: removeDevice does NOT delete unowned device =====
void test_removeDevice_skips_delete_for_unowned() {
    TestDevice dev("test,unowned");
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN, false);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);

    // Should not crash (dev is on stack)
    bool ok = mgr->removeDevice(static_cast<uint8_t>(slot));
    TEST_ASSERT_TRUE(ok);

    // Object is still accessible (stack allocated, not deleted)
    TEST_ASSERT_EQUAL(1, dev.deinitCallCount);
}

// ===== Test 5: _ownsDevice flag cleared after remove — second registration safe =====
void test_removeDevice_resets_ownership_flag() {
    // First: owned heap device
    TestOwnedDevice* owned = new TestOwnedDevice("test,seq-owned");
    int slot = mgr->registerDevice(owned, HAL_DISC_MANUAL, true);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);
    mgr->removeDevice(static_cast<uint8_t>(slot));
    TEST_ASSERT_TRUE(gDestructorFired);

    // Second: unowned stack device in same slot (or nearby)
    TestDevice stack_dev("test,seq-stack");
    int slot2 = mgr->registerDevice(&stack_dev, HAL_DISC_BUILTIN, false);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot2);

    // Should not attempt to delete stack_dev — no crash
    gDestructorFired = false;
    mgr->removeDevice(static_cast<uint8_t>(slot2));
    // gDestructorFired should remain false since stack_dev has no destructor side-effect
    // (TestDevice destructor not tracked, but TestOwnedDevice is gone already)
    TEST_ASSERT_FALSE(gDestructorFired);
    TEST_ASSERT_EQUAL(0, mgr->getCount());
}

// ===== Test 6: reset() deletes all owned devices =====
void test_reset_deletes_owned_devices() {
    TestOwnedDevice* dev = new TestOwnedDevice("test,reset-owned");
    int slot = mgr->registerDevice(dev, HAL_DISC_MANUAL, true);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);
    TEST_ASSERT_FALSE(gDestructorFired);

    mgr->reset();
    TEST_ASSERT_TRUE(gDestructorFired);
    TEST_ASSERT_EQUAL(0, mgr->getCount());
}

// ===== Main =====
int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_removeDevice_nulls_slot);
    RUN_TEST(test_removeDevice_calls_deinit);
    RUN_TEST(test_removeDevice_deletes_owned_device);
    RUN_TEST(test_removeDevice_skips_delete_for_unowned);
    RUN_TEST(test_removeDevice_resets_ownership_flag);
    RUN_TEST(test_reset_deletes_owned_devices);
    return UNITY_END();
}
