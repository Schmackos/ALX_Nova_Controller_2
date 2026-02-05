#ifndef SCR_SETTINGS_H
#define SCR_SETTINGS_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Create the settings main menu screen */
lv_obj_t *scr_settings_create(void);

#endif /* GUI_ENABLED */
#endif /* SCR_SETTINGS_H */
