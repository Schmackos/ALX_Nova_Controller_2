#pragma once
#ifdef DAC_ENABLED
// HAL Custom Device — loads user-defined device schemas from LittleFS
// Schema format: /hal/custom/<compatible_name>.json
//
// Schema JSON example (Tier 2 — with init register sequence):
// {
//   "compatible": "custom,my-diy-dac",
//   "name": "My DIY DAC",
//   "type": "dac",
//   "bus": "i2c",
//   "i2cAddr": "0x48",
//   "i2cBus": 2,
//   "i2sPort": 2,
//   "channels": 2,
//   "capabilities": ["dac_path", "volume_control", "mute"],
//   "initSequence": [
//     { "reg": 0, "val": 0 },
//     { "reg": 1, "val": 8 }
//   ],
//   "defaults": { "sample_rate": 48000, "bits_per_sample": 32, "mclk_multiple": 256 }
// }

#include "hal_audio_device.h"
#include "../audio_output_sink.h"
#include "../audio_input_source.h"

// Load all custom device schemas from LittleFS /hal/custom/ directory
// and register them with HalDeviceManager.
// Safe to call multiple times — removes stale custom (HAL_DISC_MANUAL) devices first.
void hal_load_custom_devices();

// Register/update a single custom device schema JSON string.
// Validates required fields and saves to /hal/custom/<compatible>.json.
// Returns true on success.
bool hal_save_custom_schema(const char* schemaJson, char* outCompatible, size_t compatLen);

// Max init register pairs per custom device (Tier 2)
static constexpr int HAL_CUSTOM_MAX_INIT_REGS = 32;

// A single I2C register write pair used in the init sequence
struct HalInitRegPair {
    uint8_t reg;
    uint8_t val;
};

class HalCustomDevice : public HalAudioDevice {
public:
    HalCustomDevice(const char* compatible, const char* name,
                    uint16_t caps, HalBusType busType, HalDeviceType devType = HAL_DEV_DAC);
    virtual ~HalCustomDevice() {}

    bool probe() override;
    HalInitResult init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t percent) override;
    bool setMute(bool mute) override;

    // Copy up to HAL_CUSTOM_MAX_INIT_REGS pairs — extra pairs are silently discarded
    void setInitSequence(const HalInitRegPair* seq, int count);

    // Sink builder for DAC-capable custom devices (HAL_CAP_DAC_PATH)
    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override;

    // Input source descriptor for ADC-capable custom devices (HAL_CAP_ADC_PATH)
    const AudioInputSource* getInputSource() const override;

    // Configure I2C address and bus index (overrides descriptor defaults).
    // Used by hal_load_custom_devices() and tests to set runtime address/bus.
    void setI2cConfig(uint8_t addr, uint8_t busIndex) {
        _descriptor.i2cAddr = addr;
        _descriptor.bus.index = busIndex;
    }

    // Mute ramp state for click-free muting (used by write callback)
    float _muteRampState = 1.0f;

private:
    HalInitRegPair _initSeq[HAL_CUSTOM_MAX_INIT_REGS];
    int            _initSeqCount = 0;
    bool           _initialized  = false;
    uint8_t        _volume       = 100;
    bool           _muted        = false;

    // Input source registered by ADC custom devices
    AudioInputSource _inputSource;
    bool             _inputSourceValid = false;

    // I2C probe helper (Bus 0 SDIO guard included)
    bool _probeI2c();

    // Execute the init register sequence over I2C
    bool _runInitSequence();
};

#endif // DAC_ENABLED
