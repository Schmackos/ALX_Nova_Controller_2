#ifndef SCR_SUPPORT_H
#define SCR_SUPPORT_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Create the support info screen */
lv_obj_t *scr_support_create(void);

#endif /* GUI_ENABLED */
#endif /* SCR_SUPPORT_H */
