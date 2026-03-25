/**
 * test_device_deps.cpp
 *
 * Unit tests for the device dependency graph feature:
 *
 *   1. Bitfield operations: addDependency, hasDependency, getDependencies
 *   2. Boundary tests: slot 0, slot 31, invalid slot (32+)
 *   3. Topological sort correctness: no deps, linear chain, diamond, priority tiebreaker
 *   4. Cycle detection: A→B→A cycle falls back to priority ordering
 *   5. Mixed dependencies + priorities
 *   6. Empty and single device edge cases
 *   7. initAll integration: verify init order via call sequence counters
 *
 * Runs on the native platform (no hardware needed).
 */

#include <unity.h>
#include <cstring>
#include <cstdint>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_init_result.h"
#include "../../src/diag_error_codes.h"

// Inline the manager .cpp files for native testing (same pattern as test_hal_core)
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// ============================================================
// Test Device with init order tracking
// ============================================================

static int g_initSequence = 0;  // Global counter incremented on each init call

class DepTestDevice : public HalDevice {
public:
    bool     probeResult;
    bool     initResult;
    int      initOrder;       // Captured init sequence number
    int      initCallCount;
    int      probeCallCount;

    DepTestDevice()
        : probeResult(true), initResult(true), initOrder(-1),
          initCallCount(0), probeCallCount(0) {
        strncpy(_descriptor.compatible, "test,dep-device", 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = HAL_DEV_DAC;
        _initPriority = HAL_PRIORITY_HARDWARE;
    }

    DepTestDevice(const char* compatible, HalDeviceType type, uint16_t priority = HAL_PRIORITY_HARDWARE)
        : probeResult(true), initResult(true), initOrder(-1),
          initCallCount(0), probeCallCount(0) {
        strncpy(_descriptor.compatible, compatible, 31);
        _descriptor.compatible[31] = '\0';
        _descriptor.type = type;
        _initPriority = priority;
    }

    bool probe() override { probeCallCount++; return probeResult; }
    HalInitResult init() override {
        initCallCount++;
        initOrder = g_initSequence++;
        if (initResult) return hal_init_ok();
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "test fail");
    }
    void deinit() override {}
    void dumpConfig() override {}
    void setCompatible(const char* name) { strncpy(_descriptor.compatible, name, 31); _descriptor.compatible[31] = '\0'; }
    bool healthCheck() override { return true; }
};

// ============================================================
// Fixtures
// ============================================================

static HalDeviceManager* mgr;

void setUp() {
    ArduinoMock::reset();
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
    g_initSequence = 0;
}

void tearDown() {}

// ============================================================
// Section 1: Bitfield operations
// ============================================================

void test_add_dependency_sets_correct_bit() {
    DepTestDevice dev;
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);
    TEST_ASSERT_GREATER_OR_EQUAL(0, slot);

    dev.addDependency(5);
    TEST_ASSERT_TRUE(dev.hasDependency(5));
    TEST_ASSERT_EQUAL_UINT32((1UL << 5), dev.getDependencies());
}

void test_has_dependency_returns_false_for_unset_bits() {
    DepTestDevice dev;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    dev.addDependency(3);
    TEST_ASSERT_TRUE(dev.hasDependency(3));
    TEST_ASSERT_FALSE(dev.hasDependency(0));
    TEST_ASSERT_FALSE(dev.hasDependency(4));
    TEST_ASSERT_FALSE(dev.hasDependency(31));
}

void test_multiple_dependencies() {
    DepTestDevice dev;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    dev.addDependency(1);
    dev.addDependency(7);
    dev.addDependency(15);

    TEST_ASSERT_TRUE(dev.hasDependency(1));
    TEST_ASSERT_TRUE(dev.hasDependency(7));
    TEST_ASSERT_TRUE(dev.hasDependency(15));
    TEST_ASSERT_FALSE(dev.hasDependency(0));
    TEST_ASSERT_FALSE(dev.hasDependency(8));

    uint32_t expected = (1UL << 1) | (1UL << 7) | (1UL << 15);
    TEST_ASSERT_EQUAL_UINT32(expected, dev.getDependencies());
}

void test_get_dependencies_returns_full_bitfield() {
    DepTestDevice dev;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // No dependencies set
    TEST_ASSERT_EQUAL_UINT32(0, dev.getDependencies());

    dev.addDependency(0);
    dev.addDependency(31);
    uint32_t expected = (1UL << 0) | (1UL << 31);
    TEST_ASSERT_EQUAL_UINT32(expected, dev.getDependencies());
}

void test_add_same_dependency_twice_is_idempotent() {
    DepTestDevice dev;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    dev.addDependency(10);
    uint32_t after_first = dev.getDependencies();
    dev.addDependency(10);
    uint32_t after_second = dev.getDependencies();

    TEST_ASSERT_EQUAL_UINT32(after_first, after_second);
}

// ============================================================
// Section 2: Boundary tests
// ============================================================

void test_dependency_slot_zero_lsb() {
    DepTestDevice dev;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    dev.addDependency(0);
    TEST_ASSERT_TRUE(dev.hasDependency(0));
    TEST_ASSERT_EQUAL_UINT32(1UL, dev.getDependencies());
}

void test_dependency_slot_31_msb() {
    DepTestDevice dev;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    dev.addDependency(31);
    TEST_ASSERT_TRUE(dev.hasDependency(31));
    TEST_ASSERT_EQUAL_UINT32((1UL << 31), dev.getDependencies());
}

void test_invalid_slot_32_rejected() {
    DepTestDevice dev;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    dev.addDependency(32);
    // Should not set any bit — 32 is out of range for 32-bit bitfield
    TEST_ASSERT_EQUAL_UINT32(0, dev.getDependencies());
    TEST_ASSERT_FALSE(dev.hasDependency(32));
}

void test_invalid_slot_255_rejected() {
    DepTestDevice dev;
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    dev.addDependency(255);
    TEST_ASSERT_EQUAL_UINT32(0, dev.getDependencies());
}

// ============================================================
// Section 3: Topological sort correctness
// ============================================================

void test_no_dependencies_uses_priority_ordering() {
    // Three devices with different priorities, no deps
    DepTestDevice devHigh("test,high", HAL_DEV_DAC, HAL_PRIORITY_BUS);      // 1000
    DepTestDevice devMid("test,mid", HAL_DEV_ADC, HAL_PRIORITY_HARDWARE);   // 800
    DepTestDevice devLow("test,low", HAL_DEV_SENSOR, HAL_PRIORITY_LATE);    // 100

    mgr->registerDevice(&devHigh, HAL_DISC_BUILTIN);
    mgr->registerDevice(&devMid, HAL_DISC_BUILTIN);
    mgr->registerDevice(&devLow, HAL_DISC_BUILTIN);

    mgr->initAll();

    // Higher priority should init first (lower initOrder number)
    TEST_ASSERT_LESS_THAN(devMid.initOrder, devHigh.initOrder);
    TEST_ASSERT_LESS_THAN(devLow.initOrder, devMid.initOrder);
}

void test_linear_chain_a_depends_b_depends_c() {
    // C must init before B, B must init before A
    DepTestDevice devA("test,a", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);
    DepTestDevice devB("test,b", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);
    DepTestDevice devC("test,c", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);

    int slotA = mgr->registerDevice(&devA, HAL_DISC_BUILTIN);
    int slotB = mgr->registerDevice(&devB, HAL_DISC_BUILTIN);
    int slotC = mgr->registerDevice(&devC, HAL_DISC_BUILTIN);

    // A depends on B, B depends on C
    devA.addDependency(slotB);
    devB.addDependency(slotC);

    mgr->initAll();

    // C first, then B, then A
    TEST_ASSERT_LESS_THAN(devB.initOrder, devC.initOrder);
    TEST_ASSERT_LESS_THAN(devA.initOrder, devB.initOrder);
}

void test_diamond_dependency() {
    //     A
    //    / \
    //   B   C
    //    \ /
    //     D
    // D must init before B and C; B and C before A
    DepTestDevice devA("test,a", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);
    DepTestDevice devB("test,b", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);
    DepTestDevice devC("test,c", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);
    DepTestDevice devD("test,d", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);

    int slotA = mgr->registerDevice(&devA, HAL_DISC_BUILTIN);
    int slotB = mgr->registerDevice(&devB, HAL_DISC_BUILTIN);
    int slotC = mgr->registerDevice(&devC, HAL_DISC_BUILTIN);
    int slotD = mgr->registerDevice(&devD, HAL_DISC_BUILTIN);

    devA.addDependency(slotB);
    devA.addDependency(slotC);
    devB.addDependency(slotD);
    devC.addDependency(slotD);

    mgr->initAll();

    // D must init before B and C
    TEST_ASSERT_LESS_THAN(devB.initOrder, devD.initOrder);
    TEST_ASSERT_LESS_THAN(devC.initOrder, devD.initOrder);
    // B and C must init before A
    TEST_ASSERT_LESS_THAN(devA.initOrder, devB.initOrder);
    TEST_ASSERT_LESS_THAN(devA.initOrder, devC.initOrder);
}

void test_priority_tiebreaker_within_same_topo_level() {
    // B and C both depend on D (same topo level), but B has higher priority
    DepTestDevice devB("test,b", HAL_DEV_DAC, HAL_PRIORITY_BUS);       // 1000
    DepTestDevice devC("test,c", HAL_DEV_DAC, HAL_PRIORITY_LATE);      // 100
    DepTestDevice devD("test,d", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);  // 800

    mgr->registerDevice(&devB, HAL_DISC_BUILTIN);
    mgr->registerDevice(&devC, HAL_DISC_BUILTIN);
    int slotD = mgr->registerDevice(&devD, HAL_DISC_BUILTIN);

    devB.addDependency(slotD);
    devC.addDependency(slotD);

    mgr->initAll();

    // D must init first (both B and C depend on it)
    TEST_ASSERT_LESS_THAN(devB.initOrder, devD.initOrder);
    TEST_ASSERT_LESS_THAN(devC.initOrder, devD.initOrder);
    // B has higher priority than C, so should init before C (tiebreaker)
    TEST_ASSERT_LESS_THAN(devC.initOrder, devB.initOrder);
}

// ============================================================
// Section 4: Cycle detection
// ============================================================

void test_cycle_a_depends_b_depends_a_falls_back_to_priority() {
    // A→B and B→A is a cycle. Should be detected and fall back to priority order.
    DepTestDevice devA("test,a", HAL_DEV_DAC, HAL_PRIORITY_BUS);       // 1000, higher prio
    DepTestDevice devB("test,b", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);  // 800

    int slotA = mgr->registerDevice(&devA, HAL_DISC_BUILTIN);
    int slotB = mgr->registerDevice(&devB, HAL_DISC_BUILTIN);

    devA.addDependency(slotB);
    devB.addDependency(slotA);

    mgr->initAll();

    // Both should still be initialized (not stuck/crashed)
    TEST_ASSERT_GREATER_OR_EQUAL(0, devA.initOrder);
    TEST_ASSERT_GREATER_OR_EQUAL(0, devB.initOrder);
    TEST_ASSERT_EQUAL(1, devA.initCallCount);
    TEST_ASSERT_EQUAL(1, devB.initCallCount);
    // With cycle fallback, should use priority: A (1000) before B (800)
    TEST_ASSERT_LESS_THAN(devB.initOrder, devA.initOrder);
}

void test_three_node_cycle() {
    // A→B→C→A cycle
    DepTestDevice devA("test,a", HAL_DEV_DAC, HAL_PRIORITY_BUS);       // 1000
    DepTestDevice devB("test,b", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);  // 800
    DepTestDevice devC("test,c", HAL_DEV_DAC, HAL_PRIORITY_LATE);      // 100

    int slotA = mgr->registerDevice(&devA, HAL_DISC_BUILTIN);
    int slotB = mgr->registerDevice(&devB, HAL_DISC_BUILTIN);
    int slotC = mgr->registerDevice(&devC, HAL_DISC_BUILTIN);

    devA.addDependency(slotB);
    devB.addDependency(slotC);
    devC.addDependency(slotA);

    mgr->initAll();

    // All should be initialized despite cycle
    TEST_ASSERT_EQUAL(1, devA.initCallCount);
    TEST_ASSERT_EQUAL(1, devB.initCallCount);
    TEST_ASSERT_EQUAL(1, devC.initCallCount);
}

// ============================================================
// Section 5: Mixed dependencies + priorities
// ============================================================

void test_deps_override_priority_when_needed() {
    // devHigh has highest priority but depends on devLow
    DepTestDevice devHigh("test,high", HAL_DEV_DAC, HAL_PRIORITY_BUS);    // 1000
    DepTestDevice devLow("test,low", HAL_DEV_DAC, HAL_PRIORITY_LATE);     // 100

    mgr->registerDevice(&devHigh, HAL_DISC_BUILTIN);
    int slotLow = mgr->registerDevice(&devLow, HAL_DISC_BUILTIN);

    devHigh.addDependency(slotLow);

    mgr->initAll();

    // Despite higher priority, devHigh must wait for devLow
    TEST_ASSERT_LESS_THAN(devHigh.initOrder, devLow.initOrder);
}

void test_independent_devices_still_sort_by_priority() {
    // devA has deps, devB and devC are independent — devB and devC still use priority
    DepTestDevice devA("test,a", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);  // 800
    DepTestDevice devB("test,b", HAL_DEV_DAC, HAL_PRIORITY_BUS);      // 1000
    DepTestDevice devC("test,c", HAL_DEV_DAC, HAL_PRIORITY_LATE);     // 100
    DepTestDevice devD("test,d", HAL_DEV_DAC, HAL_PRIORITY_DATA);     // 600

    mgr->registerDevice(&devA, HAL_DISC_BUILTIN);
    mgr->registerDevice(&devB, HAL_DISC_BUILTIN);
    mgr->registerDevice(&devC, HAL_DISC_BUILTIN);
    int slotD = mgr->registerDevice(&devD, HAL_DISC_BUILTIN);

    devA.addDependency(slotD);  // A depends on D

    mgr->initAll();

    // D must init before A
    TEST_ASSERT_LESS_THAN(devA.initOrder, devD.initOrder);
    // B (1000) is independent — should init according to priority
    // (may be before or after D depending on topo ordering, but should be before C)
    TEST_ASSERT_LESS_THAN(devC.initOrder, devB.initOrder);
}

// ============================================================
// Section 6: Edge cases — empty and single device
// ============================================================

void test_empty_device_list_no_crash() {
    // No devices registered — initAll should not crash
    mgr->initAll();
    TEST_ASSERT_EQUAL(0, mgr->getCount());
}

void test_single_device_no_deps() {
    DepTestDevice dev("test,single", HAL_DEV_DAC);
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    mgr->initAll();

    TEST_ASSERT_EQUAL(1, dev.initCallCount);
    TEST_ASSERT_EQUAL(0, dev.initOrder);
}

void test_single_device_self_dependency_handled() {
    DepTestDevice dev("test,self", HAL_DEV_DAC);
    int slot = mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Self-dependency is technically a cycle
    dev.addDependency(slot);

    mgr->initAll();

    // Should still initialize (self-dep is a degenerate cycle)
    TEST_ASSERT_EQUAL(1, dev.initCallCount);
}

void test_dependency_on_nonexistent_slot() {
    DepTestDevice dev("test,orphan", HAL_DEV_DAC);
    mgr->registerDevice(&dev, HAL_DISC_BUILTIN);

    // Depend on slot 20 which has no device
    dev.addDependency(20);

    mgr->initAll();

    // Should still initialize — missing deps are treated as satisfied
    TEST_ASSERT_EQUAL(1, dev.initCallCount);
}

// ============================================================
// Section 7: initAll integration with sequence verification
// ============================================================

void test_initall_linear_chain_five_devices() {
    // E → D → C → B → A (E depends on D, D depends on C, etc.)
    DepTestDevice devA("test,a", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);
    DepTestDevice devB("test,b", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);
    DepTestDevice devC("test,c", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);
    DepTestDevice devD("test,d", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);
    DepTestDevice devE("test,e", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);

    int slotA = mgr->registerDevice(&devA, HAL_DISC_BUILTIN);
    int slotB = mgr->registerDevice(&devB, HAL_DISC_BUILTIN);
    int slotC = mgr->registerDevice(&devC, HAL_DISC_BUILTIN);
    int slotD = mgr->registerDevice(&devD, HAL_DISC_BUILTIN);
    int slotE = mgr->registerDevice(&devE, HAL_DISC_BUILTIN);

    devE.addDependency(slotD);
    devD.addDependency(slotC);
    devC.addDependency(slotB);
    devB.addDependency(slotA);

    mgr->initAll();

    // Strict ordering: A, B, C, D, E
    TEST_ASSERT_LESS_THAN(devB.initOrder, devA.initOrder);
    TEST_ASSERT_LESS_THAN(devC.initOrder, devB.initOrder);
    TEST_ASSERT_LESS_THAN(devD.initOrder, devC.initOrder);
    TEST_ASSERT_LESS_THAN(devE.initOrder, devD.initOrder);
}

void test_initall_all_devices_init_exactly_once() {
    DepTestDevice devA("test,a", HAL_DEV_DAC);
    DepTestDevice devB("test,b", HAL_DEV_DAC);
    DepTestDevice devC("test,c", HAL_DEV_DAC);

    int slotA = mgr->registerDevice(&devA, HAL_DISC_BUILTIN);
    int slotB = mgr->registerDevice(&devB, HAL_DISC_BUILTIN);
    int slotC = mgr->registerDevice(&devC, HAL_DISC_BUILTIN);

    devC.addDependency(slotA);
    devC.addDependency(slotB);

    mgr->initAll();

    TEST_ASSERT_EQUAL(1, devA.initCallCount);
    TEST_ASSERT_EQUAL(1, devB.initCallCount);
    TEST_ASSERT_EQUAL(1, devC.initCallCount);
}

void test_initall_failed_dependency_still_inits_dependent() {
    // If devA (dependency) fails init, devB (dependent) should still attempt init
    DepTestDevice devA("test,a", HAL_DEV_DAC);
    DepTestDevice devB("test,b", HAL_DEV_DAC);

    int slotA = mgr->registerDevice(&devA, HAL_DISC_BUILTIN);
    mgr->registerDevice(&devB, HAL_DISC_BUILTIN);

    devA.initResult = false;  // A will fail
    devB.addDependency(slotA);

    mgr->initAll();

    // Both should have been called
    TEST_ASSERT_EQUAL(1, devA.initCallCount);
    TEST_ASSERT_EQUAL(1, devB.initCallCount);
}

void test_initall_wide_fan_out() {
    // One root device, 5 devices all depending on it
    DepTestDevice root("test,root", HAL_DEV_DAC, HAL_PRIORITY_HARDWARE);
    int rootSlot = mgr->registerDevice(&root, HAL_DISC_BUILTIN);

    DepTestDevice fans[5];
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "test,fan%d", i);
        fans[i].setCompatible(name);
        mgr->registerDevice(&fans[i], HAL_DISC_BUILTIN);
        fans[i].addDependency(rootSlot);
    }

    mgr->initAll();

    // Root must init first
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_LESS_THAN(fans[i].initOrder, root.initOrder);
    }
}

// ============================================================
// Main
// ============================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Section 1: Bitfield operations
    RUN_TEST(test_add_dependency_sets_correct_bit);
    RUN_TEST(test_has_dependency_returns_false_for_unset_bits);
    RUN_TEST(test_multiple_dependencies);
    RUN_TEST(test_get_dependencies_returns_full_bitfield);
    RUN_TEST(test_add_same_dependency_twice_is_idempotent);

    // Section 2: Boundary tests
    RUN_TEST(test_dependency_slot_zero_lsb);
    RUN_TEST(test_dependency_slot_31_msb);
    RUN_TEST(test_invalid_slot_32_rejected);
    RUN_TEST(test_invalid_slot_255_rejected);

    // Section 3: Topological sort
    RUN_TEST(test_no_dependencies_uses_priority_ordering);
    RUN_TEST(test_linear_chain_a_depends_b_depends_c);
    RUN_TEST(test_diamond_dependency);
    RUN_TEST(test_priority_tiebreaker_within_same_topo_level);

    // Section 4: Cycle detection
    RUN_TEST(test_cycle_a_depends_b_depends_a_falls_back_to_priority);
    RUN_TEST(test_three_node_cycle);

    // Section 5: Mixed dependencies + priorities
    RUN_TEST(test_deps_override_priority_when_needed);
    RUN_TEST(test_independent_devices_still_sort_by_priority);

    // Section 6: Edge cases
    RUN_TEST(test_empty_device_list_no_crash);
    RUN_TEST(test_single_device_no_deps);
    RUN_TEST(test_single_device_self_dependency_handled);
    RUN_TEST(test_dependency_on_nonexistent_slot);

    // Section 7: initAll integration
    RUN_TEST(test_initall_linear_chain_five_devices);
    RUN_TEST(test_initall_all_devices_init_exactly_once);
    RUN_TEST(test_initall_failed_dependency_still_inits_dependent);
    RUN_TEST(test_initall_wide_fan_out);

    return UNITY_END();
}
