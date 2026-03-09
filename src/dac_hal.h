#ifndef DAC_HAL_H
#define DAC_HAL_H

#ifdef DAC_ENABLED

#include <stdint.h>

// ===== Pin Configuration =====
#ifndef I2S_TX_DATA_PIN
#define I2S_TX_DATA_PIN 40
#endif
#ifndef DAC_I2C_SDA_PIN
#define DAC_I2C_SDA_PIN 41
#endif
#ifndef DAC_I2C_SCL_PIN
#define DAC_I2C_SCL_PIN 42
#endif

// ===== DAC Device IDs =====
#define DAC_ID_NONE       0x0000
#define DAC_ID_PCM5102A   0x0001
#define DAC_ID_ES9038Q2M  0x0002
#define DAC_ID_ES9842     0x0003
#define DAC_ID_ES8311     0x0004

// ===== DAC Capabilities =====
struct DacCapabilities {
    const char* name;              // "PCM5102A"
    const char* manufacturer;      // "Texas Instruments"
    uint16_t deviceId;             // DAC_ID_PCM5102A, etc.
    uint8_t maxChannels;           // 1-8
    bool hasHardwareVolume;
    bool hasI2cControl;
    bool needsIndependentClock;
    uint8_t i2cAddress;            // 0x00 = no I2C
    const uint32_t* supportedRates;
    uint8_t numSupportedRates;
    bool hasFilterModes;
    uint8_t numFilterModes;
};

// ===== Pin Config Passed to Driver =====
struct DacPinConfig {
    int dataOut;       // I2S TX data pin
    int i2cSda;        // I2C SDA (0 = unused)
    int i2cScl;        // I2C SCL (0 = unused)
    int mclk;          // MCLK pin (0 = shared with ADC)
};

// ===== Abstract DAC Driver =====
class DacDriver {
public:
    virtual ~DacDriver() {}
    virtual const DacCapabilities& getCapabilities() const = 0;
    virtual bool init(const DacPinConfig& pins) = 0;
    virtual void deinit() = 0;
    virtual bool configure(uint32_t sampleRate, uint8_t bitDepth) = 0;
    virtual bool setVolume(uint8_t volume) = 0;  // 0-100
    virtual bool setMute(bool mute) = 0;
    virtual bool isReady() const = 0;
    // Optional — override for DACs with digital filter selection
    virtual bool setFilterMode(uint8_t mode) { (void)mode; return false; }
    virtual const char* getFilterModeName(uint8_t mode) { (void)mode; return nullptr; }
};

// ===== Volume Curve (log-perceptual) =====
// Maps 0-100 percent to 0.0-1.0 linear gain
float dac_volume_to_linear(uint8_t percent);

// ===== Software Volume =====
// Applies gain in-place to a float buffer (uses dsps_mulc_f32 on ESP32)
void dac_apply_software_volume(float* buffer, int samples, float gain);

// ===== DAC Output Manager Public API =====

// Phase 1 — New HAL-driven API
// Forward declaration so dac_hal.h does not drag in all of hal_device.h
class HalDevice;
struct HalDeviceConfig;

// dac_boot_prepare(): one-shot boot-time init extracted from dac_output_init().
// Initialises the EEPROM mutex, loads persisted settings, computes volume gain,
// and scans the I2C bus / EEPROM for the installed DAC module.
// Safe to call multiple times (all operations are guarded by static-once flags).
void dac_boot_prepare();

// dac_activate_for_hal(): bind a HAL device to a pipeline sink slot.
// Creates the driver (from device descriptor legacyId), inits hardware,
// enables the correct I2S TX port, and registers the AudioOutputSink.
// Idempotent: calling with an already-bound device is a safe no-op.
// Returns true on success, false if driver init or I2S enable fails.
bool dac_activate_for_hal(HalDevice* dev, uint8_t sinkSlot);

// dac_deactivate_for_hal(): remove the sink and tear down hardware for a device.
// Pauses the audio task, deinits the driver, disables I2S TX, and removes
// the AudioOutputSink from the pipeline. Idempotent.
void dac_deactivate_for_hal(HalDevice* dev);

// dac_update_volume_for_slot(): update volume gain for a specific sink slot.
// If the driver for that slot has hardware volume support it is applied directly.
void dac_update_volume_for_slot(uint8_t slot, uint8_t percent);

// Legacy API — kept for backward compatibility with existing callers.
// Deprecated: prefer the slot-indexed and HAL-device variants above.
[[deprecated("use dac_boot_prepare() + dac_activate_for_hal()")]]
void dac_output_init();

[[deprecated("use dac_deactivate_for_hal()")]]
void dac_output_deinit();

bool dac_output_is_ready();

// Write processed audio to I2S TX (called from audio task, non-blocking)
// buffer = interleaved 32-bit stereo I2S frames, stereo_frames = frame count
void dac_output_write(const int32_t* buffer, int stereo_frames);

// Settings persistence
void dac_load_settings();
void dac_save_settings();
void dac_save_settings_deferred();
void dac_check_deferred_save();

// Volume update with gain recalculation + logging (operates on slot 0 / primary DAC)
void dac_update_volume(uint8_t percent);

// Periodic runtime dump (call from audio task, 5s interval)
void dac_periodic_log();

// Select a driver by device ID (returns false if not found in registry)
bool dac_select_driver(uint16_t deviceId);

// Get current driver for slot 0 (nullptr if none)
DacDriver* dac_get_driver();

// I2S TX full-duplex control (called by dac_output_init)
bool dac_enable_i2s_tx(uint32_t sampleRate);
void dac_disable_i2s_tx();

// ===== TX Diagnostics (snapshot of interval counters) =====
struct DacTxDiag {
    bool i2sTxEnabled;          // I2S TX full-duplex active
    float volumeGain;           // Current linear gain
    uint32_t writeCount;        // i2s_write() calls since last reset
    uint32_t bytesWritten;      // Bytes actually written
    uint32_t bytesExpected;     // Bytes expected to write
    int32_t  peakSample;        // Peak absolute sample value
    uint32_t zeroFrames;        // All-zero stereo frames
    uint32_t underruns;         // Cumulative TX underruns (from AppState)
};
DacTxDiag dac_get_tx_diagnostics();

// ===== Secondary DAC Output (ES8311 on P4) =====
// Independent output path — receives same audio as primary DAC
// but with its own hardware volume/mute control
[[deprecated("use dac_activate_for_hal() with ES8311 HalDevice")]]
void dac_secondary_init();

[[deprecated("use dac_deactivate_for_hal() with ES8311 HalDevice")]]
void dac_secondary_deinit();

[[deprecated("use dac_activate_for_hal()")]]
bool dac_secondary_is_ready();

[[deprecated("dispatched via slot thunk — no direct replacement needed")]]
void dac_secondary_write(const int32_t* buffer, int stereo_frames);

[[deprecated("use dac_update_volume_for_slot()")]]
void dac_secondary_set_volume(uint8_t percent);

[[deprecated("set mute via HalAudioDevice::setMute() on the ES8311 HalDevice")]]
void dac_secondary_set_mute(bool mute);

#endif // DAC_ENABLED
#endif // DAC_HAL_H
