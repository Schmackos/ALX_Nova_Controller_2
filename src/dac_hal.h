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

// ===== DAC Device IDs (EEPROM identification) =====
#define DAC_ID_NONE       0x0000
#define DAC_ID_PCM5102A   0x0001
#define DAC_ID_ES9038Q2M  0x0002
#define DAC_ID_ES9842     0x0003
#define DAC_ID_ES8311     0x0004

// ===== Volume Curve (log-perceptual) =====
// Maps 0-100 percent to 0.0-1.0 linear gain
float dac_volume_to_linear(uint8_t percent);

// ===== Software Volume =====
// Applies gain in-place to a float buffer (uses dsps_mulc_f32 on ESP32)
void dac_apply_software_volume(float* buffer, int samples, float gain);

// ===== DAC Output Manager Public API =====

// dac_boot_prepare(): one-shot boot-time init extracted from dac_output_init().
// Initialises the EEPROM mutex, loads persisted settings, computes volume gain,
// and scans the I2C bus / EEPROM for the installed DAC module.
// Safe to call multiple times (all operations are guarded by static-once flags).
void dac_boot_prepare();

// Periodic runtime dump (call from audio task, 5s interval)
void dac_periodic_log();

// I2S TX full-duplex control (slot 0 / primary port)
bool dac_enable_i2s_tx(uint32_t sampleRate);
void dac_disable_i2s_tx();

// I2S TX for a specific port (0 = primary, 2 = ES8311 I2S2)
bool dac_enable_i2s_tx_for_port(uint8_t port, uint32_t sampleRate);
void dac_disable_i2s_tx_for_port(uint8_t port);

// Query whether a given I2S TX port is currently enabled
bool dac_is_tx_enabled_for_port(uint8_t port);

// Mute ramp state accessors (for testing -- HC-6 verification)
float dac_get_mute_gain();
bool  dac_get_prev_mute();

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
