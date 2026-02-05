#ifdef GUI_ENABLED

#include "scr_keyboard.h"
#include "../gui_config.h"
#include "../gui_navigation.h"
#include "../gui_theme.h"
#include <Arduino.h>

static KeyboardConfig active_kb_cfg;
static lv_obj_t *kb_textarea = nullptr;

/* Keyboard event handler */
static void kb_event_cb(lv_event_t *e) {
    lv_obj_t *kb = (lv_obj_t *)lv_event_get_target(e);
    lv_keyboard_t *keyboard = (lv_keyboard_t *)kb;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        /* User pressed OK / Enter */
        const char *text = lv_textarea_get_text(kb_textarea);
        if (active_kb_cfg.on_done && text) {
            active_kb_cfg.on_done(text);
        }
        gui_nav_pop();
    } else if (code == LV_EVENT_CANCEL) {
        /* User pressed close */
        gui_nav_pop();
    }
}

static lv_obj_t *create_keyboard_screen(void) {
    /* Reset static pointer — previous screen object was auto-deleted */
    kb_textarea = nullptr;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, gui_style_screen(), LV_PART_MAIN);

    /* Title / prompt */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, active_kb_cfg.title ? active_kb_cfg.title : "Input");
    lv_obj_add_style(title, gui_style_dim(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    /* Text area (top portion of screen) */
    kb_textarea = lv_textarea_create(scr);
    lv_obj_set_size(kb_textarea, DISPLAY_HEIGHT - 8, 28);
    lv_obj_align(kb_textarea, LV_ALIGN_TOP_MID, 0, 16);
    lv_textarea_set_one_line(kb_textarea, true);
    lv_obj_set_style_text_font(kb_textarea, &lv_font_montserrat_12, LV_PART_MAIN);

    if (active_kb_cfg.password_mode) {
        lv_textarea_set_password_mode(kb_textarea, true);
    }
    if (active_kb_cfg.initial_text) {
        lv_textarea_set_text(kb_textarea, active_kb_cfg.initial_text);
    }

    /* LVGL keyboard widget (bottom portion) */
    lv_obj_t *kb = lv_keyboard_create(scr);
    lv_obj_set_size(kb, DISPLAY_HEIGHT, DISPLAY_WIDTH - 48);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, kb_textarea);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_12, LV_PART_ITEMS);

    /* Add to encoder group */
    lv_group_t *grp = gui_nav_get_group();
    lv_group_add_obj(grp, kb);

    /* Handle ready/cancel events */
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_CANCEL, NULL);

    return scr;
}

void scr_keyboard_open(const KeyboardConfig *config) {
    active_kb_cfg = *config;
    gui_nav_register(SCR_KEYBOARD, create_keyboard_screen);
    gui_nav_push(SCR_KEYBOARD);
}

#endif /* GUI_ENABLED */
