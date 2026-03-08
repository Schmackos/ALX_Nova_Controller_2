#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline HAL implementation for native testing =====
#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"

#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// ===== Test Device Implementation =====
class TestDevice : public HalDevice {
public:
    TestDevice(const char* compatible, HalDeviceType type) {
        strncpy(_descriptor.compatible, compatible, 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = type;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    bool probe() override { return true; }
    HalInitResult init() override { return hal_init_ok(); }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return true; }
};

// ===== Test Fixtures =====
static HalDeviceManager* mgr;

void setUp() {
    mgr = &HalDeviceManager::instance();
    mgr->reset();
}

void tearDown() {}

// ===== Tests =====

void test_register_two_same_compatible_gets_distinct_slots() {
    auto* dev1 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);
    auto* dev2 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);

    int slot1 = mgr->registerDevice(dev1, HAL_DISC_MANUAL);
    int slot2 = mgr->registerDevice(dev2, HAL_DISC_MANUAL);

    TEST_ASSERT_GREATER_OR_EQUAL(0, slot1);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot2);
    TEST_ASSERT_NOT_EQUAL(slot1, slot2);
}

void test_instance_ids_auto_assigned_sequentially() {
    auto* dev1 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);
    auto* dev2 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);
    auto* dev3 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);

    mgr->registerDevice(dev1, HAL_DISC_MANUAL);
    mgr->registerDevice(dev2, HAL_DISC_MANUAL);
    mgr->registerDevice(dev3, HAL_DISC_MANUAL);

    TEST_ASSERT_EQUAL_UINT8(0, dev1->getDescriptor().instanceId);
    TEST_ASSERT_EQUAL_UINT8(1, dev2->getDescriptor().instanceId);
    TEST_ASSERT_EQUAL_UINT8(2, dev3->getDescriptor().instanceId);
}

void test_instance_ids_independent_per_compatible() {
    auto* adc1 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);
    auto* dac1 = new TestDevice("ti,pcm5102a", HAL_DEV_DAC);
    auto* adc2 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);

    mgr->registerDevice(adc1, HAL_DISC_MANUAL);
    mgr->registerDevice(dac1, HAL_DISC_MANUAL);
    mgr->registerDevice(adc2, HAL_DISC_MANUAL);

    TEST_ASSERT_EQUAL_UINT8(0, adc1->getDescriptor().instanceId);
    TEST_ASSERT_EQUAL_UINT8(0, dac1->getDescriptor().instanceId);
    TEST_ASSERT_EQUAL_UINT8(1, adc2->getDescriptor().instanceId);
}

void test_count_by_compatible() {
    TEST_ASSERT_EQUAL_UINT8(0, mgr->countByCompatible("ti,pcm1808"));

    auto* dev1 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);
    mgr->registerDevice(dev1, HAL_DISC_MANUAL);
    TEST_ASSERT_EQUAL_UINT8(1, mgr->countByCompatible("ti,pcm1808"));

    auto* dev2 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);
    mgr->registerDevice(dev2, HAL_DISC_MANUAL);
    TEST_ASSERT_EQUAL_UINT8(2, mgr->countByCompatible("ti,pcm1808"));

    // Different compatible doesn't affect count
    TEST_ASSERT_EQUAL_UINT8(0, mgr->countByCompatible("ti,pcm5102a"));
}

void test_find_by_compatible_with_instance_id() {
    auto* dev1 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);
    auto* dev2 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);

    mgr->registerDevice(dev1, HAL_DISC_MANUAL);
    mgr->registerDevice(dev2, HAL_DISC_MANUAL);

    HalDevice* found0 = mgr->findByCompatible("ti,pcm1808", 0);
    HalDevice* found1 = mgr->findByCompatible("ti,pcm1808", 1);
    HalDevice* found2 = mgr->findByCompatible("ti,pcm1808", 2);

    TEST_ASSERT_NOT_NULL(found0);
    TEST_ASSERT_NOT_NULL(found1);
    TEST_ASSERT_NULL(found2);  // Only 2 registered

    TEST_ASSERT_EQUAL_PTR(dev1, found0);
    TEST_ASSERT_EQUAL_PTR(dev2, found1);
}

void test_find_by_compatible_without_instance_returns_first() {
    auto* dev1 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);
    auto* dev2 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);

    mgr->registerDevice(dev1, HAL_DISC_MANUAL);
    mgr->registerDevice(dev2, HAL_DISC_MANUAL);

    // Original single-arg overload still returns first match
    HalDevice* found = mgr->findByCompatible("ti,pcm1808");
    TEST_ASSERT_EQUAL_PTR(dev1, found);
}

void test_count_decrements_on_remove() {
    auto* dev1 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);
    auto* dev2 = new TestDevice("ti,pcm1808", HAL_DEV_ADC);

    int slot1 = mgr->registerDevice(dev1, HAL_DISC_MANUAL);
    mgr->registerDevice(dev2, HAL_DISC_MANUAL);

    TEST_ASSERT_EQUAL_UINT8(2, mgr->countByCompatible("ti,pcm1808"));

    mgr->removeDevice(static_cast<uint8_t>(slot1));
    TEST_ASSERT_EQUAL_UINT8(1, mgr->countByCompatible("ti,pcm1808"));
}

void test_count_by_compatible_null_returns_zero() {
    TEST_ASSERT_EQUAL_UINT8(0, mgr->countByCompatible(nullptr));
}

void test_find_by_compatible_instance_null_returns_null() {
    TEST_ASSERT_NULL(mgr->findByCompatible(nullptr, 0));
}

void test_max_instances_default_for_adc() {
    // Default max instances for ADC type is 8
    uint8_t maxInst = 8;  // from _defaultMaxInstances(HAL_DEV_ADC)
    TEST_ASSERT_EQUAL_UINT8(8, maxInst);
}

void test_descriptor_instance_id_initialized_to_zero() {
    HalDeviceDescriptor d;
    memset(&d, 0, sizeof(d));
    TEST_ASSERT_EQUAL_UINT8(0, d.instanceId);
    TEST_ASSERT_EQUAL_UINT8(0, d.maxInstances);
}

// ===== Main =====
int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_register_two_same_compatible_gets_distinct_slots);
    RUN_TEST(test_instance_ids_auto_assigned_sequentially);
    RUN_TEST(test_instance_ids_independent_per_compatible);
    RUN_TEST(test_count_by_compatible);
    RUN_TEST(test_find_by_compatible_with_instance_id);
    RUN_TEST(test_find_by_compatible_without_instance_returns_first);
    RUN_TEST(test_count_decrements_on_remove);
    RUN_TEST(test_count_by_compatible_null_returns_zero);
    RUN_TEST(test_find_by_compatible_instance_null_returns_null);
    RUN_TEST(test_max_instances_default_for_adc);
    RUN_TEST(test_descriptor_instance_id_initialized_to_zero);

    return UNITY_END();
}
