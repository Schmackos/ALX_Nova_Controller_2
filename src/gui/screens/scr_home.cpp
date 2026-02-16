#ifdef GUI_ENABLED

#include "scr_home.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../gui_theme.h"
#include "../../app_state.h"
#include "../../config.h"
#include "../../debug_serial.h"
#include <Arduino.h>
#include <WiFi.h>

/* Layout constants for 160x128 display */
#define TITLE_H     18
#define ROW_H       30
#define ROW_GAP     1
#define COL_GAP     2
#define SIDE_PAD    2
#define CELL_W      77
#define CELL_PAD    3
#define DOT_SIZE    6
#define BACK_BTN_H  16

/* Widget handles (reset in scr_home_create) */
static lv_obj_t *lbl_title = nullptr;
static lv_obj_t *lbl_update_icon = nullptr;
static lv_obj_t *lbl_amp_value = nullptr;
static lv_obj_t *dot_amp = nullptr;
static lv_obj_t *lbl_sig_value = nullptr;
static lv_obj_t *dot_sig = nullptr;
static lv_obj_t *lbl_wifi_value = nullptr;
static lv_obj_t *dot_wifi = nullptr;
static lv_obj_t *lbl_mqtt_value = nullptr;
static lv_obj_t *dot_mqtt = nullptr;
static lv_obj_t *lbl_mode_value = nullptr;
static lv_obj_t *bar_level = nullptr;

/* Back button callback */
static void on_back(lv_event_t *e) {
    (void)e;
    gui_nav_pop_deferred();
}

/* Set dot color helper */
static void set_dot_color(lv_obj_t *dot, lv_color_t color) {
    if (dot) {
        lv_obj_set_style_bg_color(dot, color, LV_PART_MAIN);
    }
}

/**
 * Create a dashboard cell at absolute position (x, y).
 * Returns: value label via *value_out, status dot via *dot_out (either can be NULL).
 */
static void create_cell(lv_obj_t *parent, int x, int y,
                         const char *icon_str, const char *title_str,
                         lv_obj_t **value_out, lv_obj_t **dot_out) {
    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_set_pos(cell, x, y);
    lv_obj_set_size(cell, CELL_W, ROW_H);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    /* Cell styling — smaller than gui_style_card for compact cells */
    lv_obj_set_style_bg_color(cell, COLOR_BG_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(cell, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cell, CELL_PAD, LV_PART_MAIN);
    lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cell, COLOR_BORDER_DARK, LV_PART_MAIN);

    /* Header row: icon + title */
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
    /* Offset title to the right of icon (icon ~10px + 2px gap) */
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, icon_str ? 12 : 0, 0);

    /* Value label */
    lv_obj_t *val = lv_label_create(cell);
    lv_label_set_text(val, "---");
    lv_obj_set_style_text_font(val, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, COLOR_TEXT_PRI, LV_PART_MAIN);
    lv_obj_align(val, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    if (value_out) *value_out = val;

    /* Status dot (6x6 circle) */
    if (dot_out) {
        lv_obj_t *dot = lv_obj_create(cell);
        lv_obj_set_size(dot, DOT_SIZE, DOT_SIZE);
        lv_obj_set_style_radius(dot, DOT_SIZE / 2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, COLOR_TEXT_DIM, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(dot, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        *dot_out = dot;
    }
}

lv_obj_t *scr_home_create(void) {
    /* Reset static pointers */
    lbl_title = nullptr;
    lbl_update_icon = nullptr;
    lbl_amp_value = nullptr;
    dot_amp = nullptr;
    lbl_sig_value = nullptr;
    dot_sig = nullptr;
    lbl_wifi_value = nullptr;
    dot_wifi = nullptr;
    lbl_mqtt_value = nullptr;
    dot_mqtt = nullptr;
    lbl_mode_value = nullptr;
    bar_level = nullptr;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, gui_style_screen(), LV_PART_MAIN);

    /* === Title bar === */
    lbl_title = lv_label_create(scr);
    lv_label_set_text(lbl_title, "ALX Nova v" FIRMWARE_VERSION);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_title, COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, SIDE_PAD, 3);

    lbl_update_icon = lv_label_create(scr);
    lv_label_set_text(lbl_update_icon, ICON_DOWNLOAD);
    lv_obj_set_style_text_font(lbl_update_icon, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_update_icon, COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_align(lbl_update_icon, LV_ALIGN_TOP_RIGHT, -SIDE_PAD, 3);

    /* === Grid: 3 rows x 2 columns === */
    int col0_x = SIDE_PAD;
    int col1_x = SIDE_PAD + CELL_W + COL_GAP;
    int row0_y = TITLE_H;
    int row1_y = TITLE_H + ROW_H + ROW_GAP;
    int row2_y = TITLE_H + 2 * (ROW_H + ROW_GAP);

    /* Row 0: Amp + Signal */
    create_cell(scr, col0_x, row0_y, ICON_CONTROL, "Amp",
                &lbl_amp_value, &dot_amp);
    create_cell(scr, col1_x, row0_y, ICON_AUDIO, "Signal",
                &lbl_sig_value, &dot_sig);

    /* Row 1: WiFi + MQTT */
    create_cell(scr, col0_x, row1_y, ICON_WIFI, "WiFi",
                &lbl_wifi_value, &dot_wifi);
    create_cell(scr, col1_x, row1_y, ICON_MQTT, "MQTT",
                &lbl_mqtt_value, &dot_mqtt);

    /* Row 2: Mode (no dot) + Level (bar instead of dot) */
    create_cell(scr, col0_x, row2_y, ICON_SETTINGS, "Mode",
                &lbl_mode_value, NULL);

    /* Level cell — custom with bar */
    {
        lv_obj_t *cell = lv_obj_create(scr);
        lv_obj_set_pos(cell, col1_x, row2_y);
        lv_obj_set_size(cell, CELL_W, ROW_H);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(cell, COLOR_BG_CARD, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(cell, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_all(cell, CELL_PAD, LV_PART_MAIN);
        lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(cell, COLOR_BORDER_DARK, LV_PART_MAIN);

        lv_obj_t *icon = lv_label_create(cell);
        lv_label_set_text(icon, ICON_LEVEL);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(icon, COLOR_PRIMARY, LV_PART_MAIN);
        lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *title = lv_label_create(cell);
        lv_label_set_text(title, "Level");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, COLOR_TEXT_SEC, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 0);

        bar_level = lv_bar_create(cell);
        lv_obj_set_size(bar_level, CELL_W - 2 * CELL_PAD - 2, 8);
        lv_bar_set_range(bar_level, -96, 0);
        lv_bar_set_value(bar_level, -96, LV_ANIM_OFF);
        lv_obj_align(bar_level, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(bar_level, COLOR_BG_SURFACE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar_level, COLOR_SUCCESS, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(bar_level, LV_OPA_COVER, LV_PART_INDICATOR);
    }

    /* === Back button === */
    lv_obj_t *back_btn = lv_obj_create(scr);
    lv_obj_set_size(back_btn, 60, BACK_BTN_H);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_add_style(back_btn, gui_style_list_item(), LV_PART_MAIN);
    lv_obj_add_style(back_btn, gui_style_list_item_focused(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_group_add_obj(gui_nav_get_group(), back_btn);

    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, ICON_BACK " Back");
    lv_obj_set_style_text_color(back_lbl, COLOR_TEXT_SEC, LV_PART_MAIN);
    lv_obj_add_style(back_lbl, gui_style_dim(), LV_PART_MAIN);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, on_back, LV_EVENT_CLICKED, NULL);

    /* Initial data fill */
    scr_home_refresh();

    LOG_D("[GUI] Home dashboard created");
    return scr;
}

void scr_home_refresh(void) {
    AppState &st = AppState::getInstance();
    char buf[32];

    /* Update icon: orange when update available, dim when up to date */
    if (lbl_update_icon) {
        lv_obj_set_style_text_color(lbl_update_icon,
            st.appState.updateAvailable ? COLOR_PRIMARY : COLOR_TEXT_DIM, LV_PART_MAIN);
    }

    /* Amp: ON/OFF + dot green/red */
    if (lbl_amp_value) {
        lv_label_set_text(lbl_amp_value, st.appState.amplifierState ? "ON" : "OFF");
    }
    set_dot_color(dot_amp, st.appState.amplifierState ? COLOR_SUCCESS : COLOR_ERROR);

    /* Signal: dBFS value + dot green/gray */
    if (lbl_sig_value) {
        bool detected = st.appState.audioLevel_dBFS >= st.appState.audioThreshold_dBFS;
        snprintf(buf, sizeof(buf), "%+.0f dBFS", st.appState.audioLevel_dBFS);
        lv_label_set_text(lbl_sig_value, buf);
        set_dot_color(dot_sig, detected ? COLOR_SUCCESS : COLOR_TEXT_DIM);
    }

    /* WiFi: status text + dot green/orange/red */
    if (lbl_wifi_value) {
        if (WiFi.status() == WL_CONNECTED) {
            lv_label_set_text(lbl_wifi_value, "Connected");
            set_dot_color(dot_wifi, COLOR_SUCCESS);
        } else if (st.appState.isAPMode) {
            lv_label_set_text(lbl_wifi_value, "AP Mode");
            set_dot_color(dot_wifi, COLOR_PRIMARY);
        } else {
            lv_label_set_text(lbl_wifi_value, "Disconnected");
            set_dot_color(dot_wifi, COLOR_ERROR);
        }
    }

    /* MQTT: status text + dot green/red/gray */
    if (lbl_mqtt_value) {
        if (!st.appState.mqttEnabled) {
            lv_label_set_text(lbl_mqtt_value, "Disabled");
            set_dot_color(dot_mqtt, COLOR_TEXT_DIM);
        } else if (st.appState.mqttConnected) {
            lv_label_set_text(lbl_mqtt_value, "Connected");
            set_dot_color(dot_mqtt, COLOR_SUCCESS);
        } else {
            lv_label_set_text(lbl_mqtt_value, "Disconnected");
            set_dot_color(dot_mqtt, COLOR_ERROR);
        }
    }

    /* Mode: alternates with timer countdown when Smart Auto + timer active */
    if (lbl_mode_value) {
        if (st.appState.currentMode == SMART_AUTO && st.appState.timerRemaining > 0 &&
            (millis() / 3000) % 2 == 1) {
            unsigned long mins = st.appState.timerRemaining / 60;
            unsigned long secs = st.appState.timerRemaining % 60;
            snprintf(buf, sizeof(buf), "%02lu:%02lu", mins, secs);
            lv_label_set_text(lbl_mode_value, buf);
        } else {
            const char *mode = (st.appState.currentMode == ALWAYS_ON)  ? "Always On" :
                               (st.appState.currentMode == ALWAYS_OFF) ? "Always Off" : "Smart Auto";
            lv_label_set_text(lbl_mode_value, mode);
        }
    }

    /* Level bar: VU combined, indicator green when signal detected */
    if (bar_level) {
        int vu = (int)st.audioVuCombined;
        if (vu < -96) vu = -96;
        if (vu > 0) vu = 0;
        lv_bar_set_value(bar_level, vu, LV_ANIM_ON);

        bool detected = st.appState.audioLevel_dBFS >= st.appState.audioThreshold_dBFS;
        lv_obj_set_style_bg_color(bar_level,
            detected ? COLOR_SUCCESS : COLOR_TEXT_DIM, LV_PART_INDICATOR);
    }
}

#endif /* GUI_ENABLED */
