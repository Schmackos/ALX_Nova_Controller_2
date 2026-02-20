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
void dac_output_init();           // Load settings, create driver, enable I2S TX
void dac_output_deinit();         // Tear down driver
void dac_output_reinit();         // Cycle I2S TX + relock DAC PLL after USB reconnect (unmutes when done)
bool dac_output_is_ready();       // Is DAC ready for samples?

// Write processed audio to I2S TX (called from audio task, non-blocking)
// buffer = interleaved 32-bit stereo I2S frames, stereo_frames = frame count
void dac_output_write(const int32_t* buffer, int stereo_frames);

// Settings persistence
void dac_load_settings();
void dac_save_settings();

// Volume update with gain recalculation + logging
void dac_update_volume(uint8_t percent);

// Periodic runtime dump (call from audio task, 5s interval)
void dac_periodic_log();

// Select a driver by device ID (returns false if not found in registry)
bool dac_select_driver(uint16_t deviceId);

// Get current driver (nullptr if none)
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

#endif // DAC_ENABLED
#endif // DAC_HAL_H
