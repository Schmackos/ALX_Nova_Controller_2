#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ===== Device Information =====
#define MANUFACTURER_NAME "ALX Audio"
#define MANUFACTURER_MODEL "ALX Audio Controller"
// Serial number is now generated at runtime from eFuse MAC (see
// deviceSerialNumber in app_state.h)

// ===== Firmware Version =====
#define FIRMWARE_VERSION "1.2.5"

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
    1.0; // Default voltage threshold in volts

// Smart Sensing modes
enum SensingMode { ALWAYS_ON, ALWAYS_OFF, SMART_AUTO };

// ===== MQTT Configuration =====
const unsigned long MQTT_RECONNECT_INTERVAL = 5000; // Reconnect every 5 seconds
const unsigned long MQTT_PUBLISH_INTERVAL =
    1000; // Publish state every 1 second
const int DEFAULT_MQTT_PORT = 1883;

// ===== Hardware Stats Configuration =====
const unsigned long HARDWARE_STATS_INTERVAL =
    2000; // Broadcast hardware stats every 2 seconds

// ===== Default AP Configuration =====
#define DEFAULT_AP_PASSWORD "12345678"

// ===== FreeRTOS Task Configuration =====
#define TASK_STACK_SIZE_SENSING 4096
#define TASK_STACK_SIZE_WEB 8192
#define TASK_STACK_SIZE_MQTT 4096
#define TASK_STACK_SIZE_OTA 8192

#define TASK_PRIORITY_SENSING 2 // High priority
#define TASK_PRIORITY_WEB 1     // Medium priority
#define TASK_PRIORITY_MQTT 1    // Medium priority
#define TASK_PRIORITY_OTA 0     // Low priority

// Note: GitHub Root CA Certificate removed - now using Mozilla certificate
// bundle via ESP32CertBundle library for automatic SSL validation of all public
// servers

#endif // CONFIG_H
