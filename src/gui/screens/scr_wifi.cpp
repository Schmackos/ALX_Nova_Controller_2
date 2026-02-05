#ifdef GUI_ENABLED

#include "scr_wifi.h"
#include "scr_menu.h"
#include "scr_value_edit.h"
#include "scr_keyboard.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../../app_state.h"
#include "../../wifi_manager.h"
#include "../../settings_manager.h"
#include <Arduino.h>
#include <WiFi.h>

/* ===== WiFi Info Screen (read-only) ===== */

static lv_obj_t *create_wifi_info_screen(void) {
    AppState &st = AppState::getInstance();

    static char ssid_str[40], ip_str[24], rssi_str[16], mac_str[24], gw_str[24];

    if (WiFi.status() == WL_CONNECTED) {
        snprintf(ssid_str, sizeof(ssid_str), "%s", WiFi.SSID().c_str());
        snprintf(ip_str, sizeof(ip_str), "%s", WiFi.localIP().toString().c_str());
        snprintf(rssi_str, sizeof(rssi_str), "%d dBm", WiFi.RSSI());
        snprintf(gw_str, sizeof(gw_str), "%s", WiFi.gatewayIP().toString().c_str());
    } else {
        snprintf(ssid_str, sizeof(ssid_str), "Not connected");
        snprintf(ip_str, sizeof(ip_str), "---");
        snprintf(rssi_str, sizeof(rssi_str), "---");
        snprintf(gw_str, sizeof(gw_str), "---");
    }
    snprintf(mac_str, sizeof(mac_str), "%s", WiFi.macAddress().c_str());

    static MenuConfig cfg;
    cfg.title = "WiFi Status";
    cfg.item_count = 6;
    cfg.items[0] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};
    cfg.items[1] = {"SSID", ssid_str, nullptr, MENU_INFO, nullptr};
    cfg.items[2] = {"IP", ip_str, nullptr, MENU_INFO, nullptr};
    cfg.items[3] = {"Signal", rssi_str, nullptr, MENU_INFO, nullptr};
    cfg.items[4] = {"Gateway", gw_str, nullptr, MENU_INFO, nullptr};
    cfg.items[5] = {"MAC", mac_str, nullptr, MENU_INFO, nullptr};

    return scr_menu_create(&cfg);
}

static void show_wifi_info(void) {
    gui_nav_register(SCR_INFO, create_wifi_info_screen);
    gui_nav_push(SCR_INFO);
}

/* ===== WiFi Scan + Connect ===== */

static String scanned_ssids[10];
static int scan_count = 0;

/* Temporary storage for selected SSID during connect flow */
static String selected_ssid;

static void on_password_entered(const char *password) {
    Serial.printf("[GUI] Connecting to %s\n", selected_ssid.c_str());
    WiFiNetworkConfig config;
    config.ssid = selected_ssid;
    config.password = String(password);
    config.useStaticIP = false;
    saveWiFiNetwork(config);
    connectToWiFi(config);
}

static void scan_item_action_factory(int idx) {
    selected_ssid = scanned_ssids[idx];

    KeyboardConfig kb = {};
    kb.title = "Password";
    kb.initial_text = nullptr;
    kb.password_mode = true;
    kb.on_done = on_password_entered;
    scr_keyboard_open(&kb);
}

/* We need individual callbacks for up to 10 scan results */
static void scan_action_0(void) { scan_item_action_factory(0); }
static void scan_action_1(void) { scan_item_action_factory(1); }
static void scan_action_2(void) { scan_item_action_factory(2); }
static void scan_action_3(void) { scan_item_action_factory(3); }
static void scan_action_4(void) { scan_item_action_factory(4); }
static void scan_action_5(void) { scan_item_action_factory(5); }
static void scan_action_6(void) { scan_item_action_factory(6); }
static void scan_action_7(void) { scan_item_action_factory(7); }
static void scan_action_8(void) { scan_item_action_factory(8); }
static void scan_action_9(void) { scan_item_action_factory(9); }

static menu_action_fn scan_actions[] = {
    scan_action_0, scan_action_1, scan_action_2, scan_action_3, scan_action_4,
    scan_action_5, scan_action_6, scan_action_7, scan_action_8, scan_action_9,
};

static lv_obj_t *create_wifi_scan_screen(void) {
    /* Perform synchronous scan */
    int n = WiFi.scanNetworks();
    scan_count = (n > 10) ? 10 : n;

    static char rssi_bufs[10][12];
    static MenuConfig cfg;
    cfg.title = "Select Network";
    cfg.item_count = 1 + scan_count;
    cfg.items[0] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};

    for (int i = 0; i < scan_count; i++) {
        scanned_ssids[i] = WiFi.SSID(i);
        snprintf(rssi_bufs[i], sizeof(rssi_bufs[i]), "%ddBm", WiFi.RSSI(i));
        cfg.items[1 + i] = {scanned_ssids[i].c_str(), rssi_bufs[i], ICON_WIFI, MENU_ACTION, scan_actions[i]};
    }

    WiFi.scanDelete();
    return scr_menu_create(&cfg);
}

static void show_wifi_scan(void) {
    gui_nav_register(SCR_WIFI_SCAN, create_wifi_scan_screen);
    gui_nav_push(SCR_WIFI_SCAN);
}

/* ===== Access Point Sub-menu ===== */

static void on_ap_toggle_confirm(int val, float, int) {
    if (val) {
        startAccessPoint();
    } else {
        stopAccessPoint();
    }
}

static void edit_ap_toggle(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Enable AP";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = AppState::getInstance().apEnabled;
    cfg.on_confirm = on_ap_toggle_confirm;
    scr_value_edit_open(&cfg);
}

static void on_auto_ap_confirm(int val, float, int) {
    AppState::getInstance().autoAPEnabled = (val != 0);
    saveSettings();
    AppState::getInstance().markSettingsDirty();
}

static void edit_auto_ap(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Auto AP";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = AppState::getInstance().autoAPEnabled;
    cfg.on_confirm = on_auto_ap_confirm;
    scr_value_edit_open(&cfg);
}

static void on_ap_ssid_done(const char *text) {
    AppState::getInstance().apSSID = String(text);
    saveSettings();
    AppState::getInstance().markSettingsDirty();
    Serial.printf("[GUI] AP SSID set to: %s\n", text);
}

static void edit_ap_ssid(void) {
    KeyboardConfig kb = {};
    kb.title = "AP SSID";
    kb.initial_text = AppState::getInstance().apSSID.c_str();
    kb.password_mode = false;
    kb.on_done = on_ap_ssid_done;
    scr_keyboard_open(&kb);
}

static void on_ap_password_done(const char *text) {
    AppState::getInstance().apPassword = String(text);
    saveSettings();
    AppState::getInstance().markSettingsDirty();
    Serial.printf("[GUI] AP password changed\n");
}

static void edit_ap_password(void) {
    KeyboardConfig kb = {};
    kb.title = "AP Password";
    kb.initial_text = nullptr;
    kb.password_mode = true;
    kb.on_done = on_ap_password_done;
    scr_keyboard_open(&kb);
}

lv_obj_t *scr_wifi_ap_create(void) {
    AppState &st = AppState::getInstance();

    static char ap_str[8], auto_ap_str[8];
    snprintf(ap_str, sizeof(ap_str), "%s", st.apEnabled ? "ON" : "OFF");
    snprintf(auto_ap_str, sizeof(auto_ap_str), "%s", st.autoAPEnabled ? "ON" : "OFF");

    static MenuConfig cfg;
    cfg.title = "Access Point";
    cfg.item_count = 5;
    cfg.items[0] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};
    cfg.items[1] = {"Enable AP", ap_str, nullptr, MENU_ACTION, edit_ap_toggle};
    cfg.items[2] = {"Auto AP", auto_ap_str, nullptr, MENU_ACTION, edit_auto_ap};
    cfg.items[3] = {"AP SSID", st.apSSID.c_str(), ICON_EDIT, MENU_ACTION, edit_ap_ssid};
    cfg.items[4] = {"AP Password", "****", ICON_EDIT, MENU_ACTION, edit_ap_password};

    return scr_menu_create(&cfg);
}

/* ===== Network Config Sub-menu ===== */

lv_obj_t *scr_wifi_net_create(void) {
    static MenuConfig cfg;
    cfg.title = "Network Config";
    cfg.item_count = 2;
    cfg.items[0] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};
    cfg.items[1] = {"Select Network", nullptr, ICON_WIFI, MENU_SUBMENU, show_wifi_scan};

    return scr_menu_create(&cfg);
}

/* ===== Main WiFi Menu ===== */

static void show_net_config(void) {
    gui_nav_register(SCR_WIFI_NET_MENU, scr_wifi_net_create);
    gui_nav_push(SCR_WIFI_NET_MENU);
}

static void show_ap_config(void) {
    gui_nav_register(SCR_WIFI_AP_MENU, scr_wifi_ap_create);
    gui_nav_push(SCR_WIFI_AP_MENU);
}

lv_obj_t *scr_wifi_create(void) {
    static char status_str[24];
    if (WiFi.status() == WL_CONNECTED) {
        snprintf(status_str, sizeof(status_str), "Connected");
    } else if (AppState::getInstance().isAPMode) {
        snprintf(status_str, sizeof(status_str), "AP Mode");
    } else {
        snprintf(status_str, sizeof(status_str), "Disconnected");
    }

    static MenuConfig cfg;
    cfg.title = "WiFi";
    cfg.item_count = 4;
    cfg.items[0] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};
    cfg.items[1] = {"Connection", status_str, ICON_WIFI, MENU_ACTION, show_wifi_info};
    cfg.items[2] = {"Network Config", nullptr, nullptr, MENU_SUBMENU, show_net_config};
    cfg.items[3] = {"Access Point", nullptr, nullptr, MENU_SUBMENU, show_ap_config};

    return scr_menu_create(&cfg);
}

#endif /* GUI_ENABLED */
