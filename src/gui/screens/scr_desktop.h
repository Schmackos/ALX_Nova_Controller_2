#ifndef SCR_DESKTOP_H
#define SCR_DESKTOP_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Create the desktop carousel screen */
lv_obj_t *scr_desktop_create(void);

/* Refresh live summary data on dashboard cards */
void scr_desktop_refresh(void);

#endif /* GUI_ENABLED */
#endif /* SCR_DESKTOP_H */
