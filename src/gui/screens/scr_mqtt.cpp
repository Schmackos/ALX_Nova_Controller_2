#ifdef GUI_ENABLED

#include "scr_mqtt.h"
#include "scr_menu.h"
#include "scr_value_edit.h"
#include "scr_keyboard.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../../app_state.h"
#include "../../mqtt_handler.h"
#include <Arduino.h>

/* ===== Value editor confirmations ===== */

static void on_mqtt_enable_confirm(int int_val, float, int) {
    AppState::getInstance().mqttEnabled = (int_val != 0);
    saveMqttSettings();
    AppState::getInstance().markSettingsDirty();
    if (int_val) {
        setupMqtt();
    }
    Serial.printf("[GUI] MQTT %s\n", int_val ? "enabled" : "disabled");
}

static void on_mqtt_port_confirm(int int_val, float, int) {
    AppState::getInstance().mqttPort = int_val;
    saveMqttSettings();
    AppState::getInstance().markSettingsDirty();
    Serial.printf("[GUI] MQTT port set to %d\n", int_val);
}

static void on_mqtt_ha_confirm(int int_val, float, int) {
    AppState::getInstance().mqttHADiscovery = (int_val != 0);
    saveMqttSettings();
    AppState::getInstance().markSettingsDirty();
    if (int_val && AppState::getInstance().mqttConnected) {
        publishHADiscovery();
    }
    Serial.printf("[GUI] HA Discovery %s\n", int_val ? "enabled" : "disabled");
}

/* ===== Keyboard callbacks ===== */

static void on_broker_done(const char *text) {
    AppState::getInstance().mqttBroker = text;
    saveMqttSettings();
    AppState::getInstance().markSettingsDirty();
    Serial.printf("[GUI] MQTT broker set to %s\n", text);
}

static void on_username_done(const char *text) {
    AppState::getInstance().mqttUsername = text;
    saveMqttSettings();
    AppState::getInstance().markSettingsDirty();
    Serial.printf("[GUI] MQTT username set\n");
}

static void on_password_done(const char *text) {
    AppState::getInstance().mqttPassword = text;
    saveMqttSettings();
    AppState::getInstance().markSettingsDirty();
    Serial.printf("[GUI] MQTT password set\n");
}

static void on_topic_done(const char *text) {
    AppState::getInstance().mqttBaseTopic = text;
    saveMqttSettings();
    AppState::getInstance().markSettingsDirty();
    Serial.printf("[GUI] MQTT base topic set to %s\n", text);
}

/* ===== Menu action callbacks ===== */

static void edit_mqtt_enable(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Enable MQTT";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = AppState::getInstance().mqttEnabled;
    cfg.on_confirm = on_mqtt_enable_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_mqtt_broker(void) {
    KeyboardConfig cfg = {};
    cfg.title = "MQTT Broker";
    cfg.initial_text = AppState::getInstance().mqttBroker.c_str();
    cfg.password_mode = false;
    cfg.on_done = on_broker_done;
    scr_keyboard_open(&cfg);
}

static void edit_mqtt_port(void) {
    ValueEditConfig cfg = {};
    cfg.title = "MQTT Port";
    cfg.type = VE_NUMERIC;
    cfg.int_val = AppState::getInstance().mqttPort;
    cfg.int_min = 1;
    cfg.int_max = 65535;
    cfg.int_step = 1;
    cfg.on_confirm = on_mqtt_port_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_mqtt_username(void) {
    KeyboardConfig cfg = {};
    cfg.title = "MQTT Username";
    cfg.initial_text = AppState::getInstance().mqttUsername.c_str();
    cfg.password_mode = false;
    cfg.on_done = on_username_done;
    scr_keyboard_open(&cfg);
}

static void edit_mqtt_password(void) {
    KeyboardConfig cfg = {};
    cfg.title = "MQTT Password";
    cfg.initial_text = AppState::getInstance().mqttPassword.c_str();
    cfg.password_mode = true;
    cfg.on_done = on_password_done;
    scr_keyboard_open(&cfg);
}

static void edit_mqtt_topic(void) {
    KeyboardConfig cfg = {};
    cfg.title = "Base Topic";
    cfg.initial_text = AppState::getInstance().mqttBaseTopic.c_str();
    cfg.password_mode = false;
    cfg.on_done = on_topic_done;
    scr_keyboard_open(&cfg);
}

static void edit_mqtt_ha(void) {
    ValueEditConfig cfg = {};
    cfg.title = "HA Discovery";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = AppState::getInstance().mqttHADiscovery;
    cfg.on_confirm = on_mqtt_ha_confirm;
    scr_value_edit_open(&cfg);
}

/* ===== Build the MQTT menu ===== */
static MenuConfig mqtt_menu;

static void build_mqtt_menu(void) {
    AppState &st = AppState::getInstance();

    static char enable_str[8];
    snprintf(enable_str, sizeof(enable_str), "%s", st.mqttEnabled ? "ON" : "OFF");

    static char status_str[24];
    if (!st.mqttEnabled) {
        snprintf(status_str, sizeof(status_str), "Disabled");
    } else if (st.mqttConnected) {
        snprintf(status_str, sizeof(status_str), "Connected");
    } else {
        snprintf(status_str, sizeof(status_str), "Disconnected");
    }

    static char broker_str[24];
    if (st.mqttBroker.length() > 0) {
        snprintf(broker_str, sizeof(broker_str), "%.20s", st.mqttBroker.c_str());
    } else {
        snprintf(broker_str, sizeof(broker_str), "(not set)");
    }

    static char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", st.mqttPort);

    static char user_str[16];
    if (st.mqttUsername.length() > 0) {
        snprintf(user_str, sizeof(user_str), "%.12s", st.mqttUsername.c_str());
    } else {
        snprintf(user_str, sizeof(user_str), "(none)");
    }

    static char ha_str[8];
    snprintf(ha_str, sizeof(ha_str), "%s", st.mqttHADiscovery ? "ON" : "OFF");

    mqtt_menu.title = "MQTT";
    mqtt_menu.item_count = 9;
    mqtt_menu.items[0] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};
    mqtt_menu.items[1] = {"Status", status_str, ICON_MQTT, MENU_INFO, nullptr};
    mqtt_menu.items[2] = {"Enable MQTT", enable_str, nullptr, MENU_ACTION, edit_mqtt_enable};
    mqtt_menu.items[3] = {"Broker", broker_str, nullptr, MENU_ACTION, edit_mqtt_broker};
    mqtt_menu.items[4] = {"Port", port_str, nullptr, MENU_ACTION, edit_mqtt_port};
    mqtt_menu.items[5] = {"Username", user_str, nullptr, MENU_ACTION, edit_mqtt_username};
    mqtt_menu.items[6] = {"Password", "***", nullptr, MENU_ACTION, edit_mqtt_password};
    mqtt_menu.items[7] = {"Base Topic", nullptr, nullptr, MENU_ACTION, edit_mqtt_topic};
    mqtt_menu.items[8] = {"HA Discovery", ha_str, nullptr, MENU_ACTION, edit_mqtt_ha};
}

lv_obj_t *scr_mqtt_create(void) {
    build_mqtt_menu();
    return scr_menu_create(&mqtt_menu);
}

#endif /* GUI_ENABLED */
