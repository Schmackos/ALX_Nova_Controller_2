#ifndef GUI_INPUT_H
#define GUI_INPUT_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Initialize encoder and button hardware, register LVGL indev drivers */
void gui_input_init(void);

/* Get the LVGL encoder input device (for group assignment) */
lv_indev_t *gui_get_encoder_indev(void);

/* Returns true if any input activity occurred since last call (for wake detection) */
bool gui_input_activity(void);

/* Raw mode: rotation bypasses LVGL group navigation, collectable via get_raw_diff.
 * Button presses still go through LVGL normally (LV_EVENT_CLICKED). */
void gui_input_set_raw_mode(bool raw);
int32_t gui_input_get_raw_diff(void);

#endif /* GUI_ENABLED */
#endif /* GUI_INPUT_H */
