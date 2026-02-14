#if defined(GUI_ENABLED) && defined(DSP_ENABLED)

#include "scr_dsp.h"
#include "scr_menu.h"
#include "scr_value_edit.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../../app_state.h"
#include "../../dsp_pipeline.h"
#include "../../dsp_coefficients.h"
#include "../../dsp_api.h"
#include "../../debug_serial.h"

extern void saveDspSettingsDebounced();

/* Static value strings for menu items */
static char en_str[4];
static char bypass_str[4];
static char cpu_str[12];
static char preset_str[24];
static char ch_str[DSP_MAX_CHANNELS][24];

/* Menu config */
static MenuConfig dsp_menu;

/* Forward declarations */
static void edit_enabled(void);
static void edit_bypass(void);
static void cycle_preset(void);
static void edit_ch_bypass_0(void);
static void edit_ch_bypass_1(void);
static void edit_ch_bypass_2(void);
static void edit_ch_bypass_3(void);
static void open_peq(void);

static const char *ch_names[DSP_MAX_CHANNELS] = {"L1", "R1", "L2", "R2"};

static void build_dsp_menu(void) {
    AppState &st = AppState::getInstance();
    DspState *cfg = dsp_get_active_config();
    DspMetrics m = dsp_get_metrics();

    snprintf(en_str, sizeof(en_str), "%s", st.dspEnabled ? "ON" : "OFF");
    snprintf(bypass_str, sizeof(bypass_str), "%s", st.dspBypass ? "ON" : "OFF");
    snprintf(cpu_str, sizeof(cpu_str), "%.1f%%", m.cpuLoadPercent);

    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        int chainStages = dsp_chain_stage_count(cfg->channels[ch]);
        // Count active PEQ bands
        int peqActive = 0;
        for (int b = 0; b < DSP_PEQ_BANDS && b < cfg->channels[ch].stageCount; b++) {
            if (cfg->channels[ch].stages[b].enabled) peqActive++;
        }
        snprintf(ch_str[ch], sizeof(ch_str[ch]), "%dP %dC%s",
                 peqActive, chainStages,
                 cfg->channels[ch].bypass ? " BYP" : "");
    }

    // Preset string
    if (st.dspPresetIndex >= 0 && st.dspPresetIndex < 4 && st.dspPresetNames[st.dspPresetIndex][0]) {
        snprintf(preset_str, sizeof(preset_str), "%s", st.dspPresetNames[st.dspPresetIndex]);
    } else {
        snprintf(preset_str, sizeof(preset_str), "Custom");
    }

    dsp_menu.title = "DSP";
    int idx = 0;
    dsp_menu.items[idx++] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};
    dsp_menu.items[idx++] = {"Enabled", en_str, nullptr, MENU_ACTION, edit_enabled};
    dsp_menu.items[idx++] = {"Bypass", bypass_str, nullptr, MENU_ACTION, edit_bypass};
    dsp_menu.items[idx++] = {"Preset", preset_str, nullptr, MENU_ACTION, cycle_preset};
    dsp_menu.items[idx++] = {"CPU Load", cpu_str, nullptr, MENU_INFO, nullptr};
    dsp_menu.items[idx++] = {"PEQ Bands", nullptr, nullptr, MENU_ACTION, open_peq};

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

/* Cycle through presets (next existing slot, wrap around) */
static void cycle_preset(void) {
    AppState &st = AppState::getInstance();
    int current = st.dspPresetIndex;
    for (int i = 1; i <= 4; i++) {
        int slot = (current + i) % 4;
        if (dsp_preset_exists(slot)) {
            dsp_preset_load(slot);
            return;
        }
    }
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

    if (st.dspPresetIndex >= 0 && st.dspPresetIndex < 4 && st.dspPresetNames[st.dspPresetIndex][0]) {
        snprintf(preset_str, sizeof(preset_str), "%s", st.dspPresetNames[st.dspPresetIndex]);
    } else {
        snprintf(preset_str, sizeof(preset_str), "Custom");
    }
    scr_menu_set_item_value(3, preset_str);

    snprintf(cpu_str, sizeof(cpu_str), "%.1f%%", m.cpuLoadPercent);
    scr_menu_set_item_value(4, cpu_str);

    // PEQ Bands item at index 5, channels start at index 6
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        int chainStages = dsp_chain_stage_count(cfg->channels[ch]);
        int peqActive = 0;
        for (int b = 0; b < DSP_PEQ_BANDS && b < cfg->channels[ch].stageCount; b++) {
            if (cfg->channels[ch].stages[b].enabled) peqActive++;
        }
        snprintf(ch_str[ch], sizeof(ch_str[ch]), "%dP %dC%s",
                 peqActive, chainStages,
                 cfg->channels[ch].bypass ? " BYP" : "");
        scr_menu_set_item_value(6 + ch, ch_str[ch]);
    }
}

static void open_peq(void) {
    gui_nav_push_deferred(SCR_PEQ_MENU);
}

/* ===== PEQ Band List Screen ===== */
static int peq_channel = 0;  // Selected channel for PEQ editing
static MenuConfig peq_menu;
static char peq_band_str[DSP_PEQ_BANDS][28];

static const char *peq_type_short(DspStageType t) {
    switch (t) {
        case DSP_BIQUAD_PEQ: return "PEQ";
        case DSP_BIQUAD_LOW_SHELF: return "LS";
        case DSP_BIQUAD_HIGH_SHELF: return "HS";
        case DSP_BIQUAD_NOTCH: return "N";
        case DSP_BIQUAD_LPF: return "LP";
        case DSP_BIQUAD_HPF: return "HP";
        case DSP_BIQUAD_BPF: return "BP";
        case DSP_BIQUAD_ALLPASS: return "AP";
        default: return "?";
    }
}

/* Forward: PEQ band edit actions */
static void peq_edit_band_0(void);
static void peq_edit_band_1(void);
static void peq_edit_band_2(void);
static void peq_edit_band_3(void);
static void peq_edit_band_4(void);
static void peq_edit_band_5(void);
static void peq_edit_band_6(void);
static void peq_edit_band_7(void);
static void peq_edit_band_8(void);
static void peq_edit_band_9(void);

static menu_action_fn peq_band_fns[DSP_PEQ_BANDS] = {
    peq_edit_band_0, peq_edit_band_1, peq_edit_band_2, peq_edit_band_3, peq_edit_band_4,
    peq_edit_band_5, peq_edit_band_6, peq_edit_band_7, peq_edit_band_8, peq_edit_band_9
};

/* Channel cycle for PEQ */
static void peq_next_ch(void) {
    peq_channel = (peq_channel + 1) % DSP_MAX_CHANNELS;
    /* Rebuild and refresh on same screen */
    scr_peq_refresh();
}

static void build_peq_menu(void) {
    DspState *cfg = dsp_get_active_config();
    peq_menu.title = "PEQ Bands";
    int idx = 0;
    peq_menu.items[idx++] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};

    /* Channel selector */
    static char peq_ch_str[8];
    const char *ch_names_peq[] = {"L1", "R1", "L2", "R2"};
    snprintf(peq_ch_str, sizeof(peq_ch_str), "%s", ch_names_peq[peq_channel]);
    peq_menu.items[idx++] = {"Channel", peq_ch_str, nullptr, MENU_ACTION, peq_next_ch};

    for (int b = 0; b < DSP_PEQ_BANDS && b < cfg->channels[peq_channel].stageCount; b++) {
        const DspStage &s = cfg->channels[peq_channel].stages[b];
        snprintf(peq_band_str[b], sizeof(peq_band_str[b]), "%s %dHz %s",
                 peq_type_short(s.type),
                 (int)s.biquad.frequency,
                 s.enabled ? "ON" : "OFF");
        static char band_labels[DSP_PEQ_BANDS][8];
        snprintf(band_labels[b], sizeof(band_labels[b]), "Band %d", b + 1);
        peq_menu.items[idx++] = {band_labels[b], peq_band_str[b], nullptr, MENU_ACTION, peq_band_fns[b]};
    }

    peq_menu.item_count = idx;
}

lv_obj_t *scr_peq_create(void) {
    build_peq_menu();
    return scr_menu_create(&peq_menu);
}

void scr_peq_refresh(void) {
    DspState *cfg = dsp_get_active_config();
    const char *ch_names_peq[] = {"L1", "R1", "L2", "R2"};
    static char peq_ch_str[8];
    snprintf(peq_ch_str, sizeof(peq_ch_str), "%s", ch_names_peq[peq_channel]);
    scr_menu_set_item_value(1, peq_ch_str);

    for (int b = 0; b < DSP_PEQ_BANDS && b < cfg->channels[peq_channel].stageCount; b++) {
        const DspStage &s = cfg->channels[peq_channel].stages[b];
        snprintf(peq_band_str[b], sizeof(peq_band_str[b]), "%s %dHz %s",
                 peq_type_short(s.type),
                 (int)s.biquad.frequency,
                 s.enabled ? "ON" : "OFF");
        scr_menu_set_item_value(2 + b, peq_band_str[b]);
    }
}

/* ===== PEQ Band Detail Editor ===== */
static int peq_edit_band_idx = 0;
static MenuConfig peq_band_menu;
static char peq_b_en_str[4];
static char peq_b_freq_str[12];
static char peq_b_gain_str[12];
static char peq_b_q_str[12];
static char peq_b_type_str[12];

static void peq_band_edit_enable(void);
static void peq_band_edit_freq(void);
static void peq_band_edit_gain(void);
static void peq_band_edit_q(void);
static void peq_band_edit_type(void);

static void build_peq_band_menu(void) {
    DspState *cfg = dsp_get_active_config();
    const DspStage &s = cfg->channels[peq_channel].stages[peq_edit_band_idx];

    static char title_buf[20];
    snprintf(title_buf, sizeof(title_buf), "PEQ Band %d", peq_edit_band_idx + 1);
    peq_band_menu.title = title_buf;

    snprintf(peq_b_en_str, sizeof(peq_b_en_str), "%s", s.enabled ? "ON" : "OFF");
    snprintf(peq_b_freq_str, sizeof(peq_b_freq_str), "%d Hz", (int)s.biquad.frequency);
    snprintf(peq_b_gain_str, sizeof(peq_b_gain_str), "%.1f dB", s.biquad.gain);
    snprintf(peq_b_q_str, sizeof(peq_b_q_str), "%.2f", s.biquad.Q);
    snprintf(peq_b_type_str, sizeof(peq_b_type_str), "%s", peq_type_short(s.type));

    int idx = 0;
    peq_band_menu.items[idx++] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};
    peq_band_menu.items[idx++] = {"Enable", peq_b_en_str, nullptr, MENU_ACTION, peq_band_edit_enable};
    peq_band_menu.items[idx++] = {"Frequency", peq_b_freq_str, nullptr, MENU_ACTION, peq_band_edit_freq};
    peq_band_menu.items[idx++] = {"Gain", peq_b_gain_str, nullptr, MENU_ACTION, peq_band_edit_gain};
    peq_band_menu.items[idx++] = {"Q Factor", peq_b_q_str, nullptr, MENU_ACTION, peq_band_edit_q};
    peq_band_menu.items[idx++] = {"Type", peq_b_type_str, nullptr, MENU_ACTION, peq_band_edit_type};
    peq_band_menu.item_count = idx;
}

lv_obj_t *scr_peq_band_create(void) {
    build_peq_band_menu();
    return scr_menu_create(&peq_band_menu);
}

void scr_peq_band_refresh(void) {
    DspState *cfg = dsp_get_active_config();
    if (peq_edit_band_idx >= cfg->channels[peq_channel].stageCount) return;
    const DspStage &s = cfg->channels[peq_channel].stages[peq_edit_band_idx];

    snprintf(peq_b_en_str, sizeof(peq_b_en_str), "%s", s.enabled ? "ON" : "OFF");
    scr_menu_set_item_value(1, peq_b_en_str);
    snprintf(peq_b_freq_str, sizeof(peq_b_freq_str), "%d Hz", (int)s.biquad.frequency);
    scr_menu_set_item_value(2, peq_b_freq_str);
    snprintf(peq_b_gain_str, sizeof(peq_b_gain_str), "%.1f dB", s.biquad.gain);
    scr_menu_set_item_value(3, peq_b_gain_str);
    snprintf(peq_b_q_str, sizeof(peq_b_q_str), "%.2f", s.biquad.Q);
    scr_menu_set_item_value(4, peq_b_q_str);
    snprintf(peq_b_type_str, sizeof(peq_b_type_str), "%s", peq_type_short(s.type));
    scr_menu_set_item_value(5, peq_b_type_str);
}

/* PEQ band edit callbacks */
static void open_peq_band(int band) {
    peq_edit_band_idx = band;
    gui_nav_push_deferred(SCR_PEQ_BAND_EDIT);
}

static void peq_edit_band_0(void) { open_peq_band(0); }
static void peq_edit_band_1(void) { open_peq_band(1); }
static void peq_edit_band_2(void) { open_peq_band(2); }
static void peq_edit_band_3(void) { open_peq_band(3); }
static void peq_edit_band_4(void) { open_peq_band(4); }
static void peq_edit_band_5(void) { open_peq_band(5); }
static void peq_edit_band_6(void) { open_peq_band(6); }
static void peq_edit_band_7(void) { open_peq_band(7); }
static void peq_edit_band_8(void) { open_peq_band(8); }
static void peq_edit_band_9(void) { open_peq_band(9); }

/* Band parameter edit callbacks */
static void on_peq_enable_confirm(int val, float, int) {
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[peq_channel].stages[peq_edit_band_idx].enabled = (val == 1);
    dsp_swap_config();
    saveDspSettingsDebounced();
    AppState::getInstance().markDspConfigDirty();
}

static void peq_band_edit_enable(void) {
    DspState *cfg = dsp_get_active_config();
    ValueEditConfig vc = {};
    vc.title = "Band Enable";
    vc.type = VE_TOGGLE;
    vc.int_val = cfg->channels[peq_channel].stages[peq_edit_band_idx].enabled ? 1 : 0;
    vc.on_confirm = on_peq_enable_confirm;
    scr_value_edit_open(&vc);
}

static void on_peq_freq_confirm(int val, float, int) {
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[peq_channel].stages[peq_edit_band_idx].biquad.frequency = (float)val;
    dsp_compute_biquad_coeffs(cfg->channels[peq_channel].stages[peq_edit_band_idx].biquad,
                              cfg->channels[peq_channel].stages[peq_edit_band_idx].type, cfg->sampleRate);
    dsp_swap_config();
    saveDspSettingsDebounced();
    AppState::getInstance().markDspConfigDirty();
}

static void peq_band_edit_freq(void) {
    DspState *cfg = dsp_get_active_config();
    ValueEditConfig vc = {};
    vc.title = "Frequency";
    vc.type = VE_NUMERIC;
    vc.int_val = (int)cfg->channels[peq_channel].stages[peq_edit_band_idx].biquad.frequency;
    vc.int_min = 20;
    vc.int_max = 20000;
    vc.int_step = 10;
    vc.int_unit = "Hz";
    vc.on_confirm = on_peq_freq_confirm;
    scr_value_edit_open(&vc);
}

static void on_peq_gain_confirm(int, float val, int) {
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[peq_channel].stages[peq_edit_band_idx].biquad.gain = val;
    dsp_compute_biquad_coeffs(cfg->channels[peq_channel].stages[peq_edit_band_idx].biquad,
                              cfg->channels[peq_channel].stages[peq_edit_band_idx].type, cfg->sampleRate);
    dsp_swap_config();
    saveDspSettingsDebounced();
    AppState::getInstance().markDspConfigDirty();
}

static void peq_band_edit_gain(void) {
    DspState *cfg = dsp_get_active_config();
    ValueEditConfig vc = {};
    vc.title = "Gain";
    vc.type = VE_FLOAT;
    vc.float_val = cfg->channels[peq_channel].stages[peq_edit_band_idx].biquad.gain;
    vc.float_min = -24.0f;
    vc.float_max = 24.0f;
    vc.float_step = 0.5f;
    vc.float_unit = "dB";
    vc.float_decimals = 1;
    vc.on_confirm = on_peq_gain_confirm;
    scr_value_edit_open(&vc);
}

static void on_peq_q_confirm(int, float val, int) {
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[peq_channel].stages[peq_edit_band_idx].biquad.Q = val;
    dsp_compute_biquad_coeffs(cfg->channels[peq_channel].stages[peq_edit_band_idx].biquad,
                              cfg->channels[peq_channel].stages[peq_edit_band_idx].type, cfg->sampleRate);
    dsp_swap_config();
    saveDspSettingsDebounced();
    AppState::getInstance().markDspConfigDirty();
}

static void peq_band_edit_q(void) {
    DspState *cfg = dsp_get_active_config();
    ValueEditConfig vc = {};
    vc.title = "Q Factor";
    vc.type = VE_FLOAT;
    vc.float_val = cfg->channels[peq_channel].stages[peq_edit_band_idx].biquad.Q;
    vc.float_min = 0.1f;
    vc.float_max = 25.0f;
    vc.float_step = 0.1f;
    vc.float_decimals = 2;
    vc.on_confirm = on_peq_q_confirm;
    scr_value_edit_open(&vc);
}

static const CycleOption peq_type_options[] = {
    {"PEQ", DSP_BIQUAD_PEQ},
    {"Low Shelf", DSP_BIQUAD_LOW_SHELF},
    {"High Shelf", DSP_BIQUAD_HIGH_SHELF},
    {"Notch", DSP_BIQUAD_NOTCH},
    {"BPF", DSP_BIQUAD_BPF},
    {"LPF", DSP_BIQUAD_LPF},
    {"HPF", DSP_BIQUAD_HPF},
    {"Allpass", DSP_BIQUAD_ALLPASS},
};

static void on_peq_type_confirm(int, float, int option_idx) {
    if (option_idx < 0 || option_idx >= 8) return;
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();
    cfg->channels[peq_channel].stages[peq_edit_band_idx].type = (DspStageType)peq_type_options[option_idx].value;
    dsp_compute_biquad_coeffs(cfg->channels[peq_channel].stages[peq_edit_band_idx].biquad,
                              cfg->channels[peq_channel].stages[peq_edit_band_idx].type, cfg->sampleRate);
    dsp_swap_config();
    saveDspSettingsDebounced();
    AppState::getInstance().markDspConfigDirty();
}

static void peq_band_edit_type(void) {
    DspState *cfg = dsp_get_active_config();
    DspStageType current = cfg->channels[peq_channel].stages[peq_edit_band_idx].type;
    int cur_idx = 0;
    for (int i = 0; i < 8; i++) {
        if (peq_type_options[i].value == current) { cur_idx = i; break; }
    }
    ValueEditConfig vc = {};
    vc.title = "Filter Type";
    vc.type = VE_CYCLE;
    vc.options = peq_type_options;
    vc.option_count = 8;
    vc.current_option = cur_idx;
    vc.on_confirm = on_peq_type_confirm;
    scr_value_edit_open(&vc);
}

#endif /* GUI_ENABLED && DSP_ENABLED */
