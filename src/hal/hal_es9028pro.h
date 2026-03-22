#pragma once
#ifdef DAC_ENABLED
// HalEs9028pro — ESS ES9028PRO 8-channel 32-bit DAC (HAL_DEV_DAC)
// Implements: HalEssSabreDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled: 8-channel volume (8-bit, 0.5dB/step), mute, 8 digital filter presets,
// TDM slave input from ESP32-P4 I2S master (8-slot TDM).
// Architecture: HyperStream II, 124 dB DNR, PCM up to 768kHz, DSD512.
// Register-compatible with ES9038PRO; primary difference is the modulator tier.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9028pro"
//
// TDM interleaver
// ---------------
// The ES9028PRO accepts all 8 channels on a single TDM DATA line in 8-slot mode:
//   [SLOT0=CH1][SLOT1=CH2][SLOT2=CH3][SLOT3=CH4][SLOT4=CH5][SLOT5=CH6]
//   [SLOT6=CH7][SLOT7=CH8] per frame, 32-bit per slot.
//
// This driver exposes FOUR AudioOutputSink entries to the bridge:
//   sink index 0 -> "ES9028PRO CH1/2"  (SLOT0 + SLOT1)
//   sink index 1 -> "ES9028PRO CH3/4"  (SLOT2 + SLOT3)
//   sink index 2 -> "ES9028PRO CH5/6"  (SLOT4 + SLOT5)
//   sink index 3 -> "ES9028PRO CH7/8"  (SLOT6 + SLOT7)

#include "hal_ess_sabre_dac_base.h"
#include "hal_tdm_interleaver.h"

class HalEs9028pro : public HalEssSabreDacBase {
public:
    HalEs9028pro();
    virtual ~HalEs9028pro() {}

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
