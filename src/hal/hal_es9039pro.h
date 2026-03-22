#pragma once
#ifdef DAC_ENABLED
// HalEs9039pro — ESS ES9039PRO / ES9039MPRO 8-channel 32-bit DAC (HAL_DEV_DAC)
// Implements: HalEssSabreDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled: 8-channel volume (8-bit, 0.5dB/step), mute, 8 digital filter presets,
// TDM slave input from ESP32-P4 I2S master (8-slot TDM).
// Architecture: HyperStream IV (4th gen modulator), 132 dB DNR, PCM up to 768kHz, DSD1024.
// Handles both PRO and MPRO package variants: auto-detected via chip ID at init.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible strings: "ess,es9039pro" / "ess,es9039mpro"
//
// Variant detection
// -----------------
// Both ES9039PRO and ES9039MPRO share the same register map. The MPRO variant is the
// industrial/automotive temperature range package. chip ID at register 0xE1 distinguishes:
//   ES9039PRO_CHIP_ID  (0x39) -> standard PRO
//   ES9039MPRO_CHIP_ID (0x3A) -> MPRO variant (descriptor name updated at init)
//
// TDM interleaver
// ---------------
// This driver exposes FOUR AudioOutputSink entries to the bridge:
//   sink index 0 -> "ES9039PRO CH1/2"  (or "ES9039MPRO CH1/2" if MPRO detected)
//   sink index 1 -> "ES9039PRO CH3/4"
//   sink index 2 -> "ES9039PRO CH5/6"
//   sink index 3 -> "ES9039PRO CH7/8"

#include "hal_ess_sabre_dac_base.h"
#include "hal_tdm_interleaver.h"

class HalEs9039pro : public HalEssSabreDacBase {
public:
    HalEs9039pro();
    virtual ~HalEs9039pro() {}

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
    bool              _isMpro     = false;  // true when ES9039MPRO chip ID detected

    // Sink name storage — updated on MPRO detection so pointers in AudioOutputSink remain valid
    char _sinkName0[32] = {};
    char _sinkName1[32] = {};
    char _sinkName2[32] = {};
    char _sinkName3[32] = {};
};

#endif // DAC_ENABLED
