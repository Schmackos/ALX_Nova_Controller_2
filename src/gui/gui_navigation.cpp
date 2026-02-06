#ifdef GUI_ENABLED

#include "gui_navigation.h"
#include "gui_input.h"
#include "../buzzer_handler.h"
#include "../debug_serial.h"
#include <Arduino.h>

/* Navigation stack */
static ScreenId nav_stack[NAV_STACK_MAX];
static int nav_depth = 0;

/* Focus index per stack level — restored on pop */
static int nav_focus_index[NAV_STACK_MAX] = {0};

/* Screen creator registry */
static screen_create_fn screen_creators[SCR_COUNT] = {nullptr};

/* LVGL group for encoder input on current screen */
static lv_group_t *current_group = nullptr;

void gui_nav_init(void) {
    nav_depth = 0;
    current_group = lv_group_create();
    lv_group_set_default(current_group);
    lv_indev_set_group(gui_get_encoder_indev(), current_group);
}

void gui_nav_register(ScreenId id, screen_create_fn creator) {
    if (id < SCR_COUNT) {
        screen_creators[id] = creator;
    }
}

static void activate_screen(ScreenId id, lv_scr_load_anim_t anim) {
    if (id >= SCR_COUNT || screen_creators[id] == nullptr) {
        LOG_E("[GUI Nav] No creator for screen %d", id);
        return;
    }

    /* Clear the current group and reset editing state */
    lv_group_remove_all_objs(current_group);
    lv_group_set_editing(current_group, false);

    /* Create the new screen */
    lv_obj_t *scr = screen_creators[id]();
    if (scr == nullptr) {
        LOG_E("[GUI Nav] Creator returned null for screen %d", id);
        return;
    }

    /* Animate screen transition */
    lv_screen_load_anim(scr, anim, 200, 0, true);
}

void gui_nav_set_focus_index(int idx) {
    if (nav_depth > 0) {
        nav_focus_index[nav_depth - 1] = idx;
    }
}

static void restore_focus(int target_idx) {
    if (target_idx <= 0) return;
    uint32_t count = lv_group_get_obj_count(current_group);
    if (count == 0) return;
    if (target_idx >= (int)count) target_idx = (int)count - 1;
    for (int i = 0; i < target_idx; i++) {
        lv_group_focus_next(current_group);
    }
}

void gui_nav_push(ScreenId id) {
    if (nav_depth >= NAV_STACK_MAX) {
        LOG_E("[GUI Nav] Stack overflow!");
        return;
    }

    nav_stack[nav_depth] = id;
    nav_depth++;
    nav_focus_index[nav_depth - 1] = 0;

    /* Determine animation direction */
    lv_scr_load_anim_t anim;
    if (id == SCR_DESKTOP) {
        anim = LV_SCR_LOAD_ANIM_FADE_IN;
    } else {
        anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;
        buzzer_play(BUZZ_NAV);
    }

    activate_screen(id, anim);
    LOG_D("[GUI Nav] Push screen %d (depth %d)", id, nav_depth);
}

void gui_nav_pop(void) {
    if (nav_depth <= 1) {
        /* Already at root, nothing to pop to */
        return;
    }

    nav_depth--;
    ScreenId prev = nav_stack[nav_depth - 1];
    int saved_idx = nav_focus_index[nav_depth - 1];

    buzzer_play(BUZZ_NAV);
    activate_screen(prev, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    restore_focus(saved_idx);
    LOG_D("[GUI Nav] Pop to screen %d (depth %d, focus %d)", prev, nav_depth, saved_idx);
}

void gui_nav_pop_to_root(void) {
    if (nav_depth <= 1) return;

    nav_depth = 1;
    int saved_idx = nav_focus_index[0];
    activate_screen(nav_stack[0], LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    restore_focus(saved_idx);
    LOG_D("[GUI Nav] Pop to root (depth %d, focus %d)", nav_depth, saved_idx);
}

ScreenId gui_nav_current(void) {
    if (nav_depth == 0) return SCR_DESKTOP;
    return nav_stack[nav_depth - 1];
}

int gui_nav_depth(void) {
    return nav_depth;
}

lv_group_t *gui_nav_get_group(void) {
    return current_group;
}

#endif /* GUI_ENABLED */
