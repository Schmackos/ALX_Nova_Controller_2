#ifndef SCR_DSP_H
#define SCR_DSP_H

#if defined(GUI_ENABLED) && defined(DSP_ENABLED)

#include <lvgl.h>

lv_obj_t *scr_dsp_create(void);
void scr_dsp_refresh(void);

#endif /* GUI_ENABLED && DSP_ENABLED */
#endif /* SCR_DSP_H */
