#ifdef GUI_ENABLED

#include "scr_desktop.h"
#include "../gui_config.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../gui_theme.h"
#include "../../app_state.h"
#include "../../config.h"
#include "../../debug_serial.h"
#ifdef DSP_ENABLED
#include "../../dsp_pipeline.h"
#endif
#include <Arduino.h>
#include <WiFi.h>

/* Number of dashboard cards */
#ifdef DSP_ENABLED
#define CARD_COUNT 8
#else
#define CARD_COUNT 7
#endif

/* Compact dashboard cell constants (for Home card 0) */
#define DASH_CELL_W    66
#define DASH_CELL_H    26
#define DASH_CELL_PAD  2
#define DASH_DOT_SIZE  5
#define DASH_COL_GAP   3
#define DASH_ROW_GAP   1
#define DASH_TITLE_H   13

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
#ifdef DSP_ENABLED
    {ICON_DSP,      "DSP",      SCR_DSP_MENU},
#endif
};

/* References to summary labels for live updates (cards 1-6) */
static lv_obj_t *summary_labels[CARD_COUNT] = {nullptr};
static lv_obj_t *dot_indicators[CARD_COUNT] = {nullptr};
static lv_obj_t *desktop_tileview = nullptr;

/* Home dashboard widget handles (card 0) */
static lv_obj_t *dash_update_icon = nullptr;
static lv_obj_t *dash_amp_value = nullptr;
static lv_obj_t *dash_amp_dot = nullptr;
static lv_obj_t *dash_sig_value = nullptr;
static lv_obj_t *dash_sig_dot = nullptr;
static lv_obj_t *dash_wifi_value = nullptr;
static lv_obj_t *dash_wifi_dot = nullptr;
static lv_obj_t *dash_mqtt_value = nullptr;
static lv_obj_t *dash_mqtt_dot = nullptr;
static lv_obj_t *dash_mode_value = nullptr;
static lv_obj_t *dash_level_bar = nullptr;

/* Build summary text for each card (cards 1-6) */
static void get_card_summary(int idx, char *buf, size_t len) {
    AppState &st = AppState::getInstance();

    switch (idx) {
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
                     st.darkMode ? "Dark" : "Light");
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
#ifdef DSP_ENABLED
        case 7: { /* DSP */
            DspMetrics m = dsp_get_metrics();
            snprintf(buf, len, "%s%s\nCPU: %.0f%%",
                     st.dspEnabled ? "Enabled" : "Disabled",
                     st.dspBypass ? " (BYP)" : "",
                     m.cpuLoadPercent);
            break;
        }
#endif
        default:
            snprintf(buf, len, "---");
    }
}

/* Create a compact dashboard cell at position (x, y) within a card */
static void create_compact_cell(lv_obj_t *parent, int x, int y,
                                const char *icon_str, const char *title_str,
                                lv_obj_t **value_out, lv_obj_t **dot_out) {
    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_set_pos(cell, x, y);
    lv_obj_set_size(cell, DASH_CELL_W, DASH_CELL_H);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(cell, COLOR_BG_SURFACE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(cell, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cell, DASH_CELL_PAD, LV_PART_MAIN);
    lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cell, COLOR_BORDER_DARK, LV_PART_MAIN);

    if (icon_str) {
        lv_obj_t *icon = lv_label_create(cell);
        lv_label_set_text(icon, icon_str);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(icon, COLOR_PRIMARY, LV_PART_MAIN);
        lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0);
    }

    lv_obj_t *title = lv_label_create(cell);
    lv_label_set_text(title, title_str);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, COLOR_TEXT_SEC, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, icon_str ? 11 : 0, 0);

    lv_obj_t *val = lv_label_create(cell);
    lv_label_set_text(val, "---");
    lv_obj_set_style_text_font(val, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, COLOR_TEXT_PRI, LV_PART_MAIN);
    lv_obj_align(val, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    if (value_out) *value_out = val;

    if (dot_out) {
        lv_obj_t *dot = lv_obj_create(cell);
        lv_obj_set_size(dot, DASH_DOT_SIZE, DASH_DOT_SIZE);
        lv_obj_set_style_radius(dot, DASH_DOT_SIZE / 2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, COLOR_TEXT_DIM, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(dot, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        *dot_out = dot;
    }
}

/* Build the Home dashboard grid inside card 0 */
static void build_home_dashboard(lv_obj_t *card) {
    /* Title bar: "ALX Nova v1.5.3" + download icon */
    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "ALX Nova v" FIRMWARE_VERSION);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    dash_update_icon = lv_label_create(card);
    lv_label_set_text(dash_update_icon, ICON_DOWNLOAD);
    lv_obj_set_style_text_font(dash_update_icon, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(dash_update_icon, COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_align(dash_update_icon, LV_ALIGN_TOP_RIGHT, 0, 0);

    /* Grid positions within card content area */
    int col0_x = 0;
    int col1_x = DASH_CELL_W + DASH_COL_GAP;
    int row0_y = DASH_TITLE_H;
    int row1_y = DASH_TITLE_H + DASH_CELL_H + DASH_ROW_GAP;
    int row2_y = DASH_TITLE_H + 2 * (DASH_CELL_H + DASH_ROW_GAP);

    /* Row 0: Amp + Signal */
    create_compact_cell(card, col0_x, row0_y, ICON_CONTROL, "Amp",
                        &dash_amp_value, &dash_amp_dot);
    create_compact_cell(card, col1_x, row0_y, ICON_AUDIO, "Signal",
                        &dash_sig_value, &dash_sig_dot);

    /* Row 1: WiFi + MQTT */
    create_compact_cell(card, col0_x, row1_y, ICON_WIFI, "WiFi",
                        &dash_wifi_value, &dash_wifi_dot);
    create_compact_cell(card, col1_x, row1_y, ICON_MQTT, "MQTT",
                        &dash_mqtt_value, &dash_mqtt_dot);

    /* Row 2: Mode (no dot) + Level (bar) */
    create_compact_cell(card, col0_x, row2_y, ICON_SETTINGS, "Mode",
                        &dash_mode_value, NULL);

    /* Level cell — custom with bar */
    {
        lv_obj_t *cell = lv_obj_create(card);
        lv_obj_set_pos(cell, col1_x, row2_y);
        lv_obj_set_size(cell, DASH_CELL_W, DASH_CELL_H);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(cell, COLOR_BG_SURFACE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(cell, 3, LV_PART_MAIN);
        lv_obj_set_style_pad_all(cell, DASH_CELL_PAD, LV_PART_MAIN);
        lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(cell, COLOR_BORDER_DARK, LV_PART_MAIN);

        lv_obj_t *icon = lv_label_create(cell);
        lv_label_set_text(icon, ICON_LEVEL);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(icon, COLOR_PRIMARY, LV_PART_MAIN);
        lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *lbl = lv_label_create(cell);
        lv_label_set_text(lbl, "Level");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_SEC, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 11, 0);

        dash_level_bar = lv_bar_create(cell);
        lv_obj_set_size(dash_level_bar, DASH_CELL_W - 2 * DASH_CELL_PAD - 2, 6);
        lv_bar_set_range(dash_level_bar, -96, 0);
        lv_bar_set_value(dash_level_bar, -96, LV_ANIM_OFF);
        lv_obj_align(dash_level_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(dash_level_bar, COLOR_BG_SURFACE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dash_level_bar, COLOR_SUCCESS, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(dash_level_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    }
}

/* Refresh Home dashboard widgets (card 0) */
static void set_dash_dot(lv_obj_t *dot, lv_color_t color) {
    if (dot) lv_obj_set_style_bg_color(dot, color, LV_PART_MAIN);
}

static void refresh_dashboard(void) {
    AppState &st = AppState::getInstance();
    char buf[32];

    if (dash_update_icon) {
        lv_obj_set_style_text_color(dash_update_icon,
            st.updateAvailable ? COLOR_PRIMARY : COLOR_TEXT_DIM, LV_PART_MAIN);
    }

    if (dash_amp_value) {
        lv_label_set_text(dash_amp_value, st.amplifierState ? "ON" : "OFF");
    }
    set_dash_dot(dash_amp_dot, st.amplifierState ? COLOR_SUCCESS : COLOR_ERROR);

    if (dash_sig_value) {
        bool detected = st.audioLevel_dBFS >= st.audioThreshold_dBFS;
        snprintf(buf, sizeof(buf), "%+.0f dBFS", st.audioLevel_dBFS);
        lv_label_set_text(dash_sig_value, buf);
        set_dash_dot(dash_sig_dot, detected ? COLOR_SUCCESS : COLOR_TEXT_DIM);
    }

    if (dash_wifi_value) {
        if (WiFi.status() == WL_CONNECTED) {
            lv_label_set_text(dash_wifi_value, "Connected");
            set_dash_dot(dash_wifi_dot, COLOR_SUCCESS);
        } else if (st.isAPMode) {
            lv_label_set_text(dash_wifi_value, "AP Mode");
            set_dash_dot(dash_wifi_dot, COLOR_PRIMARY);
        } else {
            lv_label_set_text(dash_wifi_value, "Disconnected");
            set_dash_dot(dash_wifi_dot, COLOR_ERROR);
        }
    }

    if (dash_mqtt_value) {
        if (!st.mqttEnabled) {
            lv_label_set_text(dash_mqtt_value, "Disabled");
            set_dash_dot(dash_mqtt_dot, COLOR_TEXT_DIM);
        } else if (st.mqttConnected) {
            lv_label_set_text(dash_mqtt_value, "Connected");
            set_dash_dot(dash_mqtt_dot, COLOR_SUCCESS);
        } else {
            lv_label_set_text(dash_mqtt_value, "Disconnected");
            set_dash_dot(dash_mqtt_dot, COLOR_ERROR);
        }
    }

    if (dash_mode_value) {
        if (st.currentMode == SMART_AUTO && st.timerRemaining > 0 &&
            (millis() / 3000) % 2 == 1) {
            unsigned long mins = st.timerRemaining / 60;
            unsigned long secs = st.timerRemaining % 60;
            snprintf(buf, sizeof(buf), "%02lu:%02lu", mins, secs);
            lv_label_set_text(dash_mode_value, buf);
        } else {
            const char *mode = (st.currentMode == ALWAYS_ON) ? "Always On" :
                               (st.currentMode == ALWAYS_OFF) ? "Always Off" : "Smart Auto";
            lv_label_set_text(dash_mode_value, mode);
        }
    }

    if (dash_level_bar) {
        int vu = (int)st.audioVuCombined;
        if (vu < -96) vu = -96;
        if (vu > 0) vu = 0;
        lv_bar_set_value(dash_level_bar, vu, LV_ANIM_ON);
        bool detected = st.audioLevel_dBFS >= st.audioThreshold_dBFS;
        lv_obj_set_style_bg_color(dash_level_bar,
            detected ? COLOR_SUCCESS : COLOR_TEXT_DIM, LV_PART_INDICATOR);
    }
}

/* Event handler for card press (encoder click) */
static void card_click_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx == 0) return; /* Home dashboard — info shown directly on card */
    LOG_D("[GUI] Desktop card %d clicked", idx);
    if (idx >= 0 && idx < CARD_COUNT) {
        gui_nav_push_deferred(cards[idx].target_screen);
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

    /* Reset dashboard widget pointers */
    dash_update_icon = nullptr;
    dash_amp_value = nullptr;
    dash_amp_dot = nullptr;
    dash_sig_value = nullptr;
    dash_sig_dot = nullptr;
    dash_wifi_value = nullptr;
    dash_wifi_dot = nullptr;
    dash_mqtt_value = nullptr;
    dash_mqtt_dot = nullptr;
    dash_mode_value = nullptr;
    dash_level_bar = nullptr;

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
        lv_obj_set_size(card, 148, 114);
        lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_style(card, gui_style_card(), LV_PART_MAIN);
        lv_obj_add_style(card, gui_style_card_focused(), LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        /* Make card clickable for encoder */
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, card_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(card, card_focus_cb, LV_EVENT_FOCUSED, (void *)(intptr_t)i);
        lv_group_add_obj(gui_nav_get_group(), card);

        if (i == 0) {
            /* Home card — 6-cell dashboard grid */
            build_home_dashboard(card);
        } else {
            /* Standard card layout */
            lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_row(card, 2, LV_PART_MAIN);

            /* Icon + title row */
            lv_obj_t *header = lv_obj_create(card);
            lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(header, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(header, 4, LV_PART_MAIN);
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
    }

    /* Page indicator dots at the bottom */
    lv_obj_t *dots = lv_obj_create(scr);
    lv_obj_set_size(dots, DISPLAY_HEIGHT, 10);
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dots, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(dots, 0, LV_PART_MAIN);
    lv_obj_clear_flag(dots, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < CARD_COUNT; i++) {
        lv_obj_t *dot = lv_obj_create(dots);
        lv_obj_set_size(dot, 5, 5);
        lv_obj_set_style_radius(dot, 2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, (i == 0) ? COLOR_PRIMARY : COLOR_TEXT_DIM, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
        dot_indicators[i] = dot;
    }

    /* Initial dashboard fill */
    refresh_dashboard();

    LOG_D("[GUI] Desktop carousel created");
    return scr;
}

void scr_desktop_refresh(void) {
    /* Update Home dashboard (card 0) */
    refresh_dashboard();

    /* Update other card summaries (1-6) */
    for (int i = 1; i < CARD_COUNT; i++) {
        if (summary_labels[i] != nullptr) {
            char buf[64];
            get_card_summary(i, buf, sizeof(buf));
            lv_label_set_text(summary_labels[i], buf);
        }
    }
}

#endif /* GUI_ENABLED */
