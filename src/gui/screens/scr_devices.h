#ifndef SCR_DEVICES_H
#define SCR_DEVICES_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Create the HAL devices screen */
lv_obj_t *scr_devices_create(void);

/* Refresh live device data (call periodically from gui_task) */
void scr_devices_refresh(void);

#endif /* GUI_ENABLED */
#endif /* SCR_DEVICES_H */
