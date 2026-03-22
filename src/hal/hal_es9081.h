#pragma once
#ifdef DAC_ENABLED
// HalEs9081 — ESS ES9081 8-channel 32-bit DAC (HAL_DEV_DAC)
// Implements: HalEssSabreDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled: 8-channel volume (8-bit, 0.5dB/step), mute, 8 digital filter presets,
// TDM slave input from ESP32-P4 I2S master (8-slot TDM).
// Architecture: HyperStream IV, 120 dB DNR, 40-pin QFN package.
// Target use: cost-sensitive 8-channel designs, home theatre receivers, AVRs.
// Pin-compatible with ES9080Q for drop-in upgrades.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9081"
//
// TDM interleaver
// ---------------
// This driver exposes FOUR AudioOutputSink entries to the bridge:
//   sink index 0 -> "ES9081 CH1/2"  (SLOT0 + SLOT1)
//   sink index 1 -> "ES9081 CH3/4"  (SLOT2 + SLOT3)
//   sink index 2 -> "ES9081 CH5/6"  (SLOT4 + SLOT5)
//   sink index 3 -> "ES9081 CH7/8"  (SLOT6 + SLOT7)

#include "hal_ess_sabre_dac_base.h"
#include "hal_tdm_interleaver.h"

class HalEs9081 : public HalEssSabreDacBase {
public:
    HalEs9081();
    virtual ~HalEs9081() {}

    // HalDevice lifecycle
    bool          probe()       override;
    HalInitResult init()        override;
    void          deinit()      override;
    void          dumpConfig()  override;
    bool          healthCheck() override;

    // HalAudioDevice
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t percent)  override;
    bool setMute(bool mute)          override;
    bool setFilterPreset(uint8_t p)  override;

    // Multi-sink: 4 stereo pairs from 8 channels
    int  getSinkCount() const override { return _sinksBuilt ? 4 : 0; }
    bool buildSinkAt(int idx, uint8_t sinkSlot, AudioOutputSink* out) override;

private:
    HalTdmInterleaver _tdm;
    AudioOutputSink   _sinks[4] = {};
    bool              _sinksBuilt = false;
};

#endif // DAC_ENABLED
