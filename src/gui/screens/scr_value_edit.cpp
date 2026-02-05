#ifdef GUI_ENABLED

#include "scr_value_edit.h"
#include "../gui_config.h"
#include "../gui_icons.h"
#include "../gui_input.h"
#include "../gui_navigation.h"
#include "../gui_theme.h"
#include <Arduino.h>

/* Current edit state */
static ValueEditConfig active_cfg;
static int edit_int;
static float edit_float;
static int edit_option_idx;
static bool edit_toggle;

/* UI elements */
static lv_obj_t *value_label = nullptr;
static lv_timer_t *poll_timer = nullptr;

static void update_display(void) {
    char buf[32];

    switch (active_cfg.type) {
        case VE_TOGGLE:
            snprintf(buf, sizeof(buf), "%s", edit_toggle ? "ON" : "OFF");
            break;
        case VE_NUMERIC:
            if (active_cfg.int_unit) {
                snprintf(buf, sizeof(buf), "%d %s", edit_int, active_cfg.int_unit);
            } else {
                snprintf(buf, sizeof(buf), "%d", edit_int);
            }
            break;
        case VE_FLOAT:
            if (active_cfg.float_unit) {
                snprintf(buf, sizeof(buf), "%.*f %s", active_cfg.float_decimals, edit_float, active_cfg.float_unit);
            } else {
                snprintf(buf, sizeof(buf), "%.*f", active_cfg.float_decimals, edit_float);
            }
            break;
        case VE_CYCLE:
            if (edit_option_idx >= 0 && edit_option_idx < active_cfg.option_count) {
                snprintf(buf, sizeof(buf), "%s", active_cfg.options[edit_option_idx].label);
            }
            break;
    }

    if (value_label) {
        lv_label_set_text(value_label, buf);
    }
}

/* Apply encoder rotation diff to the current value */
static void apply_diff(int diff) {
    switch (active_cfg.type) {
        case VE_TOGGLE:
            edit_toggle = !edit_toggle;
            break;
        case VE_NUMERIC:
            edit_int += diff * active_cfg.int_step;
            if (edit_int > active_cfg.int_max) edit_int = active_cfg.int_max;
            if (edit_int < active_cfg.int_min) edit_int = active_cfg.int_min;
            break;
        case VE_FLOAT:
            edit_float += diff * active_cfg.float_step;
            if (edit_float > active_cfg.float_max) edit_float = active_cfg.float_max;
            if (edit_float < active_cfg.float_min) edit_float = active_cfg.float_min;
            break;
        case VE_CYCLE:
            edit_option_idx += diff;
            if (edit_option_idx >= active_cfg.option_count) edit_option_idx = 0;
            if (edit_option_idx < 0) edit_option_idx = active_cfg.option_count - 1;
            break;
    }
}

/* LVGL timer: poll raw encoder diff and update value */
static void poll_encoder_cb(lv_timer_t *t) {
    (void)t;
    int32_t diff = gui_input_get_raw_diff();
    if (diff != 0) {
        Serial.printf("[GUI] Value edit rotate: %d\n", (int)diff);
        apply_diff(diff);
        update_display();
    }
}

/* Clean up raw mode and timer before leaving */
static void cleanup(void) {
    if (poll_timer) {
        lv_timer_delete(poll_timer);
        poll_timer = nullptr;
    }
    gui_input_set_raw_mode(false);
    value_label = nullptr;
}

/* Short click: save value and go back to menu */
static void on_click(lv_event_t *e) {
    (void)e;
    Serial.println("[GUI] Value edit: CONFIRM");
    if (active_cfg.on_confirm) {
        switch (active_cfg.type) {
            case VE_TOGGLE:
                active_cfg.on_confirm(edit_toggle ? 1 : 0, 0.0f, 0);
                break;
            case VE_NUMERIC:
                active_cfg.on_confirm(edit_int, 0.0f, 0);
                break;
            case VE_FLOAT:
                active_cfg.on_confirm(0, edit_float, 0);
                break;
            case VE_CYCLE:
                active_cfg.on_confirm(active_cfg.options[edit_option_idx].value, 0.0f, edit_option_idx);
                break;
        }
    }
    cleanup();
    gui_nav_pop();
}

static lv_obj_t *create_value_edit_screen(void) {
    /* Reset static pointers — previous screen objects were auto-deleted */
    value_label = nullptr;
    if (poll_timer) { lv_timer_delete(poll_timer); poll_timer = nullptr; }

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, gui_style_screen(), LV_PART_MAIN);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, active_cfg.title);
    lv_obj_add_style(title, gui_style_title(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    /* Value display area */
    lv_obj_t *val_container = lv_obj_create(scr);
    lv_obj_set_size(val_container, 140, 40);
    lv_obj_align(val_container, LV_ALIGN_CENTER, 0, -4);
    lv_obj_add_style(val_container, gui_style_card(), LV_PART_MAIN);
    lv_obj_add_style(val_container, gui_style_card_focused(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_clear_flag(val_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(val_container, LV_OBJ_FLAG_CLICKABLE);

    /* Left arrow */
    lv_obj_t *left_arrow = lv_label_create(val_container);
    lv_label_set_text(left_arrow, ICON_BACK);
    lv_obj_set_style_text_color(left_arrow, COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_align(left_arrow, LV_ALIGN_LEFT_MID, 2, 0);

    /* Right arrow */
    lv_obj_t *right_arrow = lv_label_create(val_container);
    lv_label_set_text(right_arrow, ICON_NEXT);
    lv_obj_set_style_text_color(right_arrow, COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_align(right_arrow, LV_ALIGN_RIGHT_MID, -2, 0);

    /* Center value text */
    value_label = lv_label_create(val_container);
    lv_obj_add_style(value_label, gui_style_body(), LV_PART_MAIN);
    lv_obj_set_style_text_color(value_label, COLOR_TEXT_PRI, LV_PART_MAIN);
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, 0);

    /* Add to group — short click confirms */
    lv_group_add_obj(gui_nav_get_group(), val_container);
    lv_obj_add_event_cb(val_container, on_click, LV_EVENT_CLICKED, NULL);

    /* Hint text */
    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Rotate: change  Push: save");
    lv_obj_add_style(hint, gui_style_dim(), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);

    /* Start polling encoder rotation via raw mode */
    poll_timer = lv_timer_create(poll_encoder_cb, 30, NULL);

    update_display();
    return scr;
}

void scr_value_edit_open(const ValueEditConfig *config) {
    /* Copy config */
    active_cfg = *config;

    /* Initialize edit state */
    switch (config->type) {
        case VE_TOGGLE:
            edit_toggle = config->toggle_val;
            break;
        case VE_NUMERIC:
            edit_int = config->int_val;
            break;
        case VE_FLOAT:
            edit_float = config->float_val;
            break;
        case VE_CYCLE:
            edit_option_idx = config->current_option;
            break;
    }

    Serial.printf("[GUI] Value edit open: %s\n", config->title);

    /* Enable raw mode: rotation goes directly to value editor, not LVGL navigation */
    gui_input_set_raw_mode(true);

    /* Register temporary screen creator and push */
    gui_nav_register(SCR_VALUE_EDIT, create_value_edit_screen);
    gui_nav_push(SCR_VALUE_EDIT);
}

#endif /* GUI_ENABLED */
