#ifdef GUI_ENABLED

#include "scr_settings.h"
#include "scr_menu.h"
#include "scr_value_edit.h"
#include "../gui_config.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../gui_theme.h"
#include "../../app_state.h"
#include "../../config.h"
#include "../../settings_manager.h"
#include "../../ota_updater.h"
#include "../gui_manager.h"
#include <Arduino.h>

/* Screen timeout cycle options (in milliseconds) */
static const CycleOption timeout_options[] = {
    {"30 sec",  30000},
    {"1 min",   60000},
    {"5 min",   300000},
    {"10 min",  600000},
    {"Never",   0},
};

/* ===== Value editor confirmations ===== */

static void on_screen_timeout_confirm(int int_val, float, int) {
    AppState::getInstance().setScreenTimeout((unsigned long)int_val);
    saveSettings();
    Serial.printf("[GUI] Screen timeout set to %d ms\n", int_val);
}

static void on_backlight_confirm(int int_val, float, int) {
    if (int_val) {
        gui_wake();
    } else {
        gui_sleep();
    }
    Serial.printf("[GUI] Backlight %s\n", int_val ? "ON" : "OFF");
}

static void on_night_mode_confirm(int int_val, float, int) {
    AppState::getInstance().nightMode = (int_val != 0);
    gui_theme_set_dark(int_val != 0);
    saveSettings();
    AppState::getInstance().markSettingsDirty();
    Serial.printf("[GUI] Night mode %s\n", int_val ? "ON" : "OFF");
}

static void on_auto_update_confirm(int int_val, float, int) {
    AppState::getInstance().autoUpdateEnabled = (int_val != 0);
    saveSettings();
    AppState::getInstance().markSettingsDirty();
    Serial.printf("[GUI] Auto update %s\n", int_val ? "enabled" : "disabled");
}

static void on_ssl_confirm(int int_val, float, int) {
    AppState::getInstance().enableCertValidation = (int_val != 0);
    saveSettings();
    AppState::getInstance().markSettingsDirty();
    Serial.printf("[GUI] SSL validation %s\n", int_val ? "enabled" : "disabled");
}

/* Boot animation cycle options: None + 6 styles */
static const CycleOption boot_anim_options[] = {
    {"None",       -1},
    {"Wave Pulse",  0},
    {"Speaker",     1},
    {"Waveform",    2},
    {"Beat Bounce", 3},
    {"Freq Bars",   4},
    {"Heartbeat",   5},
};

static void on_boot_anim_confirm(int int_val, float, int) {
    AppState &st = AppState::getInstance();
    if (int_val < 0) {
        st.bootAnimEnabled = false;
        Serial.println("[GUI] Boot animation disabled");
    } else {
        st.bootAnimEnabled = true;
        st.bootAnimStyle = int_val;
        Serial.printf("[GUI] Boot animation set to style %d\n", int_val);
    }
    saveSettings();
}

/* ===== Reboot / Factory Reset with confirmation ===== */

static void do_reboot(void) {
    /* Use value editor as a confirm dialog: toggle starts at OFF, flip to ON to confirm */
    ValueEditConfig cfg = {};
    cfg.title = "Reboot? ON=Yes";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = false;
    cfg.on_confirm = [](int int_val, float, int) {
        if (int_val) {
            Serial.println("[GUI] Rebooting...");
            delay(500);
            ESP.restart();
        }
    };
    scr_value_edit_open(&cfg);
}

static void do_factory_reset(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Reset? ON=Yes";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = false;
    cfg.on_confirm = [](int int_val, float, int) {
        if (int_val) {
            Serial.println("[GUI] Factory reset...");
            performFactoryReset();
        }
    };
    scr_value_edit_open(&cfg);
}

/* ===== Menu action callbacks ===== */

static void edit_screen_timeout(void) {
    AppState &st = AppState::getInstance();
    int cur = 1; /* default: 1 min */
    for (int i = 0; i < 5; i++) {
        if ((unsigned long)timeout_options[i].value == st.screenTimeout) { cur = i; break; }
    }
    ValueEditConfig cfg = {};
    cfg.title = "Screen Timeout";
    cfg.type = VE_CYCLE;
    cfg.options = timeout_options;
    cfg.option_count = 5;
    cfg.current_option = cur;
    cfg.on_confirm = on_screen_timeout_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_backlight(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Backlight";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = AppState::getInstance().backlightOn;
    cfg.on_confirm = on_backlight_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_night_mode(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Night Mode";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = AppState::getInstance().nightMode;
    cfg.on_confirm = on_night_mode_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_auto_update(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Auto Update";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = AppState::getInstance().autoUpdateEnabled;
    cfg.on_confirm = on_auto_update_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_ssl_validation(void) {
    ValueEditConfig cfg = {};
    cfg.title = "SSL Validation";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = AppState::getInstance().enableCertValidation;
    cfg.on_confirm = on_ssl_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_boot_anim(void) {
    AppState &st = AppState::getInstance();
    int cur = 0; /* default: None */
    if (st.bootAnimEnabled) {
        for (int i = 1; i < 7; i++) {
            if (boot_anim_options[i].value == st.bootAnimStyle) { cur = i; break; }
        }
    }
    ValueEditConfig cfg = {};
    cfg.title = "Boot Animation";
    cfg.type = VE_CYCLE;
    cfg.options = boot_anim_options;
    cfg.option_count = 7;
    cfg.current_option = cur;
    cfg.on_confirm = on_boot_anim_confirm;
    scr_value_edit_open(&cfg);
}

/* ===== Build the settings menu ===== */
static MenuConfig settings_menu;

static void build_settings_menu(void) {
    AppState &st = AppState::getInstance();

    /* Screen timeout display */
    static char timeout_str[12];
    const char *t = "Custom";
    for (int i = 0; i < 5; i++) {
        if ((unsigned long)timeout_options[i].value == st.screenTimeout) {
            t = timeout_options[i].label;
            break;
        }
    }
    snprintf(timeout_str, sizeof(timeout_str), "%s", t);

    static char backlight_str[8];
    snprintf(backlight_str, sizeof(backlight_str), "%s", st.backlightOn ? "ON" : "OFF");

    static char night_str[8];
    snprintf(night_str, sizeof(night_str), "%s", st.nightMode ? "ON" : "OFF");

    static char boot_anim_str[14];
    if (!st.bootAnimEnabled) {
        snprintf(boot_anim_str, sizeof(boot_anim_str), "None");
    } else {
        const char *style_name = "Wave Pulse";
        for (int i = 1; i < 7; i++) {
            if (boot_anim_options[i].value == st.bootAnimStyle) {
                style_name = boot_anim_options[i].label;
                break;
            }
        }
        snprintf(boot_anim_str, sizeof(boot_anim_str), "%s", style_name);
    }

    static char update_str[8];
    snprintf(update_str, sizeof(update_str), "%s", st.autoUpdateEnabled ? "ON" : "OFF");

    static char ssl_str[8];
    snprintf(ssl_str, sizeof(ssl_str), "%s", st.enableCertValidation ? "ON" : "OFF");

    static char fw_str[24];
    if (st.updateAvailable) {
        snprintf(fw_str, sizeof(fw_str), "%s -> %s", FIRMWARE_VERSION, st.cachedLatestVersion.c_str());
    } else {
        snprintf(fw_str, sizeof(fw_str), "%s", FIRMWARE_VERSION);
    }

    settings_menu.title = "Settings";
    settings_menu.item_count = 10;
    settings_menu.items[0] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};
    settings_menu.items[1] = {"Screen Timeout", timeout_str, nullptr, MENU_ACTION, edit_screen_timeout};
    settings_menu.items[2] = {"Backlight", backlight_str, nullptr, MENU_ACTION, edit_backlight};
    settings_menu.items[3] = {"Night Mode", night_str, nullptr, MENU_ACTION, edit_night_mode};
    settings_menu.items[4] = {"Boot Animation", boot_anim_str, nullptr, MENU_ACTION, edit_boot_anim};
    settings_menu.items[5] = {"Auto Update", update_str, nullptr, MENU_ACTION, edit_auto_update};
    settings_menu.items[6] = {"SSL Validation", ssl_str, nullptr, MENU_ACTION, edit_ssl_validation};
    settings_menu.items[7] = {"Firmware", fw_str, ICON_SETTINGS, MENU_INFO, nullptr};
    settings_menu.items[8] = {"Reboot", nullptr, ICON_REFRESH, MENU_ACTION, do_reboot};
    settings_menu.items[9] = {"Factory Reset", nullptr, ICON_WARNING, MENU_ACTION, do_factory_reset};
}

lv_obj_t *scr_settings_create(void) {
    build_settings_menu();
    return scr_menu_create(&settings_menu);
}

#endif /* GUI_ENABLED */
