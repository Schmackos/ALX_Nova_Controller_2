#ifdef GUI_ENABLED

#include "gui_theme.h"
#include <lvgl.h>

static bool dark_mode_active = true;

/* Shared styles */
static lv_style_t style_screen;
static lv_style_t style_card;
static lv_style_t style_card_focused;
static lv_style_t style_title;
static lv_style_t style_body;
static lv_style_t style_dim;
static lv_style_t style_btn;
static lv_style_t style_btn_pressed;
static lv_style_t style_list_item;
static lv_style_t style_list_item_focused;
static bool styles_initialized = false;

static void init_styles(bool dark) {
    if (styles_initialized) {
        lv_style_reset(&style_screen);
        lv_style_reset(&style_card);
        lv_style_reset(&style_card_focused);
        lv_style_reset(&style_title);
        lv_style_reset(&style_body);
        lv_style_reset(&style_dim);
        lv_style_reset(&style_btn);
        lv_style_reset(&style_btn_pressed);
        lv_style_reset(&style_list_item);
        lv_style_reset(&style_list_item_focused);
    }

    /* Screen background */
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, dark ? COLOR_BG_DARK : COLOR_BG_LIGHT);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_text_color(&style_screen, dark ? COLOR_TEXT_PRI : COLOR_TEXT_PRI_LT);

    /* Card */
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, dark ? COLOR_BG_CARD : COLOR_CARD_LIGHT);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, 8);
    lv_style_set_pad_all(&style_card, 10);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_color(&style_card, dark ? lv_color_hex(0x2A2A4A) : lv_color_hex(0xDDDDDD));
    lv_style_set_border_opa(&style_card, LV_OPA_COVER);

    /* Card focused */
    lv_style_init(&style_card_focused);
    lv_style_set_border_color(&style_card_focused, COLOR_PRIMARY);
    lv_style_set_border_width(&style_card_focused, 2);
    lv_style_set_border_opa(&style_card_focused, LV_OPA_COVER);

    /* Title text */
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_18);
    lv_style_set_text_color(&style_title, COLOR_PRIMARY);

    /* Body text */
    lv_style_init(&style_body);
    lv_style_set_text_font(&style_body, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_body, dark ? COLOR_TEXT_PRI : COLOR_TEXT_PRI_LT);

    /* Dimmed text */
    lv_style_init(&style_dim);
    lv_style_set_text_font(&style_dim, &lv_font_montserrat_12);
    lv_style_set_text_color(&style_dim, dark ? COLOR_TEXT_SEC : COLOR_TEXT_SEC_LT);

    /* Button */
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, COLOR_PRIMARY);
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn, lv_color_hex(0xFFFFFF));
    lv_style_set_radius(&style_btn, 4);
    lv_style_set_pad_hor(&style_btn, 12);
    lv_style_set_pad_ver(&style_btn, 6);

    /* Button pressed */
    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_color(&style_btn_pressed, COLOR_PRIMARY_DK);

    /* List item */
    lv_style_init(&style_list_item);
    lv_style_set_bg_color(&style_list_item, dark ? COLOR_BG_CARD : COLOR_CARD_LIGHT);
    lv_style_set_bg_opa(&style_list_item, LV_OPA_COVER);
    lv_style_set_text_color(&style_list_item, dark ? COLOR_TEXT_PRI : COLOR_TEXT_PRI_LT);
    lv_style_set_pad_all(&style_list_item, 8);
    lv_style_set_border_width(&style_list_item, 0);
    lv_style_set_radius(&style_list_item, 4);

    /* List item focused */
    lv_style_init(&style_list_item_focused);
    lv_style_set_bg_color(&style_list_item_focused, dark ? COLOR_BG_SURFACE : COLOR_SURFACE_LIGHT);
    lv_style_set_bg_opa(&style_list_item_focused, LV_OPA_COVER);
    lv_style_set_border_color(&style_list_item_focused, COLOR_PRIMARY);
    lv_style_set_border_width(&style_list_item_focused, 1);
    lv_style_set_border_opa(&style_list_item_focused, LV_OPA_COVER);

    styles_initialized = true;
}

void gui_theme_init(bool dark_mode) {
    dark_mode_active = dark_mode;
    init_styles(dark_mode);

    /* Apply default LVGL theme with our primary color */
    lv_theme_t *th = lv_theme_default_init(
        lv_display_get_default(),
        COLOR_PRIMARY,
        COLOR_PRIMARY_DK,
        dark_mode,
        &lv_font_montserrat_14
    );
    lv_display_set_theme(lv_display_get_default(), th);
}

void gui_theme_set_dark(bool dark_mode) {
    dark_mode_active = dark_mode;
    init_styles(dark_mode);

    lv_theme_t *th = lv_theme_default_init(
        lv_display_get_default(),
        COLOR_PRIMARY,
        COLOR_PRIMARY_DK,
        dark_mode,
        &lv_font_montserrat_14
    );
    lv_display_set_theme(lv_display_get_default(), th);

    /* Force redraw */
    lv_obj_invalidate(lv_screen_active());
}

bool gui_theme_is_dark(void) {
    return dark_mode_active;
}

/* Public style accessors for use by screen modules */
lv_style_t *gui_style_screen(void) { return &style_screen; }
lv_style_t *gui_style_card(void) { return &style_card; }
lv_style_t *gui_style_card_focused(void) { return &style_card_focused; }
lv_style_t *gui_style_title(void) { return &style_title; }
lv_style_t *gui_style_body(void) { return &style_body; }
lv_style_t *gui_style_dim(void) { return &style_dim; }
lv_style_t *gui_style_btn(void) { return &style_btn; }
lv_style_t *gui_style_btn_pressed(void) { return &style_btn_pressed; }
lv_style_t *gui_style_list_item(void) { return &style_list_item; }
lv_style_t *gui_style_list_item_focused(void) { return &style_list_item_focused; }

#endif /* GUI_ENABLED */
