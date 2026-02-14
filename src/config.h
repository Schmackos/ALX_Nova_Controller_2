#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ===== Device Information =====
#define MANUFACTURER_NAME "ALX Audio"
#define MANUFACTURER_MODEL "ALX Audio Controller"
// Serial number is now generated at runtime from eFuse MAC (see
// deviceSerialNumber in app_state.h)

// ===== Firmware Version =====
#define FIRMWARE_VERSION "1.8.0"

// ===== GitHub Repository Configuration =====
#define GITHUB_REPO_OWNER "Schmackos"
#define GITHUB_REPO_NAME "ALX_Nova_Controller_2"

// ===== Pin Definitions (can be overridden by build flags in platformio.ini)
// =====
#ifndef LED_PIN
const int LED_PIN = 2; // Internal board LED pin
#endif

#ifndef RESET_BUTTON_PIN
const int RESET_BUTTON_PIN = 15; // Factory reset button
#endif

#ifndef AMPLIFIER_PIN
const int AMPLIFIER_PIN = 4; // GPIO 4 - relay control
#endif

// VOLTAGE_SENSE_PIN removed — replaced by PCM1808 I2S ADC (see i2s_audio.h)

// ===== I2S Audio ADC (PCM1808) Pin Definitions =====
#ifndef I2S_BCK_PIN
const int I2S_BCK_PIN = 16; // GPIO 16 - I2S Bit Clock
#endif
#ifndef I2S_DOUT_PIN
const int I2S_DOUT_PIN = 17; // GPIO 17 - I2S Data In (PCM1808 #1 OUT)
#endif
#ifndef I2S_DOUT2_PIN
const int I2S_DOUT2_PIN = 9;  // GPIO 9 - I2S Data In (PCM1808 #2 OUT)
#endif
#ifndef I2S_LRC_PIN
const int I2S_LRC_PIN = 18; // GPIO 18 - I2S Word Select (L/R Clock)
#endif
#ifndef I2S_MCLK_PIN
const int I2S_MCLK_PIN = 3; // GPIO 3 - Master Clock (APLL output)
#endif

#ifndef BUZZER_PIN
const int BUZZER_PIN = 8; // GPIO 8 - passive buzzer (PWM)
#endif

// ===== Buzzer Configuration =====
#define BUZZER_PWM_CHANNEL 2   // LEDC channel (Timer 1, separate from backlight Timer 0)
#define BUZZER_PWM_RESOLUTION 8 // 8-bit resolution (0-255 duty)

// ===== Signal Generator Configuration =====
#ifndef SIGGEN_PWM_PIN
const int SIGGEN_PWM_PIN = 38;   // GPIO 38 — no strapping constraints
#endif
#define SIGGEN_PWM_CHANNEL 4     // LEDC channel 4 (Timer 2)
#define SIGGEN_PWM_TIMER 2       // Separate from buzzer Timer 1 & backlight Timer 0
#define SIGGEN_PWM_RESOLUTION 10 // 10-bit (0-1023), max ~78kHz carrier

// ===== DAC Output Pin Definitions =====
#ifdef DAC_ENABLED
#ifndef I2S_TX_DATA_PIN
const int I2S_TX_DATA_PIN = 40;  // GPIO 40 - I2S TX data to DAC
#endif
#ifndef DAC_I2C_SDA_PIN
const int DAC_I2C_SDA_PIN = 41;  // GPIO 41 - I2C SDA (EEPROM + I2C DACs)
#endif
#ifndef DAC_I2C_SCL_PIN
const int DAC_I2C_SCL_PIN = 42;  // GPIO 42 - I2C SCL (EEPROM + I2C DACs)
#endif
#endif // DAC_ENABLED

// ===== DSP Pipeline Configuration =====
#ifdef DSP_ENABLED
#define DSP_MAX_STAGES       20    // Max filter stages per channel
#define DSP_PEQ_BANDS        10    // PEQ bands occupy stages 0-9; chain stages use 10-19
#define DSP_MAX_FIR_TAPS     256   // Max FIR taps (direct convolution)
#define DSP_MAX_FIR_SLOTS    2     // Max concurrent FIR stages (pool-allocated, not inline)
#define DSP_MAX_CHANNELS     4     // L1, R1, L2, R2
#define DSP_MAX_DELAY_SLOTS  2     // Max concurrent delay stages (pool-allocated)
#define DSP_MAX_DELAY_SAMPLES 4800 // Max delay = 100ms @ 48kHz
#define DSP_DEFAULT_Q        0.707f
#define DSP_CPU_WARN_PERCENT 80.0f
#endif

// ===== Server Ports =====
const int WEB_SERVER_PORT = 80;
const int WEBSOCKET_PORT = 81;

// ===== LED Timing =====
const unsigned long LED_BLINK_INTERVAL = 500; // LED blink interval in ms

// ===== Button Configuration =====
const unsigned long BTN_DEBOUNCE_TIME = 50;          // 50ms debounce
const unsigned long BTN_SHORT_PRESS_MAX = 500;       // Short press < 500ms
const unsigned long BTN_LONG_PRESS_MIN = 2000;       // Long press >= 2 seconds
const unsigned long BTN_VERY_LONG_PRESS_MIN = 10000; // Very long >= 10 seconds
const unsigned long BTN_MULTI_CLICK_WINDOW =
    400; // Time window for double/triple clicks

// Button press types
enum ButtonPressType {
  BTN_NONE,
  BTN_SHORT_PRESS,     // Quick click
  BTN_LONG_PRESS,      // Hold for 2-5 seconds
  BTN_VERY_LONG_PRESS, // Hold for 10+ seconds (factory reset)
  BTN_DOUBLE_CLICK,    // Two quick clicks --> reset the esp32
  BTN_TRIPLE_CLICK     // Three quick clicks
};

// ===== OTA Update Configuration =====
const unsigned long OTA_CHECK_INTERVAL =
    300000; // Check every 5 minutes (300000 ms)
const unsigned long AUTO_UPDATE_COUNTDOWN = 30000; // 30 seconds countdown

// ===== Smart Sensing Configuration =====
const uint16_t DEFAULT_AUDIO_UPDATE_RATE =
    50; // Default audio/VU update rate in ms (20=50Hz, 33=30Hz, 50=20Hz, 100=10Hz)
const unsigned long DEFAULT_TIMER_DURATION =
    15; // Default timer duration in minutes
const float DEFAULT_AUDIO_THRESHOLD =
    -60.0f; // Default audio threshold in dBFS (-96 to 0)
const uint32_t DEFAULT_AUDIO_SAMPLE_RATE =
    48000; // Default I2S sample rate (Hz)
const float DEFAULT_ADC_VREF = 3.3f; // PCM1808 full-scale reference voltage (V)

// ===== Dual ADC Configuration =====
#define NUM_AUDIO_ADCS 2  // Number of PCM1808 ADC modules (1 or 2)

// Smart Sensing modes
enum SensingMode { ALWAYS_ON, ALWAYS_OFF, SMART_AUTO };

// ===== MQTT Configuration =====
const unsigned long MQTT_RECONNECT_INTERVAL = 5000; // Reconnect every 5 seconds
const unsigned long MQTT_PUBLISH_INTERVAL =
    1000; // Check for state changes every 1 second
const unsigned long MQTT_HEARTBEAT_INTERVAL =
    60000; // Mandatory state publish every 60 seconds (heartbeat)
const int DEFAULT_MQTT_PORT = 1883;

// ===== Hardware Stats Configuration =====
const unsigned long HARDWARE_STATS_INTERVAL =
    2000; // Broadcast hardware stats every 2 seconds

// ===== Default AP Configuration =====
#define DEFAULT_AP_PASSWORD "12345678"

// ===== Multi-WiFi Configuration =====
#define MAX_WIFI_NETWORKS 5 // Maximum number of saved WiFi networks
#define WIFI_CONNECT_TIMEOUT                                                   \
  12000 // Connection timeout per network (12 seconds)

// ===== FreeRTOS Task Configuration =====
#define TASK_STACK_SIZE_SENSING 4096
#define TASK_STACK_SIZE_WEB 8192
#define TASK_STACK_SIZE_MQTT 4096
#define TASK_STACK_SIZE_OTA 12288
#define TASK_STACK_SIZE_AUDIO 8192

#define TASK_PRIORITY_SENSING 2 // High priority
#define TASK_PRIORITY_WEB 1     // Medium priority
#define TASK_PRIORITY_MQTT 1    // Medium priority
#define TASK_PRIORITY_OTA 0     // Low priority
#define TASK_PRIORITY_AUDIO 3   // Highest app priority (must not drop I2S samples)

// ===== GUI Configuration (TFT + Rotary Encoder) =====
#ifdef GUI_ENABLED

// TFT SPI pins (ST7735S via Hardware SPI2/FSPI)
#ifndef TFT_MOSI_PIN
#define TFT_MOSI_PIN 11
#endif
#ifndef TFT_SCLK_PIN
#define TFT_SCLK_PIN 12
#endif
#ifndef TFT_CS_PIN
#define TFT_CS_PIN 10
#endif
#ifndef TFT_DC_PIN
#define TFT_DC_PIN 13
#endif
#ifndef TFT_RST_PIN
#define TFT_RST_PIN 14
#endif
#ifndef TFT_BL_PIN
#define TFT_BL_PIN 21
#endif

// EC11 Rotary Encoder pins
#ifndef ENCODER_A_PIN
#define ENCODER_A_PIN 5
#endif
#ifndef ENCODER_B_PIN
#define ENCODER_B_PIN 6
#endif
#ifndef ENCODER_SW_PIN
#define ENCODER_SW_PIN 7
#endif

// GUI FreeRTOS task
#define TASK_STACK_SIZE_GUI 16384
#define TASK_PRIORITY_GUI 1

#endif // GUI_ENABLED

// Note: GitHub Root CA Certificate removed - now using Mozilla certificate
// bundle via ESP32CertBundle library for automatic SSL validation of all public
// servers

#endif // CONFIG_H
