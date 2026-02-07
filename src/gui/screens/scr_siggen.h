#ifndef SCR_SIGGEN_H
#define SCR_SIGGEN_H

#ifdef GUI_ENABLED

#include <lvgl.h>

lv_obj_t *scr_siggen_create(void);
void scr_siggen_refresh(void);

#endif /* GUI_ENABLED */
#endif /* SCR_SIGGEN_H */
