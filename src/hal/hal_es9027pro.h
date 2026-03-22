#pragma once
#ifdef DAC_ENABLED
// HalEs9027pro — ESS ES9027PRO 8-channel 32-bit DAC (HAL_DEV_DAC)
// Implements: HalEssSabreDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled: 8-channel volume (8-bit, 0.5dB/step), mute, 8 digital filter presets,
// TDM slave input from ESP32-P4 I2S master (8-slot TDM).
// Architecture: HyperStream IV, 124 dB DNR, PCM up to 768kHz, DSD1024.
// ES9027PRO is to ES9039PRO as ES9028PRO is to ES9038PRO (same modulator generation,
// lower tier noise floor). Register map is shared with ES9039PRO.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9027pro"
//
// TDM interleaver
// ---------------
// This driver exposes FOUR AudioOutputSink entries to the bridge:
//   sink index 0 -> "ES9027PRO CH1/2"  (SLOT0 + SLOT1)
//   sink index 1 -> "ES9027PRO CH3/4"  (SLOT2 + SLOT3)
//   sink index 2 -> "ES9027PRO CH5/6"  (SLOT4 + SLOT5)
//   sink index 3 -> "ES9027PRO CH7/8"  (SLOT6 + SLOT7)

#include "hal_ess_sabre_dac_base.h"
#include "hal_tdm_interleaver.h"

class HalEs9027pro : public HalEssSabreDacBase {
public:
    HalEs9027pro();
    virtual ~HalEs9027pro() {}

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
