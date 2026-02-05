#ifndef GUI_MANAGER_H
#define GUI_MANAGER_H

#ifdef GUI_ENABLED

/* Initialize TFT display, LVGL, input drivers, and start GUI FreeRTOS task */
void gui_init(void);

/* Wake the display (called from external input like K0 button) */
void gui_wake(void);

/* Put the display to sleep (called from web/MQTT) */
void gui_sleep(void);

/* Returns true if the display is currently awake */
bool gui_is_awake(void);

#endif /* GUI_ENABLED */
#endif /* GUI_MANAGER_H */
