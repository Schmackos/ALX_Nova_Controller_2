#ifndef SCR_CONTROL_H
#define SCR_CONTROL_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Create the control menu screen */
lv_obj_t *scr_control_create(void);

/* Refresh control menu values from AppState */
void scr_control_refresh(void);

#endif /* GUI_ENABLED */
#endif /* SCR_CONTROL_H */
