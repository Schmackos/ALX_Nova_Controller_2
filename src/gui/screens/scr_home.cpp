#ifdef GUI_ENABLED

#include "scr_home.h"
#include "scr_menu.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../../app_state.h"
#include "../../config.h"
#include "../../debug_serial.h"
#include <Arduino.h>
#include <WiFi.h>

/* Format uptime from milliseconds into human-readable string */
static void format_uptime(unsigned long ms, char *buf, int len) {
    unsigned long secs = ms / 1000;
    unsigned long mins = secs / 60;
    unsigned long hours = mins / 60;
    unsigned long days = hours / 24;
    if (days > 0) {
        snprintf(buf, len, "%lud %luh", days, hours % 24);
    } else if (hours > 0) {
        snprintf(buf, len, "%luh %lum", hours, mins % 60);
    } else if (mins > 0) {
        snprintf(buf, len, "%lum %lus", mins, secs % 60);
    } else {
        snprintf(buf, len, "%lus", secs);
    }
}

/* Format timer remaining (seconds) into MM:SS */
static void format_timer(unsigned long remaining_secs, char *buf, int len) {
    if (remaining_secs == 0) {
        snprintf(buf, len, "Idle");
    } else {
        unsigned long mins = remaining_secs / 60;
        unsigned long secs = remaining_secs % 60;
        snprintf(buf, len, "%02lu:%02lu", mins, secs);
    }
}

static MenuConfig home_config;

lv_obj_t *scr_home_create(void) {
    home_config = {};
    home_config.title = "Home";
    home_config.item_count = 9;

    /* 0: Back */
    home_config.items[0] = {ICON_BACK " Back", NULL, NULL, MENU_BACK, NULL};

    /* 1-8: Info items */
    home_config.items[1] = {"Amplifier",  "---", ICON_CONTROL,  MENU_INFO, NULL};
    home_config.items[2] = {"Signal",     "---", NULL,          MENU_INFO, NULL};
    home_config.items[3] = {"Timer",      "---", NULL,          MENU_INFO, NULL};
    home_config.items[4] = {"WiFi",       "---", ICON_WIFI,     MENU_INFO, NULL};
    home_config.items[5] = {"MQTT",       "---", ICON_MQTT,     MENU_INFO, NULL};
    home_config.items[6] = {"Mode",       "---", ICON_SETTINGS, MENU_INFO, NULL};
    home_config.items[7] = {"Firmware",   "---", NULL,          MENU_INFO, NULL};
    home_config.items[8] = {"Uptime",     "---", ICON_REFRESH,  MENU_INFO, NULL};

    lv_obj_t *scr = scr_menu_create(&home_config);

    /* Populate values immediately */
    scr_home_refresh();

    LOG_D("[GUI] Home screen created");
    return scr;
}

void scr_home_refresh(void) {
    AppState &st = AppState::getInstance();
    char buf[32];

    /* 1: Amplifier */
    scr_menu_set_item_value(1, st.amplifierState ? "ON" : "OFF");

    /* 2: Signal */
    bool detected = st.lastVoltageReading >= st.voltageThreshold;
    snprintf(buf, sizeof(buf), "%.2fV %s", st.lastVoltageReading,
             detected ? "Detected" : "\xE2\x80\x94");
    scr_menu_set_item_value(2, buf);

    /* 3: Timer */
    if (st.currentMode != SMART_AUTO || st.timerRemaining == 0) {
        scr_menu_set_item_value(3, "Idle");
    } else {
        format_timer(st.timerRemaining, buf, sizeof(buf));
        scr_menu_set_item_value(3, buf);
    }

    /* 4: WiFi */
    if (WiFi.status() == WL_CONNECTED) {
        scr_menu_set_item_value(4, "Connected");
    } else if (st.isAPMode) {
        scr_menu_set_item_value(4, "AP Mode");
    } else {
        scr_menu_set_item_value(4, "Disconnected");
    }

    /* 5: MQTT */
    if (!st.mqttEnabled) {
        scr_menu_set_item_value(5, "Disabled");
    } else if (st.mqttConnected) {
        scr_menu_set_item_value(5, "Connected");
    } else {
        scr_menu_set_item_value(5, "Disconnected");
    }

    /* 6: Mode */
    const char *mode = (st.currentMode == ALWAYS_ON)  ? "Always On" :
                       (st.currentMode == ALWAYS_OFF) ? "Always Off" : "Smart Auto";
    scr_menu_set_item_value(6, mode);

    /* 7: Firmware */
    scr_menu_set_item_value(7, FIRMWARE_VERSION);

    /* 8: Uptime */
    format_uptime(millis(), buf, sizeof(buf));
    scr_menu_set_item_value(8, buf);
}

#endif /* GUI_ENABLED */
