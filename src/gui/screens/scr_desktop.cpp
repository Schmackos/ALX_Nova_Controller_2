#ifdef GUI_ENABLED

#include "scr_desktop.h"
#include "../gui_config.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../gui_theme.h"
#include "../../app_state.h"
#include "../../config.h"
#include "../../debug_serial.h"
#include <Arduino.h>
#include <WiFi.h>

/* Number of dashboard cards */
#define CARD_COUNT 7

/* Card definitions */
struct CardDef {
    const char *icon;
    const char *title;
    ScreenId target_screen;
};

static const CardDef cards[CARD_COUNT] = {
    {ICON_HOME,     "Home",     SCR_HOME},
    {ICON_CONTROL,  "Control",  SCR_CONTROL_MENU},
    {ICON_WIFI,     "WiFi",     SCR_WIFI_MENU},
    {ICON_MQTT,     "MQTT",     SCR_MQTT_MENU},
    {ICON_SETTINGS, "Settings", SCR_SETTINGS_MENU},
    {ICON_SUPPORT,  "Support",  SCR_SUPPORT_MENU},
    {ICON_DEBUG,    "Debug",    SCR_DEBUG_MENU},
};

/* References to summary labels for live updates */
static lv_obj_t *summary_labels[CARD_COUNT] = {nullptr};
static lv_obj_t *dot_indicators[CARD_COUNT] = {nullptr};
static lv_obj_t *desktop_tileview = nullptr;

/* Build summary text for each card */
static void get_card_summary(int idx, char *buf, size_t len) {
    AppState &st = AppState::getInstance();

    switch (idx) {
        case 0: { /* Home */
            snprintf(buf, len, "Amp: %s\n%s\nFW %s",
                     st.amplifierState ? "ON" : "OFF",
                     WiFi.status() == WL_CONNECTED ? "WiFi OK" :
                     st.isAPMode ? "AP Mode" : "No WiFi",
                     FIRMWARE_VERSION);
            break;
        }
        case 1: { /* Control */
            const char *mode = (st.currentMode == ALWAYS_ON) ? "Always On" :
                               (st.currentMode == ALWAYS_OFF) ? "Always Off" : "Smart Auto";
            snprintf(buf, len, "%s\nAmp: %s\n%+.0f dBFS",
                     mode, st.amplifierState ? "ON" : "OFF", st.audioLevel_dBFS);
            break;
        }
        case 2: { /* WiFi */
            if (WiFi.status() == WL_CONNECTED) {
                snprintf(buf, len, "%s\n%s\n%ddBm",
                         WiFi.SSID().c_str(),
                         WiFi.localIP().toString().c_str(),
                         WiFi.RSSI());
            } else if (st.isAPMode) {
                snprintf(buf, len, "AP Mode\n%s", st.apSSID.c_str());
            } else {
                snprintf(buf, len, "Disconnected");
            }
            break;
        }
        case 3: { /* MQTT */
            if (!st.mqttEnabled) {
                snprintf(buf, len, "Disabled");
            } else if (st.mqttConnected) {
                snprintf(buf, len, "Connected\n%s:%d", st.mqttBroker.c_str(), st.mqttPort);
            } else {
                snprintf(buf, len, "Disconnected\n%s", st.mqttBroker.c_str());
            }
            break;
        }
        case 4: { /* Settings */
            snprintf(buf, len, "FW %s\n%s mode",
                     FIRMWARE_VERSION,
                     st.nightMode ? "Night" : "Day");
            break;
        }
        case 5: { /* Support */
            snprintf(buf, len, "User Manual");
            break;
        }
        case 6: { /* Debug */
            snprintf(buf, len, "Heap: %uKB\nUp: %lus",
                     (unsigned int)(ESP.getFreeHeap() / 1024),
                     millis() / 1000);
            break;
        }
        default:
            snprintf(buf, len, "---");
    }
}

/* Event handler for card press (encoder click) */
static void card_click_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    LOG_D("[GUI] Desktop card %d clicked", idx);
    if (idx >= 0 && idx < CARD_COUNT) {
        gui_nav_push(cards[idx].target_screen);
    }
}

/* Focus handler — scroll tileview to show the focused card */
static void card_focus_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (desktop_tileview && idx >= 0 && idx < CARD_COUNT) {
        lv_tileview_set_tile_by_index(desktop_tileview, idx, 0, LV_ANIM_ON);
        for (int i = 0; i < CARD_COUNT; i++) {
            if (dot_indicators[i]) {
                lv_obj_set_style_bg_color(dot_indicators[i],
                    (i == idx) ? COLOR_PRIMARY : COLOR_TEXT_DIM, LV_PART_MAIN);
            }
        }
        gui_nav_set_focus_index(idx);
        LOG_D("[GUI] Desktop focus -> card %d", idx);
    }
}

lv_obj_t *scr_desktop_create(void) {
    /* Reset static pointers — previous screen objects were auto-deleted */
    memset(summary_labels, 0, sizeof(summary_labels));
    memset(dot_indicators, 0, sizeof(dot_indicators));
    desktop_tileview = nullptr;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, gui_style_screen(), LV_PART_MAIN);

    /* Create a tileview for horizontal scrolling */
    desktop_tileview = lv_tileview_create(scr);
    lv_obj_set_size(desktop_tileview, DISPLAY_HEIGHT, DISPLAY_WIDTH); /* 160x128 landscape */
    lv_obj_set_style_bg_opa(desktop_tileview, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(desktop_tileview, LV_SCROLLBAR_MODE_OFF);

    /* Create one tile per card (horizontal arrangement) */
    for (int i = 0; i < CARD_COUNT; i++) {
        lv_obj_t *tile = lv_tileview_add_tile(desktop_tileview, i, 0, LV_DIR_HOR);

        /* Card container */
        lv_obj_t *card = lv_obj_create(tile);
        lv_obj_set_size(card, 140, 108);
        lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_style(card, gui_style_card(), LV_PART_MAIN);
        lv_obj_add_style(card, gui_style_card_focused(), LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        /* Make card clickable for encoder */
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, card_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(card, card_focus_cb, LV_EVENT_FOCUSED, (void *)(intptr_t)i);
        lv_group_add_obj(gui_nav_get_group(), card);

        /* Layout: vertical flex */
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(card, 4, LV_PART_MAIN);

        /* Icon + title row */
        lv_obj_t *header = lv_obj_create(card);
        lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(header, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(header, 6, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *icon_lbl = lv_label_create(header);
        lv_label_set_text(icon_lbl, cards[i].icon);
        lv_obj_add_style(icon_lbl, gui_style_title(), LV_PART_MAIN);

        lv_obj_t *title_lbl = lv_label_create(header);
        lv_label_set_text(title_lbl, cards[i].title);
        lv_obj_add_style(title_lbl, gui_style_title(), LV_PART_MAIN);

        /* Separator line */
        lv_obj_t *line = lv_obj_create(card);
        lv_obj_set_size(line, LV_PCT(100), 1);
        lv_obj_set_style_bg_color(line, COLOR_PRIMARY, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(line, LV_OPA_60, LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(line, 0, LV_PART_MAIN);

        /* Summary text */
        lv_obj_t *summary = lv_label_create(card);
        lv_obj_add_style(summary, gui_style_dim(), LV_PART_MAIN);
        lv_obj_set_width(summary, LV_PCT(100));

        char buf[64];
        get_card_summary(i, buf, sizeof(buf));
        lv_label_set_text(summary, buf);

        summary_labels[i] = summary;
    }

    /* Page indicator dots at the bottom */
    lv_obj_t *dots = lv_obj_create(scr);
    lv_obj_set_size(dots, DISPLAY_HEIGHT, 12);
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dots, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(dots, 0, LV_PART_MAIN);
    lv_obj_clear_flag(dots, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < CARD_COUNT; i++) {
        lv_obj_t *dot = lv_obj_create(dots);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_radius(dot, 3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, (i == 0) ? COLOR_PRIMARY : COLOR_TEXT_DIM, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
        dot_indicators[i] = dot;
    }

    LOG_D("[GUI] Desktop carousel created");
    return scr;
}

void scr_desktop_refresh(void) {
    for (int i = 0; i < CARD_COUNT; i++) {
        if (summary_labels[i] != nullptr) {
            char buf[64];
            get_card_summary(i, buf, sizeof(buf));
            lv_label_set_text(summary_labels[i], buf);
        }
    }
}

#endif /* GUI_ENABLED */
