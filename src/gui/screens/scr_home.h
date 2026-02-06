#ifndef SCR_HOME_H
#define SCR_HOME_H

#ifdef GUI_ENABLED

#include <lvgl.h>

lv_obj_t *scr_home_create(void);
void scr_home_refresh(void);

#endif /* GUI_ENABLED */
#endif /* SCR_HOME_H */
