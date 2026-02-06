#ifdef GUI_ENABLED

#include "scr_menu.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../gui_theme.h"
#include "../gui_config.h"
#include <Arduino.h>

/* Track value labels for dynamic updates */
static lv_obj_t *value_labels[MENU_MAX_ITEMS] = {nullptr};
static int current_item_count = 0;

/* Back button callback */
static void back_cb(lv_event_t *e) {
    (void)e;
    gui_nav_pop();
}

/* Scroll focused row into view and report focus index */
static void row_focus_cb(lv_event_t *e) {
    lv_obj_t *row = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_scroll_to_view(row, LV_ANIM_ON);
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    gui_nav_set_focus_index(idx);
}

/* Generic menu item click callback */
static void item_click_cb(lv_event_t *e) {
    menu_action_fn action = (menu_action_fn)lv_event_get_user_data(e);
    Serial.printf("[GUI] Menu item clicked (action=%p)\n", (void *)action);
    if (action) {
        action();
    }
}

lv_obj_t *scr_menu_create(const MenuConfig *config) {
    current_item_count = 0;
    memset(value_labels, 0, sizeof(value_labels));

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, gui_style_screen(), LV_PART_MAIN);

    /* Title bar */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, config->title);
    lv_obj_add_style(title, gui_style_title(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    /* Scrollable list container */
    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, DISPLAY_HEIGHT, DISPLAY_WIDTH - 28); /* Below title */
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    lv_group_t *grp = gui_nav_get_group();

    for (int i = 0; i < config->item_count && i < MENU_MAX_ITEMS; i++) {
        const MenuItem *item = &config->items[i];

        /* Create row container */
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_style(row, gui_style_list_item(), LV_PART_MAIN);
        lv_obj_add_style(row, gui_style_list_item_focused(), LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_pad_hor(row, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(row, 6, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        /* Left side: icon + label */
        lv_obj_t *left = lv_obj_create(row);
        lv_obj_set_height(left, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(left, 1);
        lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(left, 4, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(left, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(left, 0, LV_PART_MAIN);
        lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

        if (item->icon) {
            lv_obj_t *icon = lv_label_create(left);
            lv_label_set_text(icon, item->icon);
            if (item->type == MENU_BACK) {
                lv_obj_set_style_text_color(icon, COLOR_TEXT_SEC, LV_PART_MAIN);
            } else {
                lv_obj_set_style_text_color(icon, COLOR_PRIMARY, LV_PART_MAIN);
            }
        }

        lv_obj_t *lbl = lv_label_create(left);
        lv_label_set_text(lbl, item->label);
        lv_obj_set_width(lbl, LV_PCT(100));
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_add_style(lbl, gui_style_body(), LV_PART_MAIN);
        if (item->type == MENU_BACK) {
            lv_obj_set_style_text_color(lbl, COLOR_TEXT_SEC, LV_PART_MAIN);
        }

        /* Right side: value text */
        if (item->value) {
            lv_obj_t *val = lv_label_create(row);
            lv_label_set_text(val, item->value);
            lv_obj_add_style(val, gui_style_dim(), LV_PART_MAIN);
            value_labels[i] = val;
        } else if (item->type == MENU_SUBMENU) {
            lv_obj_t *arrow = lv_label_create(row);
            lv_label_set_text(arrow, ICON_NEXT);
            lv_obj_set_style_text_color(arrow, COLOR_TEXT_DIM, LV_PART_MAIN);
        }

        /* Make selectable items clickable */
        if (item->type != MENU_INFO) {
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_group_add_obj(grp, row);

            lv_obj_add_event_cb(row, row_focus_cb, LV_EVENT_FOCUSED, (void *)(intptr_t)i);

            if (item->type == MENU_BACK) {
                lv_obj_add_event_cb(row, back_cb, LV_EVENT_CLICKED, NULL);
            } else if (item->action) {
                lv_obj_add_event_cb(row, item_click_cb, LV_EVENT_CLICKED, (void *)item->action);
            }
        }

        current_item_count++;
    }

    return scr;
}

void scr_menu_set_item_value(int index, const char *value) {
    if (index >= 0 && index < current_item_count && value_labels[index]) {
        lv_label_set_text(value_labels[index], value);
    }
}

#endif /* GUI_ENABLED */
