#pragma once
// Unified diagnostic error codes — format 0xSSCC (SS=subsystem, CC=error)
// All subsystems share one enum for cross-system correlation and single lookup table.

#include <stdint.h>

enum DiagErrorCode : uint16_t {
    // 0x0000 = No error (success)
    DIAG_OK                             = 0x0000,

    // ===== 0x01xx: System =====
    DIAG_SYS_HEAP_CRITICAL              = 0x0101,  // Free heap < 40KB
    DIAG_SYS_HEAP_RECOVERED             = 0x0102,  // Heap above threshold again
    DIAG_SYS_PSRAM_ALLOC_FAIL           = 0x0103,  // PSRAM allocation failed
    DIAG_SYS_TASK_STACK_LOW             = 0x0104,  // Task stack watermark < 512 bytes
    DIAG_SYS_LOOP_TIME_SPIKE            = 0x0105,  // Main loop > 50ms
    DIAG_SYS_BOOT_LOOP                  = 0x0106,  // 3 consecutive crash boots
    DIAG_SYS_HEAP_WARNING               = 0x0107,  // Free heap < 50KB (early warning)
    DIAG_SYS_HEAP_WARNING_CLEARED       = 0x0108,  // Heap above warning threshold again

    // ===== 0x10xx: HAL — General =====
    DIAG_HAL_INIT_FAILED                = 0x1001,  // Device init() returned false
    DIAG_HAL_PROBE_FAILED               = 0x1002,  // Device probe() returned false
    DIAG_HAL_PIN_CONFLICT               = 0x1003,  // GPIO already claimed
    DIAG_HAL_SLOT_FULL                  = 0x1004,  // All device slots occupied (HAL_MAX_DEVICES)
    DIAG_HAL_HEALTH_FAIL                = 0x1005,  // healthCheck() returned false
    DIAG_HAL_HEALTH_RECOVERED           = 0x1006,  // healthCheck() passed after failure
    DIAG_HAL_DEVICE_REMOVED             = 0x1007,  // Device removed from registry
    DIAG_HAL_REINIT_OK                  = 0x1008,  // Re-init succeeded after retry
    DIAG_HAL_REINIT_EXHAUSTED           = 0x1009,  // Max retries exhausted → ERROR
    DIAG_HAL_DEVICE_FLAPPING            = 0x100A,  // >2 AVAIL↔UNAVAIL in 30s
    DIAG_HAL_CONFIG_INVALID             = 0x100B,  // Invalid config supplied
    DIAG_HAL_CONFIG_APPLIED             = 0x100C,  // Config changed
    DIAG_HAL_DEVICE_DETECTED            = 0x100D,  // New device found
    DIAG_HAL_TOGGLE_OVERFLOW            = 0x100E,  // Toggle queue full — request dropped
    DIAG_HAL_REGISTRY_FULL              = 0x100F,  // Driver registry full (HAL_MAX_DRIVERS)
    DIAG_HAL_DB_FULL                    = 0x1010,  // Device DB full (HAL_DB_MAX_ENTRIES)

    // ===== 0x11xx: HAL — Discovery =====
    DIAG_HAL_I2C_BUS_CONFLICT           = 0x1101,  // Bus 0 scan skipped (WiFi SDIO)
    DIAG_HAL_EEPROM_READ_ERROR          = 0x1102,  // EEPROM read failure
    DIAG_HAL_NO_DRIVER_MATCH            = 0x1103,  // EEPROM found, no matching driver
    DIAG_HAL_SCAN_CONFLICT              = 0x1104,  // Scan already in progress

    // ===== 0x20xx: Audio — I2S/ADC =====
    DIAG_AUDIO_I2S_READ_ERROR           = 0x2001,  // i2s_read() returned error
    DIAG_AUDIO_I2S_TIMEOUT              = 0x2002,  // i2s_read() timed out
    DIAG_AUDIO_NO_DATA                  = 0x2003,  // >100 consecutive zero buffers
    DIAG_AUDIO_NOISE_ONLY               = 0x2004,  // Noise floor elevated
    DIAG_AUDIO_CLIPPING                 = 0x2005,  // Sustained clip rate > 1%
    DIAG_AUDIO_HW_FAULT                 = 0x2006,  // Sustained clip rate > 30%
    DIAG_AUDIO_I2S_RECOVERY             = 0x2007,  // I2S driver restarted (storm if >3/60s)
    DIAG_AUDIO_ADC_RECOVERED            = 0x2008,  // ADC back to AUDIO_OK
    DIAG_AUDIO_SINK_WRITE_FAIL          = 0x2009,  // Sink write callback failed
    DIAG_AUDIO_SINK_UNDERRUN            = 0x200A,  // DMA TX buffer underrun
    DIAG_AUDIO_SINK_NOT_READY           = 0x200B,  // Sink skipped (not ready)
    DIAG_AUDIO_DC_OFFSET_HIGH           = 0x200C,  // DC offset > 5% sustained
    DIAG_AUDIO_PIPELINE_STALL           = 0x200D,  // NO_DATA with HAL AVAILABLE

    // ===== 0x30xx: DSP =====
    DIAG_DSP_SWAP_FAIL                  = 0x3001,  // Config swap mutex timeout
    DIAG_DSP_POOL_EXHAUSTED             = 0x3002,  // Biquad/FIR/delay pool full
    DIAG_DSP_COEFF_INVALID              = 0x3003,  // NaN or inf in coefficients
    DIAG_DSP_STAGE_ADD_FAIL             = 0x3004,  // add_stage() rollback
    DIAG_DSP_FILE_LOAD_FAIL             = 0x3005,  // JSON parse error on load
    DIAG_DSP_CPU_WARN                   = 0x3006,  // CPU load exceeded 80%
    DIAG_DSP_CPU_CRIT                   = 0x3007,  // CPU load exceeded 95%, FIR auto-bypassed

    // ===== 0x40xx: WiFi =====
    DIAG_WIFI_CONNECT_FAIL              = 0x4001,  // Connection attempt failed
    DIAG_WIFI_DISCONNECT                = 0x4002,  // Unexpected disconnection
    DIAG_WIFI_AP_FALLBACK               = 0x4003,  // Fell back to AP mode
    DIAG_WIFI_SCAN_FAIL                 = 0x4004,  // Scan returned error

    // ===== 0x50xx: MQTT =====
    DIAG_MQTT_CONNECT_FAIL              = 0x5001,  // Broker connection failed
    DIAG_MQTT_DISCONNECT                = 0x5002,  // Unexpected disconnection
    DIAG_MQTT_PUBLISH_FAIL              = 0x5003,  // Publish returned false
    DIAG_MQTT_SUBSCRIBE_FAIL            = 0x5004,  // Subscribe failed
    DIAG_MQTT_RECONNECT_FLOOD           = 0x5005,  // >5 reconnects in 60s

    // ===== 0x60xx: OTA =====
    DIAG_OTA_CHECK_FAIL                 = 0x6001,  // GitHub API request failed
    DIAG_OTA_DOWNLOAD_FAIL              = 0x6002,  // Firmware download failed
    DIAG_OTA_CHECKSUM_MISMATCH          = 0x6003,  // SHA256 verification failed
    DIAG_OTA_FLASH_FAIL                 = 0x6004,  // Update.begin() or write failed
    DIAG_OTA_CERT_FAIL                  = 0x6005,  // SSL certificate validation failed
    DIAG_OTA_NETWORK_STALL              = 0x6006,  // No data received for OTA_STALL_TIMEOUT_MS

    // ===== 0x70xx: USB Audio =====
    DIAG_USB_INIT_FAIL                  = 0x7001,  // TinyUSB init failed
    DIAG_USB_BUFFER_OVERRUN             = 0x7002,  // Ring buffer full
    DIAG_USB_BUFFER_UNDERRUN            = 0x7003,  // Ring buffer empty during read

    // Sentinel — must be last. Used by error code coverage tests.
    DIAG_CODE_COUNT
};

// Severity levels (independent of LOG_D/I/W/E log levels)
enum DiagSeverity : uint8_t {
    DIAG_SEV_INFO   = 0,   // Informational (recovery, periodic status)
    DIAG_SEV_WARN   = 1,   // Warning (degraded but functional)
    DIAG_SEV_ERROR  = 2,   // Error (subsystem failed, may self-heal)
    DIAG_SEV_CRIT   = 3,   // Critical (action required, possible data loss)
};

// Subsystem identifiers (derived from error code high byte)
enum DiagSubsystem : uint8_t {
    DIAG_SUB_SYSTEM     = 0x01,
    DIAG_SUB_HAL        = 0x10,
    DIAG_SUB_HAL_DISC   = 0x11,
    DIAG_SUB_AUDIO      = 0x20,
    DIAG_SUB_DSP        = 0x30,
    DIAG_SUB_WIFI       = 0x40,
    DIAG_SUB_MQTT       = 0x50,
    DIAG_SUB_OTA        = 0x60,
    DIAG_SUB_USB        = 0x70,
};

// Extract subsystem from error code
static inline DiagSubsystem diag_subsystem_from_code(DiagErrorCode code) {
    return (DiagSubsystem)(((uint16_t)code >> 8) & 0xFF);
}

// Subsystem name for JSON/log output
static inline const char* diag_subsystem_name(DiagSubsystem sub) {
    switch (sub) {
        case DIAG_SUB_SYSTEM:   return "System";
        case DIAG_SUB_HAL:      return "HAL";
        case DIAG_SUB_HAL_DISC: return "HAL";
        case DIAG_SUB_AUDIO:    return "Audio";
        case DIAG_SUB_DSP:      return "DSP";
        case DIAG_SUB_WIFI:     return "WiFi";
        case DIAG_SUB_MQTT:     return "MQTT";
        case DIAG_SUB_OTA:      return "OTA";
        case DIAG_SUB_USB:      return "USB";
        default:                return "Unknown";
    }
}

// Severity label for JSON/log output
static inline const char* diag_severity_char(DiagSeverity sev) {
    switch (sev) {
        case DIAG_SEV_INFO:  return "I";
        case DIAG_SEV_WARN:  return "W";
        case DIAG_SEV_ERROR: return "E";
        case DIAG_SEV_CRIT:  return "C";
        default:             return "?";
    }
}
