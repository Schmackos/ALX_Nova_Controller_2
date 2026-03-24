#ifdef GUI_ENABLED

#include "scr_devices.h"
#include "../gui_config.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../gui_theme.h"
#include "../../debug_serial.h"
#ifdef DAC_ENABLED
#include "../../hal/hal_device_manager.h"
#include "../../hal/hal_types.h"
#endif

/* Scrollable container handle for rebuild on refresh */
static lv_obj_t *device_cont = nullptr;

/* State dot size */
#define DEV_DOT_SIZE 8

/* Back button callback */
static void on_back(lv_event_t *e) {
    (void)e;
    gui_nav_pop_deferred();
}

/* Map HalDeviceState to a color */
static lv_color_t state_to_color(uint8_t state) {
#ifdef DAC_ENABLED
    switch ((HalDeviceState)state) {
        case HAL_STATE_AVAILABLE:   return COLOR_SUCCESS;
        case HAL_STATE_DETECTED:
        case HAL_STATE_CONFIGURING:
        case HAL_STATE_DISABLED:    return COLOR_WARNING;
        case HAL_STATE_UNAVAILABLE:
        case HAL_STATE_ERROR:       return COLOR_ERROR;
        default:                    return COLOR_TEXT_DIM;
    }
#else
    (void)state;
    return COLOR_TEXT_DIM;
#endif
}

/* Map HalDeviceState to a short label */
static const char *state_to_label(uint8_t state) {
#ifdef DAC_ENABLED
    switch ((HalDeviceState)state) {
        case HAL_STATE_UNKNOWN:     return "Unknown";
        case HAL_STATE_DETECTED:    return "Detected";
        case HAL_STATE_CONFIGURING: return "Config...";
        case HAL_STATE_AVAILABLE:   return "Ready";
        case HAL_STATE_UNAVAILABLE: return "Unavail";
        case HAL_STATE_ERROR:       return "Error";
        case HAL_STATE_DISABLED:    return "Disabled";
        case HAL_STATE_REMOVED:     return "Removed";
        default:                    return "?";
    }
#else
    (void)state;
    return "?";
#endif
}

/* Map HalDeviceType to a short label */
static const char *type_to_label(uint8_t type) {
#ifdef DAC_ENABLED
    switch ((HalDeviceType)type) {
        case HAL_DEV_DAC:    return "DAC";
        case HAL_DEV_ADC:    return "ADC";
        case HAL_DEV_CODEC:  return "Codec";
        case HAL_DEV_AMP:    return "Amp";
        case HAL_DEV_DSP:    return "DSP";
        case HAL_DEV_SENSOR: return "Sensor";
        default:             return "Other";
    }
#else
    (void)type;
    return "Other";
#endif
}

/* Map HalDiscovery to a short label */
static const char *discovery_to_label(uint8_t disc) {
#ifdef DAC_ENABLED
    switch ((HalDiscovery)disc) {
        case HAL_DISC_BUILTIN: return "Built-in";
        case HAL_DISC_EEPROM:  return "EEPROM";
        case HAL_DISC_GPIO_ID: return "GPIO ID";
        case HAL_DISC_MANUAL:  return "Manual";
        case HAL_DISC_ONLINE:  return "Online";
        default:               return "?";
    }
#else
    (void)disc;
    return "?";
#endif
}

#ifdef DAC_ENABLED
/* Callback for forEach — adds a device row to the container */
struct DeviceRowCtx {
    lv_obj_t *cont;
    int count;
};

static void add_device_row(HalDevice* device, void* ctx) {
    DeviceRowCtx* rc = (DeviceRowCtx*)ctx;
    if (!device || !rc->cont) return;

    const HalDeviceDescriptor& desc = device->getDescriptor();

    /* Device row container — fixed height for 2 lines of montserrat_10 */
    lv_obj_t *row = lv_obj_create(rc->cont);
    lv_obj_set_size(row, LV_PCT(100), 30);
    lv_obj_set_style_bg_color(row, COLOR_BG_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(row, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 3, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, COLOR_BORDER_DARK, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* State dot — top right */
    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_set_size(dot, DEV_DOT_SIZE, DEV_DOT_SIZE);
    lv_obj_set_style_radius(dot, DEV_DOT_SIZE / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dot, state_to_color(device->_state), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, 0, 0);

    /* Row content width = parent minus dot and padding */
    lv_coord_t row_w = DISPLAY_HEIGHT - 24;  /* 160 - margins/padding/dot */

    /* Device name — clips with "..." if too long */
    lv_obj_t *name_lbl = lv_label_create(row);
    lv_label_set_text_fmt(name_lbl, "%s", desc.name);
    lv_obj_set_width(name_lbl, row_w);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(name_lbl, COLOR_TEXT_PRI, LV_PART_MAIN);
    lv_obj_align(name_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Type + State + Discovery on second line — clips with "..." */
    lv_obj_t *info_lbl = lv_label_create(row);
    lv_label_set_text_fmt(info_lbl, "%s | %s | %s",
             type_to_label(desc.type),
             state_to_label(device->_state),
             discovery_to_label(device->getDiscovery()));
    lv_obj_set_width(info_lbl, row_w);
    lv_label_set_long_mode(info_lbl, LV_LABEL_LONG_DOT);
    lv_obj_add_style(info_lbl, gui_style_dim(), LV_PART_MAIN);
    lv_obj_align(info_lbl, LV_ALIGN_TOP_LEFT, 0, 13);

    rc->count++;
}
#endif /* DAC_ENABLED */

/* Build device list inside the scrollable container */
static void build_device_list(lv_obj_t *cont) {
    /* Clear existing children */
    lv_obj_clean(cont);

#ifdef DAC_ENABLED
    HalDeviceManager& mgr = HalDeviceManager::instance();
    uint8_t count = mgr.getCount();

    if (count == 0) {
        lv_obj_t *empty_lbl = lv_label_create(cont);
        lv_label_set_text(empty_lbl, "No devices registered");
        lv_obj_add_style(empty_lbl, gui_style_dim(), LV_PART_MAIN);
        return;
    }

    DeviceRowCtx ctx = {cont, 0};
    mgr.forEach(add_device_row, &ctx);
#else
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, "HAL not available");
    lv_obj_add_style(lbl, gui_style_dim(), LV_PART_MAIN);
#endif
}

lv_obj_t *scr_devices_create(void) {
    /* Reset static pointers */
    device_cont = nullptr;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, gui_style_screen(), LV_PART_MAIN);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, LV_SYMBOL_LIST " Devices");
    lv_obj_add_style(title, gui_style_title(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    /* Scrollable container */
    device_cont = lv_obj_create(scr);
    lv_obj_set_size(device_cont, DISPLAY_HEIGHT, DISPLAY_WIDTH - 36);
    lv_obj_align(device_cont, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_flex_flow(device_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(device_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(device_cont, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(device_cont, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(device_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(device_cont, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(device_cont, LV_SCROLLBAR_MODE_AUTO);

    /* Make scrollable for encoder */
    lv_group_add_obj(gui_nav_get_group(), device_cont);

    /* Build device list */
    build_device_list(device_cont);

    /* Back button at bottom */
    lv_obj_t *back_btn = lv_obj_create(scr);
    lv_obj_set_size(back_btn, 60, 16);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_add_style(back_btn, gui_style_list_item(), LV_PART_MAIN);
    lv_obj_add_style(back_btn, gui_style_list_item_focused(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_group_add_obj(gui_nav_get_group(), back_btn);

    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, ICON_BACK " Back");
    lv_obj_set_style_text_color(back_lbl, COLOR_TEXT_SEC, LV_PART_MAIN);
    lv_obj_add_style(back_lbl, gui_style_dim(), LV_PART_MAIN);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, on_back, LV_EVENT_CLICKED, NULL);

    LOG_D("[GUI] Devices screen created");
    return scr;
}

void scr_devices_refresh(void) {
    if (!device_cont) return;
    build_device_list(device_cont);
}

#endif /* GUI_ENABLED */
