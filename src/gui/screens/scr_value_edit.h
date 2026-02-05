#ifndef SCR_VALUE_EDIT_H
#define SCR_VALUE_EDIT_H

#ifdef GUI_ENABLED

#include <lvgl.h>

/* Value editor types */
enum ValueEditType {
    VE_TOGGLE,    /* ON / OFF */
    VE_NUMERIC,   /* Integer range with step */
    VE_FLOAT,     /* Float range with step */
    VE_CYCLE,     /* Cycle through a list of string options */
};

/* Callback when value is confirmed */
typedef void (*value_confirm_fn)(int int_val, float float_val, int option_idx);

/* Cycle option entry */
struct CycleOption {
    const char *label;
    int value;    /* Integer value associated with this option */
};

/* Value editor configuration */
struct ValueEditConfig {
    const char *title;
    ValueEditType type;

    /* For VE_TOGGLE */
    bool toggle_val;

    /* For VE_NUMERIC */
    int int_val;
    int int_min;
    int int_max;
    int int_step;
    const char *int_unit;    /* e.g. "min", "ms", NULL */

    /* For VE_FLOAT */
    float float_val;
    float float_min;
    float float_max;
    float float_step;
    const char *float_unit;  /* e.g. "V", NULL */
    int float_decimals;      /* Number of decimal places */

    /* For VE_CYCLE */
    const CycleOption *options;
    int option_count;
    int current_option;      /* Index of current selection */

    /* Confirmation callback */
    value_confirm_fn on_confirm;
};

/* Open the value editor screen with given config */
void scr_value_edit_open(const ValueEditConfig *config);

#endif /* GUI_ENABLED */
#endif /* SCR_VALUE_EDIT_H */
