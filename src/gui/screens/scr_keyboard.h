#ifndef SCR_KEYBOARD_H
#define SCR_KEYBOARD_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Callback type for keyboard result */
typedef void (*keyboard_done_fn)(const char *text);

/* Keyboard configuration */
struct KeyboardConfig {
    const char *title;         /* Prompt text, e.g. "Enter Password" */
    const char *initial_text;  /* Pre-filled text, or NULL */
    bool password_mode;        /* Mask characters */
    keyboard_done_fn on_done;  /* Called with result text when user confirms */
};

/* Open the keyboard screen */
void scr_keyboard_open(const KeyboardConfig *config);

#endif /* GUI_ENABLED */
#endif /* SCR_KEYBOARD_H */
