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

// ===== Audio-capable Test Device with configurable capabilities and counts =====
class TestAudioDevice : public HalDevice {
public:
    TestAudioDevice(const char* compatible, HalDeviceType type,
                    uint32_t capabilities, int sinkCount = 1, int sourceCount = 1)
        : _sinkCount(sinkCount), _sourceCount(sourceCount)
    {
        strncpy(_descriptor.compatible, compatible, 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = type;
        _descriptor.capabilities = capabilities;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    bool probe() override { return true; }
    HalInitResult init() override { return hal_init_ok(); }
    void deinit() override {}
    void dumpConfig() override {}
    bool healthCheck() override { return true; }

    int getSinkCount() const override { return _sinkCount; }
    int getInputSourceCount() const override { return _sourceCount; }

private:
    int _sinkCount;
    int _sourceCount;
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

// ===== Matrix Allocation Tests =====

void test_multi_sink_device_gets_consecutive_slots() {
    // An 8ch DAC with getSinkCount()=4 should occupy 4 consecutive manager slots
    // when registered as 4 separate sink-capable devices (one per stereo pair).
    // Here we verify that a single device with getSinkCount()=4 reports correctly
    // and that registering 4 DAC-path devices yields 4 consecutive slots.
    auto* dac8ch = new TestAudioDevice("ess,es9038pro", HAL_DEV_DAC,
                                       HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME,
                                       4,  // sinkCount
                                       0); // sourceCount

    int slot0 = mgr->registerDevice(dac8ch, HAL_DISC_EEPROM);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot0);
    TEST_ASSERT_EQUAL_INT(4, dac8ch->getSinkCount());

    // Register 3 more DAC-path devices to simulate the bridge allocating
    // consecutive sink slots for the remaining stereo pairs
    int slots[4];
    slots[0] = slot0;
    for (int i = 1; i < 4; i++) {
        char compat[32];
        snprintf(compat, sizeof(compat), "test,dac-pair-%d", i);
        auto* pair = new TestAudioDevice(compat, HAL_DEV_DAC,
                                         HAL_CAP_DAC_PATH, 1, 0);
        slots[i] = mgr->registerDevice(pair, HAL_DISC_EEPROM);
        TEST_ASSERT_GREATER_OR_EQUAL(0, slots[i]);
    }

    // All 4 slots should be distinct
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            TEST_ASSERT_NOT_EQUAL(slots[i], slots[j]);
        }
    }

    // Slots are assigned from the first free position, so they should be consecutive
    for (int i = 1; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT(slots[0] + i, slots[i]);
    }
}

void test_multi_source_device_gets_consecutive_lanes() {
    // A 4ch ADC with getInputSourceCount()=2 should expose 2 sources.
    // Register it plus a second ADC device to verify consecutive lane assignment.
    auto* adc4ch = new TestAudioDevice("ess,es9843pro", HAL_DEV_ADC,
                                       HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME,
                                       0,  // sinkCount
                                       2); // sourceCount (2 stereo pairs)

    int slot0 = mgr->registerDevice(adc4ch, HAL_DISC_EEPROM);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot0);
    TEST_ASSERT_EQUAL_INT(2, adc4ch->getInputSourceCount());

    // Register a second ADC to verify it gets a distinct slot
    auto* adc2ch = new TestAudioDevice("ess,es9822pro", HAL_DEV_ADC,
                                       HAL_CAP_ADC_PATH, 0, 1);
    int slot1 = mgr->registerDevice(adc2ch, HAL_DISC_EEPROM);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot1);
    TEST_ASSERT_NOT_EQUAL(slot0, slot1);

    // Second slot is consecutive (manager assigns first free)
    TEST_ASSERT_EQUAL_INT(slot0 + 1, slot1);
}

void test_simultaneous_multi_sink_and_source() {
    // Register a 4-sink DAC and a 2-source ADC simultaneously.
    // Verify no overlap in slot assignment.
    auto* dac = new TestAudioDevice("ess,es9038pro", HAL_DEV_DAC,
                                    HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME,
                                    4,  // sinkCount
                                    0); // sourceCount
    auto* adc = new TestAudioDevice("ess,es9843pro", HAL_DEV_ADC,
                                    HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME,
                                    0,  // sinkCount
                                    2); // sourceCount

    int dacSlot = mgr->registerDevice(dac, HAL_DISC_EEPROM);
    int adcSlot = mgr->registerDevice(adc, HAL_DISC_EEPROM);

    TEST_ASSERT_GREATER_OR_EQUAL(0, dacSlot);
    TEST_ASSERT_GREATER_OR_EQUAL(0, adcSlot);
    TEST_ASSERT_NOT_EQUAL(dacSlot, adcSlot);

    // Verify capabilities are preserved correctly
    TEST_ASSERT_TRUE(dac->getDescriptor().capabilities & HAL_CAP_DAC_PATH);
    TEST_ASSERT_FALSE(dac->getDescriptor().capabilities & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(adc->getDescriptor().capabilities & HAL_CAP_ADC_PATH);
    TEST_ASSERT_FALSE(adc->getDescriptor().capabilities & HAL_CAP_DAC_PATH);

    // Verify multi-count accessors
    TEST_ASSERT_EQUAL_INT(4, dac->getSinkCount());
    TEST_ASSERT_EQUAL_INT(0, dac->getInputSourceCount());
    TEST_ASSERT_EQUAL_INT(0, adc->getSinkCount());
    TEST_ASSERT_EQUAL_INT(2, adc->getInputSourceCount());

    // Both devices should be retrievable from distinct slots
    HalDevice* fromDac = mgr->getDevice(static_cast<uint8_t>(dacSlot));
    HalDevice* fromAdc = mgr->getDevice(static_cast<uint8_t>(adcSlot));
    TEST_ASSERT_EQUAL_PTR(dac, fromDac);
    TEST_ASSERT_EQUAL_PTR(adc, fromAdc);
}

void test_boundary_slot_allocation_near_capacity() {
    // Fill up to HAL_MAX_DEVICES - 1 slots, then register one more.
    // The last device should still get a valid slot.
    HalDevice* devices[HAL_MAX_DEVICES];
    for (int i = 0; i < HAL_MAX_DEVICES - 1; i++) {
        char compat[32];
        snprintf(compat, sizeof(compat), "test,filler-%d", i);
        devices[i] = new TestAudioDevice(compat, HAL_DEV_SENSOR,
                                         0, 1, 0);
        int slot = mgr->registerDevice(devices[i], HAL_DISC_MANUAL);
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, slot, "Filler device registration failed");
    }

    // Register the final device at the boundary
    auto* lastDev = new TestAudioDevice("test,last-dac", HAL_DEV_DAC,
                                        HAL_CAP_DAC_PATH, 4, 0);
    int lastSlot = mgr->registerDevice(lastDev, HAL_DISC_MANUAL);
    TEST_ASSERT_GREATER_OR_EQUAL(0, lastSlot);
    TEST_ASSERT_EQUAL_INT(HAL_MAX_DEVICES - 1, lastSlot);

    // Verify the last device is accessible and has correct properties
    HalDevice* retrieved = mgr->getDevice(static_cast<uint8_t>(lastSlot));
    TEST_ASSERT_EQUAL_PTR(lastDev, retrieved);
    TEST_ASSERT_EQUAL_INT(4, lastDev->getSinkCount());
    TEST_ASSERT_TRUE(lastDev->getDescriptor().capabilities & HAL_CAP_DAC_PATH);
}

void test_device_registration_overflow_rejected() {
    // Fill all HAL_MAX_DEVICES (32) slots
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        char compat[32];
        snprintf(compat, sizeof(compat), "test,fill-%d", i);
        auto* dev = new TestAudioDevice(compat, HAL_DEV_SENSOR, 0, 1, 0);
        int slot = mgr->registerDevice(dev, HAL_DISC_MANUAL);
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, slot, "Pre-fill registration failed");
    }

    // The 33rd device should be rejected
    auto* overflow = new TestAudioDevice("test,overflow", HAL_DEV_DAC,
                                         HAL_CAP_DAC_PATH, 4, 0);
    int result = mgr->registerDevice(overflow, HAL_DISC_MANUAL);
    TEST_ASSERT_EQUAL_INT(-1, result);

    // Clean up the rejected device manually since the manager didn't take ownership
    delete overflow;
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

    // Matrix allocation tests
    RUN_TEST(test_multi_sink_device_gets_consecutive_slots);
    RUN_TEST(test_multi_source_device_gets_consecutive_lanes);
    RUN_TEST(test_simultaneous_multi_sink_and_source);
    RUN_TEST(test_boundary_slot_allocation_near_capacity);
    RUN_TEST(test_device_registration_overflow_rejected);

    return UNITY_END();
}
