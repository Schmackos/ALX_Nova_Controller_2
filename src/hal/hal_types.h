#pragma once
// HAL Types — enums, structs, constants for the Hardware Abstraction Layer
// Phase 0: Purely additive — no existing files modified

#include <stdint.h>
#include <string.h>

// ===== Limits =====
#define HAL_MAX_DEVICES  32
#define HAL_MAX_PINS     56
#define HAL_GPIO_MAX     54   // ESP32-P4 highest valid GPIO (0-54)
#define HAL_MAX_DRIVERS  32

// ===== Device Types =====
enum HalDeviceType : uint8_t {
    HAL_DEV_NONE    = 0,
    HAL_DEV_DAC     = 1,
    HAL_DEV_ADC     = 2,
    HAL_DEV_CODEC   = 3,   // DAC + ADC combined (e.g. ES8311)
    HAL_DEV_AMP     = 4,   // Amplifier (GPIO control)
    HAL_DEV_DSP     = 5,   // External DSP chip
    HAL_DEV_SENSOR  = 6,   // Temperature, etc.
    HAL_DEV_DISPLAY = 7,   // TFT/OLED display
    HAL_DEV_INPUT   = 8,   // Rotary encoder, buttons
    HAL_DEV_GPIO    = 9,   // Generic GPIO (LED, buzzer, relay, signal gen)
};

// ===== Device Lifecycle States =====
enum HalDeviceState : uint8_t {
    HAL_STATE_UNKNOWN     = 0,   // Slot allocated, not yet probed
    HAL_STATE_DETECTED    = 1,   // I2C ACK or EEPROM found
    HAL_STATE_CONFIGURING = 2,   // Driver found, init in progress
    HAL_STATE_AVAILABLE   = 3,   // probe() + init() succeeded, _ready=true
    HAL_STATE_UNAVAILABLE = 4,   // healthCheck() failed, _ready=false
    HAL_STATE_ERROR       = 5,   // init() failed or 3 consecutive health failures
    HAL_STATE_MANUAL      = 6,   // User-configured via web UI
    HAL_STATE_REMOVED     = 7,   // Device removed or absent on rescan
};

// ===== Discovery Method =====
enum HalDiscovery : uint8_t {
    HAL_DISC_BUILTIN  = 0,   // Hardcoded onboard device
    HAL_DISC_EEPROM   = 1,   // Discovered via AT24C02 EEPROM
    HAL_DISC_GPIO_ID  = 2,   // Resistor ID on GPIO (placeholder)
    HAL_DISC_MANUAL   = 3,   // User-configured
    HAL_DISC_ONLINE   = 4,   // Fetched from GitHub YAML DB
};

// ===== Bus Types =====
enum HalBusType : uint8_t {
    HAL_BUS_NONE     = 0,
    HAL_BUS_I2C      = 1,
    HAL_BUS_I2S      = 2,
    HAL_BUS_SPI      = 3,
    HAL_BUS_GPIO     = 4,   // Direct GPIO control (e.g. NS4150B amp)
    HAL_BUS_INTERNAL = 5,   // On-chip peripheral (e.g. temp sensor)
};

// ===== I2C Bus Indices =====
#define HAL_I2C_BUS_EXT      0   // GPIO48 SDA / GPIO54 SCL — external (SDIO conflict risk)
#define HAL_I2C_BUS_ONBOARD  1   // GPIO7 SDA / GPIO8 SCL — ES8311 dedicated
#define HAL_I2C_BUS_EXP      2   // GPIO28 SDA / GPIO29 SCL — expansion (always safe)

// ===== Setup Priorities (ESPHome-inspired) =====
// Higher value = initialised first. initAll() sorts descending.
#define HAL_PRIORITY_BUS       1000   // I2C, I2S, SPI bus controllers
#define HAL_PRIORITY_IO         900   // GPIO expanders, pin allocation
#define HAL_PRIORITY_HARDWARE   800   // Audio codec/DAC/ADC hardware init
#define HAL_PRIORITY_DATA       600   // Data consumers (pipeline, metering)
#define HAL_PRIORITY_LATE       100   // Non-critical (diagnostics, logging)

// ===== Capability Bit Flags =====
// Bits 0-7: original flags (uint8_t-safe, preserved for backward compat)
#define HAL_CAP_HW_VOLUME    (1 << 0)
#define HAL_CAP_FILTERS      (1 << 1)
#define HAL_CAP_MUTE         (1 << 2)
#define HAL_CAP_ADC_PATH     (1 << 3)
#define HAL_CAP_DAC_PATH     (1 << 4)
#define HAL_CAP_PGA_CONTROL  (1 << 5)   // ADC gain knob
#define HAL_CAP_HPF_CONTROL  (1 << 6)   // ADC HPF toggle
#define HAL_CAP_CODEC        (1 << 7)   // both ADC + DAC paths
// Bits 8-11: extended flags (require uint16_t capabilities field)
#define HAL_CAP_MQA          (1 << 8)   // MQA decoder support
#define HAL_CAP_LINE_DRIVER  (1 << 9)   // Line driver outputs
#define HAL_CAP_APLL         (1 << 10)  // Asynchronous PLL
#define HAL_CAP_DSD          (1 << 11)  // DSD native playback
// Bits 12-15 reserved for future use

// ===== Bus Reference =====
struct HalBusRef {
    HalBusType type;
    uint8_t    index;    // Bus instance (0, 1, 2 for I2C)
    int        pinA;     // SDA / MOSI / DATA
    int        pinB;     // SCL / SCLK
    uint32_t   freqHz;   // Bus frequency (400000 for I2C, sample rate for I2S)
};

// ===== Pin Allocation Tracking =====
struct HalPinAlloc {
    int8_t     gpio;     // GPIO number (-1 = unused)
    HalBusType bus;
    uint8_t    busIndex;
    uint8_t    slot;     // Device slot that claimed this pin
};

// ===== Device Descriptor =====
struct HalDeviceDescriptor {
    char          compatible[32];    // "vendor,model" e.g. "ti,pcm5102a"
    char          name[33];          // Human-readable name
    char          manufacturer[33];  // "Texas Instruments"
    HalDeviceType type;
    uint16_t      legacyId;          // DAC_ID_* for backward compat (0 = none)
    uint16_t      hwVersion;
    HalBusRef     bus;
    uint8_t       i2cAddr;           // Primary I2C address (0 = none)
    uint8_t       channelCount;      // 1-8 audio channels
    uint32_t      sampleRatesMask;   // Bit per rate: b0=8k, b1=16k, b2=44.1k, b3=48k, b4=96k
    uint16_t      capabilities;      // HAL_CAP_* flags (bits 0-7 legacy, bits 8-15 extended)
    uint8_t       instanceId;        // Auto-assigned per compatible string (0-based)
    uint8_t       maxInstances;      // Max concurrent instances (0 = use default by type)
};

// ===== Per-Device Runtime Config (persisted in /hal_config.json) =====
struct HalDeviceConfig {
    bool     valid;           // Config has been set
    uint8_t  i2cAddr;         // Override I2C address (0 = use default)
    uint8_t  i2cBusIndex;     // I2C bus (0=ext, 1=onboard, 2=expansion)
    uint32_t i2cSpeedHz;      // 100000 or 400000 (0 = default)
    int8_t   pinSda;          // -1 = default
    int8_t   pinScl;          // -1 = default
    int8_t   pinMclk;         // -1 = default
    int8_t   pinData;         // -1 = default
    uint8_t  i2sPort;         // 0, 1, or 2 (255 = default)
    uint32_t sampleRate;      // Preferred sample rate (0 = auto)
    uint8_t  bitDepth;        // Preferred bit depth (0 = auto)
    uint8_t  volume;          // Initial volume 0-100
    bool     mute;            // Initial mute state
    bool     enabled;         // User enable/disable
    char     userLabel[33];   // Custom display name (empty = use descriptor name)
    // Extended config fields (Phase 4)
    uint16_t mclkMultiple;    // MCLK multiplier (e.g. 256, 384, 512)
    uint8_t  i2sFormat;       // I2S data format (0=Philips, 1=left-justified, 2=right-justified)
    uint8_t  pgaGain;         // PGA gain in dB (for ADC devices)
    bool     hpfEnabled;      // High-pass filter enable
    int8_t   paControlPin;    // Power amp control GPIO (-1 = none)
    // I2S pin overrides (I2S devices only; -1 = use board default from config.h)
    int8_t   pinBck;          // I2S bit clock GPIO
    int8_t   pinLrc;          // I2S word select / LRCLK GPIO
    int8_t   pinFmt;          // Format select GPIO (-1 = not wired; LOW=Philips, HIGH=MSB/left-justified)
    // I2S clock topology — only meaningful for HAL_BUS_I2S devices
    // true  = this device outputs MCLK/BCK/WS clocks (I2S master with clock output)
    // false = this device receives clocks only (I2S master data-only, BCK/WS/MCLK = UNUSED)
    // Default (false): only the port-0 device outputs clocks (legacy dual-master design)
    bool     isI2sClockMaster;

    // Generic GPIO pin overrides (non-I2S, non-I2C devices)
    // Devices interpret these based on their type:
    //   Encoder: gpioA=pin A, gpioB=pin B, gpioC=switch pin
    //   Buzzer/SigGen/NS4150B: gpioA=primary GPIO pin
    //   Button: gpioA=button input pin
    int8_t   gpioA;    // -1 = use compile-time default
    int8_t   gpioB;    // -1 = use compile-time default
    int8_t   gpioC;    // -1 = use compile-time default
    int8_t   gpioD;    // -1 = reserved for future use

    // USB Audio config (USB OTG pins are hardware-fixed; only identifiers configurable)
    uint16_t usbPid;   // USB Product ID (0 = use default 0x4004)

    // Device-specific filter mode (e.g., PCM5102A sharp/slow roll-off)
    uint8_t filterMode; // 0 = default

    // I2S mode for expansion mezzanine devices
    uint8_t  i2sMode;    // 0=standard stereo, 1=TDM (255 = default/auto)
    uint8_t  tdmSlots;   // TDM slot count when i2sMode=1 (2,4,8,16; 0=auto)
};

// Sample rate mask helpers
#define HAL_RATE_8K    (1 << 0)
#define HAL_RATE_16K   (1 << 1)
#define HAL_RATE_44K1  (1 << 2)
#define HAL_RATE_48K   (1 << 3)
#define HAL_RATE_96K   (1 << 4)
#define HAL_RATE_192K  (1 << 5)
#define HAL_RATE_384K  (1 << 6)
#define HAL_RATE_768K  (1 << 7)

// ===== Safe String Copy Helper =====
// Unlike strncpy, always writes a terminating '\0' even if src is longer than destSize.
inline void hal_safe_strcpy(char* dest, size_t destSize, const char* src) {
    if (destSize == 0) return;
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

// ===== HalDeviceConfig Validation =====
// Checks all configurable fields in HalDeviceConfig for out-of-range values.
// Returns on the first violation found.  Call before mgr.setConfig().

struct HalConfigError {
    bool valid;
    char field[24];
    char message[48];
};

inline HalConfigError hal_validate_config(const HalDeviceConfig& cfg) {
    HalConfigError err = {true, {0}, {0}};

    // i2sPort: 0-2 or 255 (auto/default)
    if (cfg.i2sPort != 255 && cfg.i2sPort > 2) {
        err.valid = false;
        hal_safe_strcpy(err.field,   sizeof(err.field),   "i2sPort");
        hal_safe_strcpy(err.message, sizeof(err.message), "must be 0-2 or 255 (auto)");
        return err;
    }

    // i2cBusIndex: 0-2
    if (cfg.i2cBusIndex > 2) {
        err.valid = false;
        hal_safe_strcpy(err.field,   sizeof(err.field),   "i2cBusIndex");
        hal_safe_strcpy(err.message, sizeof(err.message), "must be 0-2");
        return err;
    }

    // GPIO pin helper: -1 (unset) or 0..HAL_GPIO_MAX
    auto checkPin = [&](int16_t pin, const char* name) -> bool {
        if (pin != -1 && (pin < 0 || pin > HAL_GPIO_MAX)) {
            err.valid = false;
            hal_safe_strcpy(err.field,   sizeof(err.field),   name);
            hal_safe_strcpy(err.message, sizeof(err.message), "must be -1 or 0-54");
            return false;
        }
        return true;
    };

    if (!checkPin(cfg.pinSda,  "pinSda"))  return err;
    if (!checkPin(cfg.pinScl,  "pinScl"))  return err;
    if (!checkPin(cfg.pinMclk, "pinMclk")) return err;
    if (!checkPin(cfg.pinData, "pinData")) return err;
    if (!checkPin(cfg.pinBck,  "pinBck"))  return err;
    if (!checkPin(cfg.pinLrc,  "pinLrc"))  return err;
    if (!checkPin(cfg.pinFmt,  "pinFmt"))  return err;
    if (!checkPin(cfg.gpioA,   "gpioA"))   return err;
    if (!checkPin(cfg.gpioB,   "gpioB"))   return err;
    if (!checkPin(cfg.gpioC,   "gpioC"))   return err;
    if (!checkPin(cfg.gpioD,   "gpioD"))   return err;

    return err;  // err.valid == true
}

// ===== Descriptor Initializer Helper =====
// Fills all common HalDeviceDescriptor fields in one call, eliminating
// per-driver boilerplate (memset + 10-12 individual assignments).
// Extra driver-specific fields (e.g. _initPriority, _pin) must be set after.
inline void hal_init_descriptor(HalDeviceDescriptor& d,
    const char* compatible, const char* name, const char* manufacturer,
    HalDeviceType type, uint8_t channels, uint8_t i2cAddr,
    HalBusType busType, uint8_t busIndex,
    uint32_t ratesMask, uint16_t caps)
{
    memset(&d, 0, sizeof(d));
    hal_safe_strcpy(d.compatible, sizeof(d.compatible), compatible);
    hal_safe_strcpy(d.name, sizeof(d.name), name);
    hal_safe_strcpy(d.manufacturer, sizeof(d.manufacturer), manufacturer);
    d.type            = type;
    d.channelCount    = channels;
    d.i2cAddr         = i2cAddr;
    d.bus.type        = busType;
    d.bus.index       = busIndex;
    d.sampleRatesMask = ratesMask;
    d.capabilities    = caps;
}
