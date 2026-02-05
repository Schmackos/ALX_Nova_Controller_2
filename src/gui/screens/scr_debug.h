#ifndef SCR_DEBUG_H
#define SCR_DEBUG_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Create the debug info screen */
lv_obj_t *scr_debug_create(void);

/* Refresh live debug data (call periodically from gui_task) */
void scr_debug_refresh(void);

#endif /* GUI_ENABLED */
#endif /* SCR_DEBUG_H */
