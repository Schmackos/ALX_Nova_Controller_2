#include <cstring>
#include <string>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

/**
 * GUI Navigation Tests
 *
 * Tests the navigation stack logic (push/pop/pop_to_root)
 * using a simplified re-implementation of the navigation state machine.
 * This avoids LVGL dependencies while verifying the core algorithm.
 */

/* ===== Simplified Navigation Stack (mirrors gui_navigation.cpp logic) ===== */

#define NAV_STACK_MAX 8

enum ScreenId {
    SCR_DESKTOP = 0,
    SCR_CONTROL_MENU,
    SCR_WIFI_MENU,
    SCR_MQTT_MENU,
    SCR_SETTINGS_MENU,
    SCR_DEBUG_MENU,
    SCR_VALUE_EDIT,
    SCR_KEYBOARD,
    SCR_WIFI_SCAN,
    SCR_WIFI_AP_MENU,
    SCR_WIFI_NET_MENU,
    SCR_INFO,
    SCR_COUNT
};

static ScreenId nav_stack[NAV_STACK_MAX];
static int nav_depth = 0;

static void nav_init(void) {
    nav_depth = 0;
}

static bool nav_push(ScreenId id) {
    if (nav_depth >= NAV_STACK_MAX) {
        return false; /* Stack overflow */
    }
    nav_stack[nav_depth] = id;
    nav_depth++;
    return true;
}

static bool nav_pop(void) {
    if (nav_depth <= 1) {
        return false; /* Can't pop root */
    }
    nav_depth--;
    return true;
}

static void nav_pop_to_root(void) {
    if (nav_depth <= 1) return;
    nav_depth = 1;
}

static ScreenId nav_current(void) {
    if (nav_depth == 0) return SCR_DESKTOP;
    return nav_stack[nav_depth - 1];
}

static int nav_get_depth(void) {
    return nav_depth;
}

/* ===== Tests ===== */

void setUp(void) {
    nav_init();
}

void tearDown(void) {}

/* Test: Initial state is empty */
void test_nav_initial_state(void) {
    TEST_ASSERT_EQUAL(0, nav_get_depth());
    TEST_ASSERT_EQUAL(SCR_DESKTOP, nav_current());
}

/* Test: Push desktop as first screen */
void test_nav_push_desktop(void) {
    TEST_ASSERT_TRUE(nav_push(SCR_DESKTOP));
    TEST_ASSERT_EQUAL(1, nav_get_depth());
    TEST_ASSERT_EQUAL(SCR_DESKTOP, nav_current());
}

/* Test: Push then navigate to control menu */
void test_nav_push_into_menu(void) {
    nav_push(SCR_DESKTOP);
    nav_push(SCR_CONTROL_MENU);

    TEST_ASSERT_EQUAL(2, nav_get_depth());
    TEST_ASSERT_EQUAL(SCR_CONTROL_MENU, nav_current());
}

/* Test: Pop returns to previous screen */
void test_nav_pop_returns_to_previous(void) {
    nav_push(SCR_DESKTOP);
    nav_push(SCR_WIFI_MENU);
    nav_push(SCR_WIFI_AP_MENU);

    TEST_ASSERT_EQUAL(3, nav_get_depth());
    TEST_ASSERT_EQUAL(SCR_WIFI_AP_MENU, nav_current());

    nav_pop();
    TEST_ASSERT_EQUAL(2, nav_get_depth());
    TEST_ASSERT_EQUAL(SCR_WIFI_MENU, nav_current());

    nav_pop();
    TEST_ASSERT_EQUAL(1, nav_get_depth());
    TEST_ASSERT_EQUAL(SCR_DESKTOP, nav_current());
}

/* Test: Can't pop below root */
void test_nav_pop_at_root(void) {
    nav_push(SCR_DESKTOP);

    TEST_ASSERT_FALSE(nav_pop());
    TEST_ASSERT_EQUAL(1, nav_get_depth());
    TEST_ASSERT_EQUAL(SCR_DESKTOP, nav_current());
}

/* Test: Can't pop when empty */
void test_nav_pop_when_empty(void) {
    TEST_ASSERT_FALSE(nav_pop());
    TEST_ASSERT_EQUAL(0, nav_get_depth());
}

/* Test: Pop to root from deep navigation */
void test_nav_pop_to_root(void) {
    nav_push(SCR_DESKTOP);
    nav_push(SCR_SETTINGS_MENU);
    nav_push(SCR_VALUE_EDIT);
    nav_push(SCR_KEYBOARD);

    TEST_ASSERT_EQUAL(4, nav_get_depth());

    nav_pop_to_root();
    TEST_ASSERT_EQUAL(1, nav_get_depth());
    TEST_ASSERT_EQUAL(SCR_DESKTOP, nav_current());
}

/* Test: Pop to root when already at root */
void test_nav_pop_to_root_at_root(void) {
    nav_push(SCR_DESKTOP);
    nav_pop_to_root();

    TEST_ASSERT_EQUAL(1, nav_get_depth());
    TEST_ASSERT_EQUAL(SCR_DESKTOP, nav_current());
}

/* Test: Stack overflow protection */
void test_nav_stack_overflow(void) {
    for (int i = 0; i < NAV_STACK_MAX; i++) {
        TEST_ASSERT_TRUE(nav_push(SCR_DESKTOP));
    }

    /* Should fail on overflow */
    TEST_ASSERT_FALSE(nav_push(SCR_CONTROL_MENU));
    TEST_ASSERT_EQUAL(NAV_STACK_MAX, nav_get_depth());
}

/* Test: Deep navigation and pop sequence */
void test_nav_deep_push_pop_sequence(void) {
    nav_push(SCR_DESKTOP);           /* depth 1 */
    nav_push(SCR_MQTT_MENU);         /* depth 2 */
    nav_push(SCR_VALUE_EDIT);        /* depth 3 */

    TEST_ASSERT_EQUAL(SCR_VALUE_EDIT, nav_current());

    nav_pop();                       /* back to MQTT */
    TEST_ASSERT_EQUAL(SCR_MQTT_MENU, nav_current());

    nav_push(SCR_KEYBOARD);          /* depth 3 again */
    TEST_ASSERT_EQUAL(SCR_KEYBOARD, nav_current());

    nav_pop();                       /* back to MQTT */
    nav_pop();                       /* back to desktop */
    TEST_ASSERT_EQUAL(SCR_DESKTOP, nav_current());
    TEST_ASSERT_EQUAL(1, nav_get_depth());
}

/* Test: All screen IDs are pushable */
void test_nav_all_screen_ids(void) {
    for (int i = 0; i < (int)SCR_COUNT; i++) {
        nav_init();
        nav_push((ScreenId)i);
        TEST_ASSERT_EQUAL(i, nav_current());
    }
}

/* Test: Pop preserves stack integrity */
void test_nav_push_pop_push_sequence(void) {
    nav_push(SCR_DESKTOP);
    nav_push(SCR_CONTROL_MENU);
    nav_pop();
    nav_push(SCR_WIFI_MENU);

    TEST_ASSERT_EQUAL(2, nav_get_depth());
    TEST_ASSERT_EQUAL(SCR_WIFI_MENU, nav_current());

    nav_pop();
    TEST_ASSERT_EQUAL(SCR_DESKTOP, nav_current());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_nav_initial_state);
    RUN_TEST(test_nav_push_desktop);
    RUN_TEST(test_nav_push_into_menu);
    RUN_TEST(test_nav_pop_returns_to_previous);
    RUN_TEST(test_nav_pop_at_root);
    RUN_TEST(test_nav_pop_when_empty);
    RUN_TEST(test_nav_pop_to_root);
    RUN_TEST(test_nav_pop_to_root_at_root);
    RUN_TEST(test_nav_stack_overflow);
    RUN_TEST(test_nav_deep_push_pop_sequence);
    RUN_TEST(test_nav_all_screen_ids);
    RUN_TEST(test_nav_push_pop_push_sequence);

    return UNITY_END();
}
