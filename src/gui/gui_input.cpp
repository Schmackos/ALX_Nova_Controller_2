#ifdef GUI_ENABLED

#include "gui_input.h"
#include "gui_config.h"
#include "../buzzer_handler.h"
#include "../debug_serial.h"
#include "../hal/hal_device_manager.h"
#include "../hal/hal_encoder.h"
#include <Arduino.h>
#include <lvgl.h>

/* File-static pin tracking (DRAM-safe for ISR access) */
static int _enc_pinA  = ENCODER_A_PIN;
static int _enc_pinB  = ENCODER_B_PIN;
static int _enc_pinSw = ENCODER_SW_PIN;

/* Volatile state shared with ISRs */
static volatile int32_t encoder_diff = 0;
static volatile bool encoder_pressed = false;
static volatile bool input_activity_flag = false;
static volatile bool press_activity_flag = false;

/* ISR state for encoder */
static volatile uint8_t encoder_last_state = 0;
static volatile int8_t enc_sub_count = 0;  /* Sub-step accumulator within one detent */

/* Debounce for encoder switch */
static volatile unsigned long last_sw_time = 0;

/* LVGL indev pointer */
static lv_indev_t *encoder_indev = nullptr;

/* Raw mode: rotation goes to raw_diff instead of LVGL enc_diff */
static bool raw_mode = false;
static volatile int32_t raw_diff = 0;

/* Encoder pin change ISR — Gray code decoding with detent filtering.
 * Accumulates sub-steps and only emits ±1 when the encoder returns
 * to the detent position (both pins HIGH = state 0b11 with pullups).
 * This ensures one physical click = one logical step. */
static void IRAM_ATTR encoder_isr() {
    uint8_t a = digitalRead(_enc_pinA);
    uint8_t b = digitalRead(_enc_pinB);
    uint8_t state = (a << 1) | b;

    static const int8_t transitions[] = {
         0, -1, +1,  0,
        +1,  0,  0, -1,
        -1,  0,  0, +1,
         0, +1, -1,  0
    };

    int8_t dir = transitions[(encoder_last_state << 2) | state];
    if (dir != 0) {
        enc_sub_count += dir;
    }

    /* Only emit a step when encoder reaches detent (resting position) */
    if (state == 0x3) {
        if (enc_sub_count > 0) {
            encoder_diff += 1;
            input_activity_flag = true;
            buzzer_request_tick();
        } else if (enc_sub_count < 0) {
            encoder_diff -= 1;
            input_activity_flag = true;
            buzzer_request_tick();
        }
        enc_sub_count = 0;
    }

    encoder_last_state = state;
}

/* Encoder switch ISR */
static void IRAM_ATTR encoder_sw_isr() {
    unsigned long now = millis();
    if (now - last_sw_time > ENCODER_DEBOUNCE_MS) {
        bool pressed = (digitalRead(_enc_pinSw) == LOW);
        encoder_pressed = pressed;
        input_activity_flag = true;
        if (pressed) {
            press_activity_flag = true;
            buzzer_request_click();
        }
        last_sw_time = now;
    }
}

/* Track previous press state for edge detection */
static bool prev_pressed = false;

/* LVGL encoder read callback */
static void encoder_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;

    /* Read encoder rotation */
    int32_t diff = encoder_diff;
    encoder_diff = 0;

    if (raw_mode) {
        /* In raw mode, rotation goes to raw_diff — not to LVGL */
        raw_diff += diff;
        data->enc_diff = 0;
    } else {
        data->enc_diff = diff;
    }

    /* Button state: verify against physical pin to avoid stuck press state.
     * The ISR handles buzzer feedback; LVGL gets the authoritative pin reading. */
    bool pressed_now = (digitalRead(_enc_pinSw) == LOW);
    data->state = pressed_now ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

    /* Serial debug: rotation */
    if (diff != 0) {
        LOG_D("[GUI Input] Encoder rotate: %s (%d)%s", diff > 0 ? "CW" : "CCW", (int)diff, raw_mode ? " [raw]" : "");
    }

    /* Serial debug: press/release edges */
    if (pressed_now && !prev_pressed) {
        LOG_D("[GUI Input] Encoder button pressed");
    } else if (!pressed_now && prev_pressed) {
        LOG_D("[GUI Input] Encoder button released");
    }
    prev_pressed = pressed_now;
}

void gui_input_init_pins(int pinA, int pinB, int pinSw) {
    /* Store resolved pins */
    _enc_pinA  = pinA;
    _enc_pinB  = pinB;
    _enc_pinSw = pinSw;

    /* Reset volatile ISR state */
    encoder_diff = 0;
    encoder_pressed = false;
    input_activity_flag = false;
    press_activity_flag = false;
    encoder_last_state = 0;
    enc_sub_count = 0;
    last_sw_time = 0;
    raw_mode = false;
    raw_diff = 0;
    prev_pressed = false;

    /* Configure encoder pins with pullups */
    pinMode(_enc_pinA, INPUT_PULLUP);
    pinMode(_enc_pinB, INPUT_PULLUP);
    pinMode(_enc_pinSw, INPUT_PULLUP);

    /* Read initial encoder state */
    encoder_last_state = (digitalRead(_enc_pinA) << 1) | digitalRead(_enc_pinB);

    /* Attach interrupts for encoder rotation */
    attachInterrupt(digitalPinToInterrupt(_enc_pinA), encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(_enc_pinB), encoder_isr, CHANGE);

    /* Attach interrupt for encoder button */
    attachInterrupt(digitalPinToInterrupt(_enc_pinSw), encoder_sw_isr, CHANGE);

    /* Register LVGL encoder input device (only once) */
    if (!encoder_indev) {
        encoder_indev = lv_indev_create();
        lv_indev_set_type(encoder_indev, LV_INDEV_TYPE_ENCODER);
        lv_indev_set_read_cb(encoder_indev, encoder_read_cb);
        lv_indev_set_mode(encoder_indev, LV_INDEV_MODE_TIMER);
    }

    LOG_I("[GUI Input] Encoder + button initialized (A=%d, B=%d, SW=%d)", _enc_pinA, _enc_pinB, _enc_pinSw);
}

void gui_input_deinit(void) {
    detachInterrupt(digitalPinToInterrupt(_enc_pinA));
    detachInterrupt(digitalPinToInterrupt(_enc_pinB));
    detachInterrupt(digitalPinToInterrupt(_enc_pinSw));

    encoder_diff = 0;
    encoder_pressed = false;
    enc_sub_count = 0;
    raw_diff = 0;
    LOG_I("[GUI Input] Encoder ISRs detached");
}

void gui_input_init(void) {
    /* Query HAL encoder device for resolved pins (may differ from compile-time defaults) */
    int pinA = ENCODER_A_PIN, pinB = ENCODER_B_PIN, pinSw = ENCODER_SW_PIN;
    HalDeviceManager& mgr = HalDeviceManager::instance();
    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
        HalDevice* dev = mgr.getDevice(i);
        if (!dev) continue;
        if (dev->getType() == HAL_DEV_INPUT &&
            strcmp(dev->getDescriptor().compatible, "alps,ec11") == 0) {
            HalEncoder* enc = static_cast<HalEncoder*>(dev);
            pinA  = enc->getPinA();
            pinB  = enc->getPinB();
            pinSw = enc->getPinSw();
            break;
        }
    }
    gui_input_init_pins(pinA, pinB, pinSw);
}

lv_indev_t *gui_get_encoder_indev(void) {
    return encoder_indev;
}

bool gui_input_activity(void) {
    bool activity = input_activity_flag;
    input_activity_flag = false;
    return activity;
}

bool gui_input_press_activity(void) {
    bool activity = press_activity_flag;
    press_activity_flag = false;
    return activity;
}

void gui_input_set_raw_mode(bool raw) {
    raw_mode = raw;
    raw_diff = 0;
}

int32_t gui_input_get_raw_diff(void) {
    int32_t d = raw_diff;
    raw_diff = 0;
    return d;
}

#endif /* GUI_ENABLED */
