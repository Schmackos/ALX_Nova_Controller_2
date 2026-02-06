#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ===== Device Information =====
#define MANUFACTURER_NAME "ALX Audio"
#define MANUFACTURER_MODEL "ALX Audio Controller"
// Serial number is now generated at runtime from eFuse MAC (see
// deviceSerialNumber in app_state.h)

// ===== Firmware Version =====
#define FIRMWARE_VERSION "1.5.3"

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

#ifndef VOLTAGE_SENSE_PIN
const int VOLTAGE_SENSE_PIN = 1; // GPIO 1 - voltage detection (ADC1_CH0)
#endif

#ifndef BUZZER_PIN
const int BUZZER_PIN = 8; // GPIO 8 - passive buzzer (PWM)
#endif

// ===== Buzzer Configuration =====
#define BUZZER_PWM_CHANNEL 2   // LEDC channel (Timer 1, separate from backlight Timer 0)
#define BUZZER_PWM_RESOLUTION 8 // 8-bit resolution (0-255 duty)

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
const unsigned long SMART_SENSING_HEARTBEAT_INTERVAL =
    1000; // Send heartbeat every 1 second for real-time voltage updates
const unsigned long DEFAULT_TIMER_DURATION =
    15; // Default timer duration in minutes
const float DEFAULT_VOLTAGE_THRESHOLD =
    0.1; // Default voltage threshold in volts

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
#define TASK_STACK_SIZE_OTA 8192

#define TASK_PRIORITY_SENSING 2 // High priority
#define TASK_PRIORITY_WEB 1     // Medium priority
#define TASK_PRIORITY_MQTT 1    // Medium priority
#define TASK_PRIORITY_OTA 0     // Low priority

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
