#if defined(GUI_ENABLED) && defined(DSP_ENABLED)

#include "scr_dsp.h"
#include "scr_menu.h"
#include "scr_value_edit.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../../app_state.h"
#include "../../dsp_pipeline.h"
#include "../../debug_serial.h"

extern void saveDspSettingsDebounced();

/* Static value strings for menu items */
static char en_str[4];
static char bypass_str[4];
static char cpu_str[12];
static char ch_str[DSP_MAX_CHANNELS][24];

/* Menu config */
static MenuConfig dsp_menu;

/* Forward declarations */
static void edit_enabled(void);
static void edit_bypass(void);
static void edit_ch_bypass_0(void);
static void edit_ch_bypass_1(void);
static void edit_ch_bypass_2(void);
static void edit_ch_bypass_3(void);

static const char *ch_names[DSP_MAX_CHANNELS] = {"L1", "R1", "L2", "R2"};

static void build_dsp_menu(void) {
    AppState &st = AppState::getInstance();
    DspState *cfg = dsp_get_active_config();
    DspMetrics m = dsp_get_metrics();

    snprintf(en_str, sizeof(en_str), "%s", st.dspEnabled ? "ON" : "OFF");
    snprintf(bypass_str, sizeof(bypass_str), "%s", st.dspBypass ? "ON" : "OFF");
    snprintf(cpu_str, sizeof(cpu_str), "%.1f%%", m.cpuLoadPercent);

    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        snprintf(ch_str[ch], sizeof(ch_str[ch]), "%d stg%s",
                 cfg->channels[ch].stageCount,
                 cfg->channels[ch].bypass ? " BYP" : "");
    }

    dsp_menu.title = "DSP";
    int idx = 0;
    dsp_menu.items[idx++] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};
    dsp_menu.items[idx++] = {"Enabled", en_str, nullptr, MENU_ACTION, edit_enabled};
    dsp_menu.items[idx++] = {"Bypass", bypass_str, nullptr, MENU_ACTION, edit_bypass};
    dsp_menu.items[idx++] = {"CPU Load", cpu_str, nullptr, MENU_INFO, nullptr};

    /* Per-channel info + bypass toggle */
    static menu_action_fn ch_bypass_fns[DSP_MAX_CHANNELS] = {
        edit_ch_bypass_0, edit_ch_bypass_1, edit_ch_bypass_2, edit_ch_bypass_3
    };
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        dsp_menu.items[idx++] = {ch_names[ch], ch_str[ch], nullptr, MENU_ACTION, ch_bypass_fns[ch]};
    }

    dsp_menu.item_count = idx;
}

/* Toggle DSP enabled */
static void on_enabled_confirm(int val, float, int) {
    AppState &st = AppState::getInstance();
    st.dspEnabled = (val == 1);
    saveDspSettingsDebounced();
    st.markDspConfigDirty();
}

static void edit_enabled(void) {
    AppState &st = AppState::getInstance();
    ValueEditConfig cfg = {};
    cfg.title = "DSP Enabled";
    cfg.type = VE_TOGGLE;
    cfg.int_val = st.dspEnabled ? 1 : 0;
    cfg.on_confirm = on_enabled_confirm;
    scr_value_edit_open(&cfg);
}

/* Toggle DSP bypass */
static void on_bypass_confirm(int val, float, int) {
    AppState &st = AppState::getInstance();
    st.dspBypass = (val == 1);
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();
    cfg->globalBypass = st.dspBypass;
    dsp_swap_config();
    saveDspSettingsDebounced();
    st.markDspConfigDirty();
}

static void edit_bypass(void) {
    AppState &st = AppState::getInstance();
    ValueEditConfig cfg = {};
    cfg.title = "DSP Bypass";
    cfg.type = VE_TOGGLE;
    cfg.int_val = st.dspBypass ? 1 : 0;
    cfg.on_confirm = on_bypass_confirm;
    scr_value_edit_open(&cfg);
}

/* Per-channel bypass toggles */
static void toggle_ch_bypass(int ch) {
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[ch].bypass = !cfg->channels[ch].bypass;
    dsp_swap_config();
    saveDspSettingsDebounced();
    AppState::getInstance().markDspConfigDirty();
}

static void edit_ch_bypass_0(void) { toggle_ch_bypass(0); }
static void edit_ch_bypass_1(void) { toggle_ch_bypass(1); }
static void edit_ch_bypass_2(void) { toggle_ch_bypass(2); }
static void edit_ch_bypass_3(void) { toggle_ch_bypass(3); }

lv_obj_t *scr_dsp_create(void) {
    build_dsp_menu();
    return scr_menu_create(&dsp_menu);
}

void scr_dsp_refresh(void) {
    AppState &st = AppState::getInstance();
    DspState *cfg = dsp_get_active_config();
    DspMetrics m = dsp_get_metrics();

    snprintf(en_str, sizeof(en_str), "%s", st.dspEnabled ? "ON" : "OFF");
    scr_menu_set_item_value(1, en_str);

    snprintf(bypass_str, sizeof(bypass_str), "%s", st.dspBypass ? "ON" : "OFF");
    scr_menu_set_item_value(2, bypass_str);

    snprintf(cpu_str, sizeof(cpu_str), "%.1f%%", m.cpuLoadPercent);
    scr_menu_set_item_value(3, cpu_str);

    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        snprintf(ch_str[ch], sizeof(ch_str[ch]), "%d stg%s",
                 cfg->channels[ch].stageCount,
                 cfg->channels[ch].bypass ? " BYP" : "");
        scr_menu_set_item_value(4 + ch, ch_str[ch]);
    }
}

#endif /* GUI_ENABLED && DSP_ENABLED */
