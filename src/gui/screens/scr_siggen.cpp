#ifdef GUI_ENABLED

#include "scr_siggen.h"
#include "scr_menu.h"
#include "scr_value_edit.h"
#include "../gui_icons.h"
#include "../gui_navigation.h"
#include "../../app_state.h"
#include "../../signal_generator.h"
#include "../../settings_manager.h"
#include "../../debug_serial.h"
#include <Arduino.h>

/* Waveform cycle options */
static const CycleOption waveform_opts[] = {
    {"Sine",    WAVE_SINE},
    {"Square",  WAVE_SQUARE},
    {"Noise",   WAVE_NOISE},
    {"Sweep",   WAVE_SWEEP},
};

/* Channel cycle options */
static const CycleOption channel_opts[] = {
    {"Ch 1",  SIGCHAN_CH1},
    {"Ch 2",  SIGCHAN_CH2},
    {"Both",  SIGCHAN_BOTH},
};

/* Output mode cycle options */
static const CycleOption output_opts[] = {
    {"Software", SIGOUT_SOFTWARE},
    {"PWM",      SIGOUT_PWM},
};

/* Target ADC cycle options */
static const CycleOption target_adc_opts[] = {
    {"ADC 1", SIGTARGET_ADC1},
    {"ADC 2", SIGTARGET_ADC2},
    {"Both",  SIGTARGET_BOTH},
};

/* Confirmation callbacks */
static void on_enabled_confirm(int int_val, float, int) {
    AppState::getInstance().sigGenEnabled = (int_val != 0);
    siggen_apply_params();
    AppState::getInstance().markSignalGenDirty();
    LOG_I("[GUI] Signal generator %s", int_val ? "ON" : "OFF");
}

static void on_waveform_confirm(int int_val, float, int) {
    AppState::getInstance().sigGenWaveform = int_val;
    siggen_apply_params();
    saveSignalGenSettings();
    AppState::getInstance().markSignalGenDirty();
    LOG_I("[GUI] Signal waveform set to %d", int_val);
}

static void on_frequency_confirm(int int_val, float, int) {
    AppState::getInstance().sigGenFrequency = (float)int_val;
    siggen_apply_params();
    saveSignalGenSettings();
    AppState::getInstance().markSignalGenDirty();
    LOG_I("[GUI] Signal frequency set to %d Hz", int_val);
}

static void on_amplitude_confirm(int, float float_val, int) {
    AppState::getInstance().sigGenAmplitude = float_val;
    siggen_apply_params();
    saveSignalGenSettings();
    AppState::getInstance().markSignalGenDirty();
    LOG_I("[GUI] Signal amplitude set to %+.0f dBFS", float_val);
}

static void on_channel_confirm(int int_val, float, int) {
    AppState::getInstance().sigGenChannel = int_val;
    siggen_apply_params();
    saveSignalGenSettings();
    AppState::getInstance().markSignalGenDirty();
    LOG_I("[GUI] Signal channel set to %d", int_val);
}

static void on_output_confirm(int int_val, float, int) {
    AppState::getInstance().sigGenOutputMode = int_val;
    siggen_apply_params();
    saveSignalGenSettings();
    AppState::getInstance().markSignalGenDirty();
    LOG_I("[GUI] Signal output set to %d", int_val);
}

static void on_target_adc_confirm(int int_val, float, int) {
    AppState::getInstance().sigGenTargetAdc = int_val;
    siggen_apply_params();
    saveSignalGenSettings();
    AppState::getInstance().markSignalGenDirty();
    LOG_I("[GUI] Signal target ADC set to %d", int_val);
}

/* Menu action callbacks */
static void edit_enabled(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Signal Gen";
    cfg.type = VE_TOGGLE;
    cfg.toggle_val = AppState::getInstance().sigGenEnabled;
    cfg.on_confirm = on_enabled_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_waveform(void) {
    AppState &st = AppState::getInstance();
    int cur = 0;
    for (int i = 0; i < 4; i++) {
        if (waveform_opts[i].value == st.sigGenWaveform) { cur = i; break; }
    }
    ValueEditConfig cfg = {};
    cfg.title = "Waveform";
    cfg.type = VE_CYCLE;
    cfg.options = waveform_opts;
    cfg.option_count = 4;
    cfg.current_option = cur;
    cfg.on_confirm = on_waveform_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_frequency(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Frequency";
    cfg.type = VE_NUMERIC;
    cfg.int_val = (int)AppState::getInstance().sigGenFrequency;
    cfg.int_min = 1;
    cfg.int_max = 22000;
    cfg.int_step = 10;
    cfg.int_unit = "Hz";
    cfg.on_confirm = on_frequency_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_amplitude(void) {
    ValueEditConfig cfg = {};
    cfg.title = "Amplitude";
    cfg.type = VE_FLOAT;
    cfg.float_val = AppState::getInstance().sigGenAmplitude;
    cfg.float_min = -96.0f;
    cfg.float_max = 0.0f;
    cfg.float_step = 1.0f;
    cfg.float_unit = "dBFS";
    cfg.float_decimals = 0;
    cfg.on_confirm = on_amplitude_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_channel(void) {
    AppState &st = AppState::getInstance();
    int cur = 0;
    for (int i = 0; i < 3; i++) {
        if (channel_opts[i].value == st.sigGenChannel) { cur = i; break; }
    }
    ValueEditConfig cfg = {};
    cfg.title = "Channel";
    cfg.type = VE_CYCLE;
    cfg.options = channel_opts;
    cfg.option_count = 3;
    cfg.current_option = cur;
    cfg.on_confirm = on_channel_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_output(void) {
    AppState &st = AppState::getInstance();
    int cur = 0;
    for (int i = 0; i < 2; i++) {
        if (output_opts[i].value == st.sigGenOutputMode) { cur = i; break; }
    }
    ValueEditConfig cfg = {};
    cfg.title = "Output";
    cfg.type = VE_CYCLE;
    cfg.options = output_opts;
    cfg.option_count = 2;
    cfg.current_option = cur;
    cfg.on_confirm = on_output_confirm;
    scr_value_edit_open(&cfg);
}

static void edit_target_adc(void) {
    AppState &st = AppState::getInstance();
    int cur = 0;
    for (int i = 0; i < 3; i++) {
        if (target_adc_opts[i].value == st.sigGenTargetAdc) { cur = i; break; }
    }
    ValueEditConfig cfg = {};
    cfg.title = "Target";
    cfg.type = VE_CYCLE;
    cfg.options = target_adc_opts;
    cfg.option_count = 3;
    cfg.current_option = cur;
    cfg.on_confirm = on_target_adc_confirm;
    scr_value_edit_open(&cfg);
}

/* Build the signal generator menu */
static MenuConfig siggen_menu;

static void build_siggen_menu(void) {
    AppState &st = AppState::getInstance();

    static char en_str[8];
    snprintf(en_str, sizeof(en_str), "%s", st.sigGenEnabled ? "ON" : "OFF");

    const char *wave_names[] = {"Sine", "Square", "Noise", "Sweep"};
    const char *wave_str = wave_names[st.sigGenWaveform % 4];

    static char freq_str[12];
    snprintf(freq_str, sizeof(freq_str), "%d Hz", (int)st.sigGenFrequency);

    static char amp_str[12];
    snprintf(amp_str, sizeof(amp_str), "%+.0f dBFS", st.sigGenAmplitude);

    const char *chan_names[] = {"Ch 1", "Ch 2", "Both"};
    const char *chan_str = chan_names[st.sigGenChannel % 3];

    const char *out_str = st.sigGenOutputMode == 0 ? "Software" : "PWM";

    const char *target_names[] = {"ADC 1", "ADC 2", "Both"};
    const char *target_str = target_names[st.sigGenTargetAdc % 3];

    siggen_menu.title = "Signal Gen";
    int idx = 0;
    siggen_menu.items[idx++] = {ICON_BACK " Back", nullptr, nullptr, MENU_BACK, nullptr};
    siggen_menu.items[idx++] = {"Enabled", en_str, nullptr, MENU_ACTION, edit_enabled};
    siggen_menu.items[idx++] = {"Waveform", wave_str, nullptr, MENU_ACTION, edit_waveform};
    siggen_menu.items[idx++] = {"Frequency", freq_str, nullptr, MENU_ACTION, edit_frequency};
    siggen_menu.items[idx++] = {"Amplitude", amp_str, nullptr, MENU_ACTION, edit_amplitude};
    siggen_menu.items[idx++] = {"Channel", chan_str, nullptr, MENU_ACTION, edit_channel};
    siggen_menu.items[idx++] = {"Output", out_str, nullptr, MENU_ACTION, edit_output};
    if (st.numAdcsDetected > 1) {
        siggen_menu.items[idx++] = {"Target", target_str, nullptr, MENU_ACTION, edit_target_adc};
    }
    siggen_menu.item_count = idx;
}

lv_obj_t *scr_siggen_create(void) {
    build_siggen_menu();
    return scr_menu_create(&siggen_menu);
}

void scr_siggen_refresh(void) {
    AppState &st = AppState::getInstance();

    scr_menu_set_item_value(1, st.sigGenEnabled ? "ON" : "OFF");

    const char *wave_names[] = {"Sine", "Square", "Noise", "Sweep"};
    scr_menu_set_item_value(2, wave_names[st.sigGenWaveform % 4]);

    static char freq_buf[12];
    snprintf(freq_buf, sizeof(freq_buf), "%d Hz", (int)st.sigGenFrequency);
    scr_menu_set_item_value(3, freq_buf);

    static char amp_buf[12];
    snprintf(amp_buf, sizeof(amp_buf), "%+.0f dBFS", st.sigGenAmplitude);
    scr_menu_set_item_value(4, amp_buf);

    const char *chan_names[] = {"Ch 1", "Ch 2", "Both"};
    scr_menu_set_item_value(5, chan_names[st.sigGenChannel % 3]);

    scr_menu_set_item_value(6, st.sigGenOutputMode == 0 ? "Software" : "PWM");

    if (st.numAdcsDetected > 1) {
        const char *target_names[] = {"ADC 1", "ADC 2", "Both"};
        scr_menu_set_item_value(7, target_names[st.sigGenTargetAdc % 3]);
    }
}

#endif /* GUI_ENABLED */
