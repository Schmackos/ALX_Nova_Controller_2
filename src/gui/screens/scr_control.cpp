#ifdef GUI_ENABLED

#include "scr_control.h"
#include "scr_menu.h"
#include "scr_value_edit.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../../app_state.h"
#include "../../smart_sensing.h"
#include "../../debug_serial.h"
#include <Arduino.h>

/* Sensing mode cycle options */
static const CycleOption sensing_modes[] = {
    {"Always On",  ALWAYS_ON},
    {"Always Off", ALWAYS_OFF},
    {"Smart Auto", SMART_AUTO},
};

/* Callbacks for value editor confirmations */
static void on_sensing_mode_confirm(int int_val, float, int) {
    AppState::getInstance().setSensingMode((SensingMode)int_val);
    saveSmartSensingSettings();
    LOG_I("[GUI] Sensing mode changed to %d", int_val);
}

static void on_amplifier_confirm(int int_val, float, int) {
    setAmplifierState(int_val != 0);
    LOG_I("[GUI] Amplifier set to %s", int_val ? "ON" : "OFF");
}

static void on_timer_confirm(int int_val, float, int) {
    AppState::getInstance().timerDuration = (unsigned long)int_val;
    saveSmartSensingSettings();
    LOG_I("[GUI] Timer duration set to %d min", int_val);
}

static void on_voltage_confirm(int, float float_val, int) {
    AppState::getInstance().voltageThreshold = float_val;
    saveSmartSensingSettings();
    LOG_I("[GUI] Voltage threshold set to %.2fV", float_val);
}

static void on_blinking_confirm(int int_val, float, int) {
    AppState::getInstance().setBlinkingEnabled(int_val != 0);
    LOG_I("[GUI] LED blinking set to %s", int_val ? "ON" : "OFF");
}

/* Menu action callbacks — open value editors */
static void edit_sensing_mode(void) {
    AppState &st = AppState::getInstance();
    int cur = 0;
    for (int i = 0; i < 3; i++) {
        if (sensing_modes[i].value == st.currentMode) { cur = i; break; }
    }
    ValueEditConfig cfg = {};
    cfg.title = "Sensing Mode";
    cfg.type = VE_CYCLE;
    cfg.options = sensing_modes;
    cfg.option_count = 3;
    cfg.current_option = cur;
    cfg.on_confirm = on_sensing_mode_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_amplifier(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Amplifier";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = AppState::getInstance().amplifierState;
    cfg.on_confirm = on_amplifier_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_timer_duration(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Timer Duration";
    cfg.type = VE_NUMERIC;
    cfg.int_val = (int)AppState::getInstance().timerDuration;
    cfg.int_min = 1;
    cfg.int_max = 60;
    cfg.int_step = 1;
    cfg.int_unit = "min";
    cfg.on_confirm = on_timer_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_voltage_threshold(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Voltage Thresh";
    cfg.type = VE_FLOAT;
    cfg.float_val = AppState::getInstance().voltageThreshold;
    cfg.float_min = 0.1f;
    cfg.float_max = 3.3f;
    cfg.float_step = 0.1f;
    cfg.float_unit = "V";
    cfg.float_decimals = 1;
    cfg.on_confirm = on_voltage_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_led_blinking(void) {
    ValueEditConfig cfg = {};
    cfg.title = "LED Blinking";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = AppState::getInstance().blinkingEnabled;
    cfg.on_confirm = on_blinking_confirm;
    scr_value_edit_open(&cfg);
}

/* Build the control menu */
static MenuConfig control_menu;

static void build_control_menu(void) {
    AppState &st = AppState::getInstance();

    const char *mode_str = (st.currentMode == ALWAYS_ON) ? "Always On" :
                           (st.currentMode == ALWAYS_OFF) ? "Always Off" : "Smart Auto";

    static char amp_str[8];
    snprintf(amp_str, sizeof(amp_str), "%s", st.amplifierState ? "ON" : "OFF");

    static char timer_str[12];
    snprintf(timer_str, sizeof(timer_str), "%lu min", st.timerDuration);

    static char volt_str[12];
    snprintf(volt_str, sizeof(volt_str), "%.1fV", st.voltageThreshold);

    static char blink_str[8];
    snprintf(blink_str, sizeof(blink_str), "%s", st.blinkingEnabled ? "ON" : "OFF");

    control_menu.title = "Control";
    control_menu.item_count = 6;
    control_menu.items[0] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};
    control_menu.items[1] = {"Sensing Mode", mode_str, ICON_SETTINGS, MENU_ACTION, edit_sensing_mode};
    control_menu.items[2] = {"Amplifier", amp_str, ICON_CONTROL, MENU_ACTION, edit_amplifier};
    control_menu.items[3] = {"Timer Duration", timer_str, nullptr, MENU_ACTION, edit_timer_duration};
    control_menu.items[4] = {"Voltage Thresh", volt_str, nullptr, MENU_ACTION, edit_voltage_threshold};
    control_menu.items[5] = {"LED Blinking", blink_str, nullptr, MENU_ACTION, edit_led_blinking};
}

lv_obj_t *scr_control_create(void) {
    build_control_menu();
    return scr_menu_create(&control_menu);
}

void scr_control_refresh(void) {
    AppState &st = AppState::getInstance();

    const char *mode_str = (st.currentMode == ALWAYS_ON) ? "Always On" :
                           (st.currentMode == ALWAYS_OFF) ? "Always Off" : "Smart Auto";
    scr_menu_set_item_value(1, mode_str);

    scr_menu_set_item_value(2, st.amplifierState ? "ON" : "OFF");

    static char timer_buf[12];
    snprintf(timer_buf, sizeof(timer_buf), "%lu min", st.timerDuration);
    scr_menu_set_item_value(3, timer_buf);

    static char volt_buf[12];
    snprintf(volt_buf, sizeof(volt_buf), "%.1fV", st.voltageThreshold);
    scr_menu_set_item_value(4, volt_buf);

    scr_menu_set_item_value(5, st.blinkingEnabled ? "ON" : "OFF");
}

#endif /* GUI_ENABLED */
