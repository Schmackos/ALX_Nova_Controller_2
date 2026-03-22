#ifndef CONFIG_H
#define CONFIG_H

#ifdef NATIVE_TEST
#include "../test/test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Device Information =====
#define MANUFACTURER_NAME "ALX Audio"
#define MANUFACTURER_MODEL "ALX Audio Controller"
// Serial number is now generated at runtime from eFuse MAC (see
// deviceSerialNumber in app_state.h)

// ===== Firmware Version =====
#define FIRMWARE_VERSION "1.12.3"

// ===== GitHub Repository Configuration =====
#define GITHUB_REPO_OWNER "Schmackos"
#define GITHUB_REPO_NAME "ALX_Nova_Controller_2"

// ===== Pin Definitions (can be overridden by build flags in platformio.ini)
// =====
#ifndef LED_PIN
const int LED_PIN = 1; // GPIO 1 - board LED (P4)
#endif

#ifndef RESET_BUTTON_PIN
const int RESET_BUTTON_PIN = 46; // GPIO 46 - factory reset button (P4)
#endif

#ifndef AMPLIFIER_PIN
const int AMPLIFIER_PIN = 27; // GPIO 27 - relay control (P4)
#endif

// VOLTAGE_SENSE_PIN removed — replaced by PCM1808 I2S ADC (see i2s_audio.h)

// ===== I2S Audio ADC (PCM1808) Pin Definitions =====
#ifndef I2S_BCK_PIN
const int I2S_BCK_PIN = 20; // GPIO 20 - I2S Bit Clock (P4)
#endif
#ifndef I2S_DOUT_PIN
const int I2S_DOUT_PIN = 23; // GPIO 23 - I2S Data In (PCM1808 #1 OUT) (P4)
#endif
#ifndef I2S_DOUT2_PIN
const int I2S_DOUT2_PIN = 25; // GPIO 25 - I2S Data In (PCM1808 #2 OUT) (P4)
#endif
#ifndef I2S_LRC_PIN
const int I2S_LRC_PIN = 21; // GPIO 21 - I2S Word Select (L/R Clock) (P4)
#endif
#ifndef I2S_MCLK_PIN
const int I2S_MCLK_PIN = 22; // GPIO 22 - Master Clock (PLL output) (P4)
#endif

#ifndef BUZZER_PIN
const int BUZZER_PIN = 45; // GPIO 45 - passive buzzer (PWM) (P4)
#endif

// ===== Buzzer Configuration =====
#define BUZZER_PWM_CHANNEL 2   // LEDC channel (Timer 1, separate from backlight Timer 0)
#define BUZZER_PWM_RESOLUTION 8 // 8-bit resolution (0-255 duty)

// ===== Signal Generator Configuration =====
#ifndef SIGGEN_PWM_PIN
const int SIGGEN_PWM_PIN = 47;   // GPIO 47 (P4)
#endif
#define SIGGEN_MCPWM_GROUP      0          // MCPWM group 0
#define SIGGEN_MCPWM_RESOLUTION 1000000    // 1 MHz prescaled (16-bit period; 20 Hz=50000, 20 kHz=50)

// ===== DAC Output Pin Definitions =====
#ifdef DAC_ENABLED
#ifndef I2S_TX_DATA_PIN
const int I2S_TX_DATA_PIN = 24;  // GPIO 24 - I2S TX data to DAC (P4)
#endif
#ifndef DAC_I2C_SDA_PIN
const int DAC_I2C_SDA_PIN = 48;  // GPIO 48 - I2C SDA (EEPROM + I2C DACs) (P4)
#endif
#ifndef DAC_I2C_SCL_PIN
const int DAC_I2C_SCL_PIN = 54;  // GPIO 54 - I2C SCL (EEPROM + I2C DACs) (P4)
#endif
#endif // DAC_ENABLED

// ===== DSP Pipeline Configuration =====
#ifdef DSP_ENABLED
#define DSP_MAX_STAGES       24    // Max filter stages per channel (10 PEQ + 14 chain)
#define DSP_PEQ_BANDS        10    // PEQ bands occupy stages 0-9; chain stages use 10-19
#define DSP_MAX_FIR_TAPS     256   // Max FIR taps (direct convolution)
#define DSP_MAX_FIR_SLOTS    2     // Max concurrent FIR stages (pool-allocated, not inline)
#define DSP_MAX_CHANNELS     4     // L1, R1, L2, R2
#define DSP_MAX_DELAY_SLOTS  2     // Max concurrent delay stages (pool-allocated)
#define DSP_MAX_DELAY_SAMPLES 4800 // Max delay = 100ms @ 48kHz
#define DSP_DEFAULT_Q        0.707f
#define DSP_CPU_WARN_PERCENT 80.0f
#define DSP_CPU_CRIT_PERCENT 95.0f
#define DSP_PRESET_MAX_SLOTS 32    // Max number of config preset slots

// ===== Output DSP Configuration (post-matrix per-channel DSP) =====
#ifndef OUTPUT_DSP_MAX_CHANNELS
#define OUTPUT_DSP_MAX_CHANNELS 8    // One per mono output in 8x8 matrix
#endif
#ifndef OUTPUT_DSP_MAX_STAGES
#define OUTPUT_DSP_MAX_STAGES   12   // Max stages per output channel
#endif
#endif

// ===== USB Audio Configuration =====
#ifdef USB_AUDIO_ENABLED
#define USB_AUDIO_DEFAULT_SAMPLE_RATE 48000
#define USB_AUDIO_DEFAULT_BIT_DEPTH   16
#define USB_AUDIO_RING_BUFFER_MS      20    // Ring buffer capacity in ms
#define USB_AUDIO_RING_BUFFER_FRAMES  ((USB_AUDIO_DEFAULT_SAMPLE_RATE * USB_AUDIO_RING_BUFFER_MS) / 1000)
#define USB_AUDIO_TASK_STACK_SIZE     4096
#define USB_AUDIO_TASK_PRIORITY       1     // Same as main loop — must not preempt audio
#define USB_AUDIO_TASK_CORE           0     // TinyUSB task on Core 0 (separate from audio on Core 1)
#endif

// ===== HAL Discovery Retry =====
#define HAL_PROBE_RETRY_COUNT      2       // Max I2C probe retries for timeout addresses
#define HAL_PROBE_RETRY_BACKOFF_MS 50      // Base backoff ms (multiplied by attempt number)
#define HAL_PROBE_RETRY_MAX_ADDRS  16      // Max timeout addresses to track for retry

// ===== Diagnostic Journal Configuration =====
#define DIAG_JOURNAL_HOT_ENTRIES    32    // In-memory ring buffer (PSRAM, 2KB)
#define DIAG_JOURNAL_MAX_ENTRIES   800    // Persistent ring on LittleFS (64KB)
#define DIAG_FLUSH_INTERVAL_MS   60000    // Persist WARN+ entries every 60s
#define DIAG_JOURNAL_FILE "/diag_journal.bin"

// ===== Heap Health Thresholds =====
#ifndef HEAP_WARNING_THRESHOLD
#define HEAP_WARNING_THRESHOLD  50000   // 50KB — early warning (logging + rate reduction)
#endif
#ifndef HEAP_CRITICAL_THRESHOLD
#define HEAP_CRITICAL_THRESHOLD 40000   // 40KB — critical (allocation refused, features shed)
#endif

// ===== PSRAM Health Thresholds =====
#ifndef PSRAM_WARNING_THRESHOLD
#define PSRAM_WARNING_THRESHOLD   1048576   // 1MB free PSRAM — early warning
#endif
#ifndef PSRAM_CRITICAL_THRESHOLD
#define PSRAM_CRITICAL_THRESHOLD   524288   // 512KB free PSRAM — critical
#endif

// ===== Authentication =====
#define PBKDF2_ITERATIONS_V1   10000   // Legacy p1: format
#define PBKDF2_ITERATIONS      50000   // Current p2: format

// ===== WebSocket Binary Rate Scaling =====
#define WS_BINARY_SKIP_2_CLIENTS      2       // Send every 2nd binary frame for 2 clients
#define WS_BINARY_SKIP_3PLUS          4       // Send every 4th binary frame for 3-4 clients
#define WS_BINARY_SKIP_5PLUS          6       // Send every 6th binary frame for 5-7 clients
#define WS_BINARY_SKIP_8PLUS          8       // Send every 8th binary frame for 8+ clients
#define WS_AUTH_RECOUNT_INTERVAL_MS   10000   // Recalibrate auth count every 10s

// ===== Server Ports =====
const int WEB_SERVER_PORT = 80;
const int WEBSOCKET_PORT = 81;

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
    50; // Default audio/VU update rate in ms (33=30Hz, 50=20Hz, 100=10Hz)
const unsigned long DEFAULT_TIMER_DURATION =
    15; // Default timer duration in minutes
const float DEFAULT_AUDIO_THRESHOLD =
    -60.0f; // Default audio threshold in dBFS (-96 to 0)
const uint32_t DEFAULT_AUDIO_SAMPLE_RATE =
    48000; // Default I2S sample rate (Hz)
const float DEFAULT_ADC_VREF = 3.3f; // PCM1808 full-scale reference voltage (V)

// Smart Sensing modes
enum SensingMode { ALWAYS_ON, ALWAYS_OFF, SMART_AUTO };

// ===== MQTT Configuration =====
#define MQTT_SOCKET_TIMEOUT_MS 5000  // TCP socket timeout for MQTT broker connection (ms)
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
#define TASK_STACK_SIZE_AUDIO 12288

#define TASK_PRIORITY_SENSING 2 // High priority
#define TASK_PRIORITY_WEB 1     // Medium priority
#define TASK_PRIORITY_MQTT 1    // Medium priority
#define TASK_PRIORITY_OTA 0     // Low priority
#define TASK_PRIORITY_AUDIO 3   // Highest app priority (must not drop I2S samples)
#define TASK_CORE_AUDIO     1   // Core 1 — isolates audio from WiFi system tasks on Core 0

// ===== OTA Timeout Configuration =====
// All timeouts must be shorter than TWDT timeout (30s) to prevent watchdog reboot
#define OTA_STALL_TIMEOUT_MS   20000  // 20s without receiving data = network stall
#define OTA_CONNECT_TIMEOUT_MS 20000  // 20s TCP+TLS connect timeout
#define OTA_READ_TIMEOUT_MS     5000  // 5s per-read timeout on download stream

// ===== Ethernet Config Confirm Timeout =====
#define ETH_CONFIRM_TIMEOUT_MS 60000  // 60s to confirm static IP config before revert to DHCP

// ===== I2S DMA Configuration =====
#define I2S_DMA_BUF_COUNT 12    // 12 buffers x 256 frames = ~64ms runway at 48kHz
#define I2S_DMA_BUF_LEN   256

// ===== Audio Pipeline Configuration =====
#define AUDIO_PIPELINE_MAX_INPUTS  8   // Max input lanes (HAL-driven, up to 8 stereo sources)
#define AUDIO_PIPELINE_MAX_OUTPUTS 16  // Max output channels (supports up to 16 sink slots for 8ch DAC expansion)
#define AUDIO_PIPELINE_MATRIX_SIZE 32  // 32x32 routing matrix (16 sinks × 2ch = 32 output channels)
#define AUDIO_OUT_MAX_SINKS AUDIO_PIPELINE_MAX_OUTPUTS

// ===== ES8311 Onboard Codec (P4 only) =====
// Waveshare ESP32-P4-WiFi6-DEV-Kit has ES8311 DAC + NS4150B speaker amp
// I2S pins are internal to PCB (not on 40-pin header)
#ifndef ES8311_I2S_DSDIN_PIN
#define ES8311_I2S_DSDIN_PIN  9    // P4 → ES8311 DAC data
#endif
#ifndef ES8311_I2S_LRCK_PIN
#define ES8311_I2S_LRCK_PIN   10   // Word select
#endif
#ifndef ES8311_I2S_ASDOUT_PIN
#define ES8311_I2S_ASDOUT_PIN 11   // ES8311 → P4 ADC data (unused - no mic)
#endif
#ifndef ES8311_I2S_SCLK_PIN
#define ES8311_I2S_SCLK_PIN   12   // Bit clock
#endif
#ifndef ES8311_I2S_MCLK_PIN
#define ES8311_I2S_MCLK_PIN   13   // Master clock
#endif

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
#define TASK_STACK_SIZE_GUI 10240
#define TASK_PRIORITY_GUI 1
#define TASK_CORE_GUI     0     // Core 0 — moved off Core 1 to avoid audio pipeline contention

#endif // GUI_ENABLED

// Note: GitHub Root CA Certificate removed - now using Mozilla certificate
// bundle via ESP32CertBundle library for automatic SSL validation of all public
// servers

#endif // CONFIG_H
