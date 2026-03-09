#ifdef DAC_ENABLED

#include "dac_hal.h"
#include "dac_registry.h"
#include "dac_eeprom.h"
#include "app_state.h"
#include "globals.h"
#include "debug_serial.h"
#include "config.h"
#include "audio_pipeline.h"
#include "audio_output_sink.h"
#include "hal/hal_dac_adapter.h"
#include "hal/hal_device_manager.h"
#include "hal/hal_pipeline_bridge.h"
#include "hal/hal_device.h"
#include "hal/hal_types.h"

#ifndef NATIVE_TEST
#include "i2s_audio.h"
#include "hal/hal_settings.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#ifdef BOARD_HAS_PSRAM
#include <esp_heap_caps.h>
#endif
// ESP-DSP vector math for software volume
extern "C" {
    #include "dsps_mulc.h"
}
#endif

// ===== Module State — Phase 1 Slot-Indexed Arrays =====
// Each index corresponds to an AUDIO_OUT_MAX_SINKS pipeline sink slot.
static DacDriver*    _driverForSlot[AUDIO_OUT_MAX_SINKS]  = {};  // Driver instance per slot
static HalDacAdapter* _adapterForSlot[AUDIO_OUT_MAX_SINKS] = {};  // HAL adapter per slot
static float         _volumeGainForSlot[AUDIO_OUT_MAX_SINKS] = {
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
};
// Which I2S write port each slot uses: 0 = i2s_audio_write (primary), 2 = i2s_audio_write_es8311
static uint8_t       _writePortForSlot[AUDIO_OUT_MAX_SINKS] = {};

// I2S TX enable state per port index (0 = primary RX+TX, 2 = ES8311 I2S2)
// Port 1 is ADC2 RX-only — never TX. Index 0/2 map to the two TX-capable ports.
static bool _i2sTxEnabledFor[3] = {};

// ===== Backward-compat module-level aliases (Phase 4 will remove these) =====
// _driver and _i2sTxEnabled reference slot 0 so legacy code paths still compile.
#define _driver        _driverForSlot[0]
#define _i2sTxEnabled  _i2sTxEnabledFor[0]
#define _volumeGain    _volumeGainForSlot[0]

// Mute ramp: prevents abrupt silence→audio or audio→silence transitions.
// Steps by 0.5f per buffer call (256 frames @ 48kHz ≈ 5.33ms each → ~10ms full ramp).
static float _muteGain = 1.0f;        // Current mute envelope (0.0 = silent, 1.0 = full)
static bool  _prevDacMute = false;    // Previous dacMute state to detect transitions
static const float MUTE_RAMP_STEP = 0.5f;

// Periodic logging state (5s interval, aligned with ADC dump)
static unsigned long _lastDacDumpMs = 0;
static uint32_t _prevTxUnderruns = 0;
static uint32_t _txWriteCount = 0;       // Total writes since last dump
static uint32_t _txBytesWritten = 0;     // Bytes actually written since last dump
static uint32_t _txBytesExpected = 0;    // Bytes expected since last dump
static int32_t _txPeakSample = 0;        // Peak absolute sample value (diagnostic)
static uint32_t _txZeroFrames = 0;       // Count of all-zero stereo frames

// ===== Volume Curve =====
// Attempt a perceptual log curve: gain = (10^(percent/50) - 1) / 99
// Gives: 0→0.0, 50→0.056, 75→0.225, 100→1.0
float dac_volume_to_linear(uint8_t percent) {
    if (percent == 0) return 0.0f;
    if (percent >= 100) return 1.0f;
    // 10^(p/50): p=0→1, p=50→10, p=100→100
    // Normalize: (10^(p/50) - 1) / 99 → 0.0 to 1.0
    float exponent = (float)percent / 50.0f;
    float power = 1.0f;
    // Manual pow10: 10^x = e^(x * ln10)
    float x = exponent * 2.302585f; // ln(10) ≈ 2.302585
    // Taylor approximation of e^x (good enough for 0-4.6 range)
    // Use iterative: e^x = 1 + x + x²/2 + x³/6 + ...
    float term = 1.0f;
    power = 1.0f;
    for (int i = 1; i <= 12; i++) {
        term *= x / (float)i;
        power += term;
    }
    return (power - 1.0f) / 99.0f;
}

// ===== Software Volume =====
void dac_apply_software_volume(float* buffer, int samples, float gain) {
    if (!buffer || samples <= 0) return;
    if (gain == 1.0f) return;  // Unity gain — skip
#if !defined(NATIVE_TEST)
    dsps_mulc_f32(buffer, buffer, samples, gain, 1, 1);
#else
    for (int i = 0; i < samples; i++) {
        buffer[i] *= gain;
    }
#endif
}

// ===== Volume Update (with logging) — slot 0 / primary DAC =====
void dac_update_volume(uint8_t percent) {
    float oldGain = _volumeGain;
    _volumeGain = dac_volume_to_linear(percent);
    if (_driver && _driver->getCapabilities().hasHardwareVolume) {
        _driver->setVolume(percent);
    }
#ifndef NATIVE_TEST
    LOG_I("[DAC] Volume: %d%% gain=%.4f (was %.4f)%s",
          percent, _volumeGain, oldGain,
          _driver && _driver->getCapabilities().hasHardwareVolume ? " [HW]" : " [SW]");
#else
    (void)oldGain;
#endif
}

// ===== Volume Update — slot-indexed =====
void dac_update_volume_for_slot(uint8_t slot, uint8_t percent) {
    if (slot >= AUDIO_OUT_MAX_SINKS) return;
    _volumeGainForSlot[slot] = dac_volume_to_linear(percent);
    DacDriver* drv = _driverForSlot[slot];
    if (drv && drv->getCapabilities().hasHardwareVolume) {
        drv->setVolume(percent);
    }
#ifndef NATIVE_TEST
    LOG_I("[DAC] Slot %u volume: %d%% gain=%.4f%s",
          slot, percent, _volumeGainForSlot[slot],
          drv && drv->getCapabilities().hasHardwareVolume ? " [HW]" : " [SW]");
#endif
}

// ===== Periodic DAC Runtime Dump =====
// Called from audio task alongside ADC periodic dump (every 5s)
void dac_periodic_log() {
#ifndef NATIVE_TEST
    AppState& as = AppState::getInstance();
    unsigned long now = millis();
    if (now - _lastDacDumpMs < 5000) return;
    _lastDacDumpMs = now;

    // Only log if DAC is enabled (avoid noise when disabled)
    if (!as.dac.enabled) return;

    uint32_t newUnderruns = as.dac.txUnderruns - _prevTxUnderruns;
    _prevTxUnderruns = as.dac.txUnderruns;

    LOG_I("[DAC] %s ready=%d vol=%d%%%s gain=%.4f wr=%lu ur=%lu(+%lu)",
          as.dac.modelName,
          as.dac.ready,
          as.dac.volume,
          as.dac.mute ? " MUTE" : "",
          _volumeGain,
          (unsigned long)_txWriteCount,
          (unsigned long)as.dac.txUnderruns,
          (unsigned long)newUnderruns);

    if (_txWriteCount > 0) {
        LOG_D("[DAC] TX: %lu writes, %luKB written / %luKB expected",
              (unsigned long)_txWriteCount,
              (unsigned long)(_txBytesWritten / 1024),
              (unsigned long)(_txBytesExpected / 1024));
        LOG_I("[DAC] TX peak=0x%08lX (%ld) zeroFrames=%lu/%lu",
              (unsigned long)_txPeakSample, (long)_txPeakSample,
              (unsigned long)_txZeroFrames, (unsigned long)_txWriteCount);
    }

    // Reset per-interval counters
    _txWriteCount = 0;
    _txBytesWritten = 0;
    _txBytesExpected = 0;
    _txPeakSample = 0;
    _txZeroFrames = 0;
#endif
}

// ===== I2S TX Full-Duplex (delegates to i2s_audio bridge) =====
#ifndef NATIVE_TEST

bool dac_enable_i2s_tx(uint32_t sampleRate) {
    if (_i2sTxEnabled) return true;
    bool ok = i2s_audio_enable_tx(sampleRate);
    if (ok) {
        _i2sTxEnabled = true;
        LOG_I("[DAC] I2S TX full-duplex enabled via bridge");
    } else {
        LOG_E("[DAC] I2S TX enable failed");
    }
    return ok;
}

void dac_disable_i2s_tx() {
    if (!_i2sTxEnabled) return;
    i2s_audio_disable_tx();
    _i2sTxEnabled = false;
    LOG_I("[DAC] I2S TX disabled via bridge");
}

#else
// Native test stubs
bool dac_enable_i2s_tx(uint32_t sampleRate) { (void)sampleRate; _i2sTxEnabled = true; return true; }
void dac_disable_i2s_tx() { _i2sTxEnabled = false; }
#endif

// ===== Helper: Resolve Pin Priority (Override > Descriptor > Default) =====
// cfg: HalDeviceConfig pointer (may be nullptr)
// descriptor_pin: value from the HAL device descriptor (-1 if not set)
// default_pin: board default from config.h
static int _dac_resolve_pin(int8_t cfg_override, int descriptor_pin, int default_pin) {
    if (cfg_override > 0) return (int)cfg_override;
    if (descriptor_pin > 0) return descriptor_pin;
    return default_pin;
}

// ===== Helper: Find Sink Slot Bound to a HAL Device =====
// Returns AUDIO_OUT_MAX_SINKS if not found.
static uint8_t _dac_find_slot_for_device(const HalDevice* dev) {
    if (!dev) return AUDIO_OUT_MAX_SINKS;
    for (uint8_t s = 0; s < AUDIO_OUT_MAX_SINKS; s++) {
        if (_adapterForSlot[s] && (HalDevice*)_adapterForSlot[s] == dev) {
            return s;
        }
    }
    return AUDIO_OUT_MAX_SINKS;
}

// ===== Helper: Enable I2S TX for a Specific Port =====
// port 0 → i2s_audio_enable_tx (primary RX+TX full-duplex)
// port 2 → i2s_audio_enable_es8311_tx (I2S2 secondary TX)
#ifndef NATIVE_TEST
static bool _dac_enable_i2s_tx_for_port(uint8_t port, uint32_t sampleRate) {
    if (port == 0) {
        if (_i2sTxEnabledFor[0]) return true;
        bool ok = i2s_audio_enable_tx(sampleRate);
        if (ok) {
            _i2sTxEnabledFor[0] = true;
            LOG_I("[DAC] I2S0 TX full-duplex enabled (port 0)");
        }
        return ok;
    }
    if (port == 2) {
        if (_i2sTxEnabledFor[2]) return true;
        bool ok = i2s_audio_enable_es8311_tx(sampleRate);
        if (ok) {
            _i2sTxEnabledFor[2] = true;
            LOG_I("[DAC] I2S2 TX enabled (port 2 / ES8311)");
        }
        return ok;
    }
    LOG_W("[DAC] Unknown I2S TX port %u", port);
    return false;
}

static void _dac_disable_i2s_tx_for_port(uint8_t port) {
    if (port == 0 && _i2sTxEnabledFor[0]) {
        i2s_audio_disable_tx();
        _i2sTxEnabledFor[0] = false;
        LOG_I("[DAC] I2S0 TX disabled (port 0)");
    } else if (port == 2 && _i2sTxEnabledFor[2]) {
        i2s_audio_disable_es8311_tx();
        _i2sTxEnabledFor[2] = false;
        LOG_I("[DAC] I2S2 TX disabled (port 2 / ES8311)");
    }
}
#else
static bool _dac_enable_i2s_tx_for_port(uint8_t port, uint32_t sampleRate) {
    (void)sampleRate;
    if (port < 3) { _i2sTxEnabledFor[port] = true; return true; }
    return false;
}
static void _dac_disable_i2s_tx_for_port(uint8_t port) {
    if (port < 3) _i2sTxEnabledFor[port] = false;
}
#endif

// ===== Core Write Helper: Write to a Single Slot =====
// Called by thunk functions and legacy dac_output_write().
// Applies volume and mute only when invoked from slot 0 (primary)
// because the mute ramp state is shared module-level for slot 0.
// For other slots, simpler per-slot gain is applied.
static void _dac_write_for_slot(uint8_t slot, const int32_t* buffer, int stereo_frames) {
    if (slot >= AUDIO_OUT_MAX_SINKS) return;
    if (!buffer || stereo_frames <= 0) return;

    uint8_t port = _writePortForSlot[slot];
    if (!_i2sTxEnabledFor[port]) return;

    DacDriver* drv = _driverForSlot[slot];
    float volGain   = _volumeGainForSlot[slot];
    int total_samples = stereo_frames * 2;

    // Decide whether software volume is needed
    bool needSoftwareVolume = true;
    if (drv && drv->getCapabilities().hasHardwareVolume) {
        needSoftwareVolume = false;
    }

    // For slot 0 only: apply shared mute ramp (maintains existing behavior)
    float effectiveMuteGain = 1.0f;
    if (slot == 0) {
        AppState& as = AppState::getInstance();
        bool muteNow = as.dac.mute || (volGain == 0.0f);
        if (muteNow != _prevDacMute) {
            _prevDacMute = muteNow;
            if (!muteNow) _muteGain = 0.0f;
        }
        if (muteNow) {
            _muteGain -= MUTE_RAMP_STEP;
            if (_muteGain <= 0.0f) {
                _muteGain = 0.0f;
                return;  // Fully muted — let tx_desc_auto_clear fill DMA with zeros
            }
        } else {
            if (_muteGain < 1.0f) {
                _muteGain += MUTE_RAMP_STEP;
                if (_muteGain > 1.0f) _muteGain = 1.0f;
            }
        }
        effectiveMuteGain = _muteGain;
    }

#ifndef NATIVE_TEST
    // Diagnostics (slot 0 only — matches existing behavior)
    if (slot == 0) {
        _txWriteCount++;
        _txBytesExpected += (uint32_t)(total_samples * sizeof(int32_t));

        bool allZero = true;
        for (int i = 0; i < total_samples && i < 64; i++) {
            int32_t absVal = buffer[i] < 0 ? -buffer[i] : buffer[i];
            if (absVal > _txPeakSample) _txPeakSample = absVal;
            if (buffer[i] != 0) allZero = false;
        }
        if (allZero) _txZeroFrames++;
    }

    float effectiveGain = volGain * effectiveMuteGain;

    // Write helper lambda (port-agnostic dispatch)
    auto do_write = [&](const void* src, size_t bytes, size_t* bw) {
        if (port == 0) {
            i2s_audio_write(src, bytes, bw, 20);
        } else if (port == 2) {
            i2s_audio_write_es8311(src, bytes, bw, 20);
        } else {
            if (bw) *bw = 0;
        }
    };

    if (needSoftwareVolume && effectiveGain < 1.0f) {
        static float    *fBuf  = nullptr;
        static int32_t  *txBuf = nullptr;
        if (!fBuf) {
            fBuf = (float *)heap_caps_calloc(512, sizeof(float), MALLOC_CAP_SPIRAM);
            if (!fBuf) fBuf = (float *)calloc(512, sizeof(float));
        }
        if (!txBuf) {
            txBuf = (int32_t *)heap_caps_calloc(512, sizeof(int32_t), MALLOC_CAP_SPIRAM);
            if (!txBuf) txBuf = (int32_t *)calloc(512, sizeof(int32_t));
        }
        if (!fBuf || !txBuf) return;

        const int32_t* src = buffer;
        int remaining = total_samples;
        while (remaining > 0) {
            int chunk = (remaining > 512) ? 512 : remaining;
            for (int i = 0; i < chunk; i++) {
                fBuf[i] = (float)src[i] / 2147483647.0f;
            }
            dac_apply_software_volume(fBuf, chunk, effectiveGain);
            for (int i = 0; i < chunk; i++) {
                txBuf[i] = (int32_t)(fBuf[i] * 2147483647.0f);
            }
            size_t bytes_written = 0;
            do_write(txBuf, (size_t)chunk * sizeof(int32_t), &bytes_written);
            if (slot == 0) {
                _txBytesWritten += bytes_written;
                if (bytes_written < (size_t)(chunk * sizeof(int32_t))) {
                    AppState::getInstance().dac.txUnderruns++;
                }
            }
            src += chunk;
            remaining -= chunk;
        }
    } else {
        size_t bytes_written = 0;
        do_write(buffer, (size_t)total_samples * sizeof(int32_t), &bytes_written);
        if (slot == 0) {
            _txBytesWritten += bytes_written;
            if (bytes_written < (size_t)(total_samples * sizeof(int32_t))) {
                AppState::getInstance().dac.txUnderruns++;
            }
        }
    }
#else
    (void)total_samples;
    (void)needSoftwareVolume;
    (void)effectiveMuteGain;
#endif
}

// ===== Thunk Implementations (Slot 0..7) =====
// These are plain functions (not lambdas) so they can populate function pointer arrays.
// Each set: one write thunk + one isReady thunk per slot.

static void _slot0_write(const int32_t *b, int f) { _dac_write_for_slot(0, b, f); }
static void _slot1_write(const int32_t *b, int f) { _dac_write_for_slot(1, b, f); }
static void _slot2_write(const int32_t *b, int f) { _dac_write_for_slot(2, b, f); }
static void _slot3_write(const int32_t *b, int f) { _dac_write_for_slot(3, b, f); }
static void _slot4_write(const int32_t *b, int f) { _dac_write_for_slot(4, b, f); }
static void _slot5_write(const int32_t *b, int f) { _dac_write_for_slot(5, b, f); }
static void _slot6_write(const int32_t *b, int f) { _dac_write_for_slot(6, b, f); }
static void _slot7_write(const int32_t *b, int f) { _dac_write_for_slot(7, b, f); }

static bool _slot0_ready() {
    return _i2sTxEnabledFor[_writePortForSlot[0]] && _driverForSlot[0] && _driverForSlot[0]->isReady()
           && AppState::getInstance().dac.enabled;
}
static bool _slot1_ready() {
    return _i2sTxEnabledFor[_writePortForSlot[1]] && _driverForSlot[1] && _driverForSlot[1]->isReady();
}
static bool _slot2_ready() {
    return _i2sTxEnabledFor[_writePortForSlot[2]] && _driverForSlot[2] && _driverForSlot[2]->isReady();
}
static bool _slot3_ready() {
    return _i2sTxEnabledFor[_writePortForSlot[3]] && _driverForSlot[3] && _driverForSlot[3]->isReady();
}
static bool _slot4_ready() {
    return _i2sTxEnabledFor[_writePortForSlot[4]] && _driverForSlot[4] && _driverForSlot[4]->isReady();
}
static bool _slot5_ready() {
    return _i2sTxEnabledFor[_writePortForSlot[5]] && _driverForSlot[5] && _driverForSlot[5]->isReady();
}
static bool _slot6_ready() {
    return _i2sTxEnabledFor[_writePortForSlot[6]] && _driverForSlot[6] && _driverForSlot[6]->isReady();
}
static bool _slot7_ready() {
    return _i2sTxEnabledFor[_writePortForSlot[7]] && _driverForSlot[7] && _driverForSlot[7]->isReady();
}

// Dispatch tables — indexed by sink slot
static void (*const _dac_slot_write_fn[AUDIO_OUT_MAX_SINKS])(const int32_t*, int) = {
    _slot0_write, _slot1_write, _slot2_write, _slot3_write,
    _slot4_write, _slot5_write, _slot6_write, _slot7_write,
};
static bool (*const _dac_slot_ready_fn[AUDIO_OUT_MAX_SINKS])() = {
    _slot0_ready, _slot1_ready, _slot2_ready, _slot3_ready,
    _slot4_ready, _slot5_ready, _slot6_ready, _slot7_ready,
};

// ===== dac_boot_prepare() =====
// Extracted from dac_output_init(): one-shot boot-time preparation.
// Initialises the EEPROM mutex, loads persisted settings (once),
// computes volume gain, and scans the I2C bus / EEPROM for device ID.
// All operations are guarded by static-once flags — safe to call multiple times.
void dac_boot_prepare() {
    // Initialize I2C mutex for thread-safe EEPROM access
    dac_eeprom_init_mutex();

    // Load persisted settings only on first boot.
    static bool _settingsLoaded = false;
    if (!_settingsLoaded) {
        _settingsLoaded = true;
        dac_load_settings();
    }

    AppState& as = AppState::getInstance();

    // Update primary slot volume gain from loaded settings
    _volumeGain = dac_volume_to_linear(as.dac.volume);
    LOG_I("[DAC] Volume gain: %d%% -> %.4f linear", as.dac.volume, _volumeGain);

#ifndef NATIVE_TEST
    // Scan I2C bus and look for EEPROM — only on first boot.
    // Skip re-scan on runtime re-init: GPIO 54 (DAC_I2C_SCL) is shared with
    // SDIO WiFi reset (CONFIG_ESP_SDIO_GPIO_RESET_SLAVE=54 on P4).
    static bool _eepromScanned = false;
    if (!_eepromScanned) {
        _eepromScanned = true;
        EepromDiag& ed = as.dac.eepromDiag;
        uint8_t eepMask = 0;
        ed.i2cTotalDevices = dac_i2c_scan(&eepMask);
        ed.i2cDevicesMask = eepMask;
        ed.scanned = true;
        ed.lastScanMs = millis();

        DacEepromData eepData;
        if (dac_eeprom_scan(&eepData, eepMask)) {
            ed.found = true;
            ed.eepromAddr = eepData.i2cAddress;
            ed.deviceId = eepData.deviceId;
            ed.hwRevision = eepData.hwRevision;
            strncpy(ed.deviceName, eepData.deviceName, 32);
            ed.deviceName[32] = '\0';
            strncpy(ed.manufacturer, eepData.manufacturer, 32);
            ed.manufacturer[32] = '\0';
            ed.maxChannels = eepData.maxChannels;
            ed.dacI2cAddress = eepData.dacI2cAddress;
            ed.flags = eepData.flags;
            ed.numSampleRates = eepData.numSampleRates;
            for (int i = 0; i < eepData.numSampleRates && i < 4; i++) {
                ed.sampleRates[i] = eepData.sampleRates[i];
            }

            // Override saved device ID with EEPROM device ID
            if (eepData.deviceId != 0 && eepData.deviceId != as.dac.deviceId) {
                LOG_I("[DAC] EEPROM auto-select: device 0x%04X -> 0x%04X",
                      as.dac.deviceId, eepData.deviceId);
                as.dac.deviceId = eepData.deviceId;
            }
        } else {
            ed.found = false;
        }
        as.markEepromDirty();
    }
#endif
}

// ===== dac_activate_for_hal() =====
// Bind a HAL device to a pipeline sink slot. Creates the DacDriver from the
// device descriptor's legacyId, inits hardware using pin config from HalDeviceConfig,
// enables I2S TX on the appropriate port, then registers an AudioOutputSink at sinkSlot.
// Behavior is entirely driven by device capabilities — no device-type branching.
bool dac_activate_for_hal(HalDevice* dev, uint8_t sinkSlot) {
    if (!dev) {
        LOG_E("[DAC] dac_activate_for_hal: null device");
        return false;
    }
    if (sinkSlot >= AUDIO_OUT_MAX_SINKS) {
        LOG_E("[DAC] dac_activate_for_hal: invalid slot %u", sinkSlot);
        return false;
    }

    // Idempotency guard: already bound to this exact device
    if (_adapterForSlot[sinkSlot] && (HalDevice*)_adapterForSlot[sinkSlot] == dev) {
        LOG_I("[DAC] Slot %u already bound to this device — re-activating", sinkSlot);
        if (_adapterForSlot[sinkSlot]) {
            _adapterForSlot[sinkSlot]->_ready = true;
            HalDeviceState oldState = _adapterForSlot[sinkSlot]->_state;
            _adapterForSlot[sinkSlot]->_state = HAL_STATE_AVAILABLE;
            hal_pipeline_state_change(dev->getSlot(), oldState, HAL_STATE_AVAILABLE);
        }
        return true;
    }

    // If a different device already occupies this slot, deactivate it first
    if (_adapterForSlot[sinkSlot] && (HalDevice*)_adapterForSlot[sinkSlot] != dev) {
        LOG_W("[DAC] Slot %u occupied by a different device — evicting", sinkSlot);
        dac_deactivate_for_hal((HalDevice*)_adapterForSlot[sinkSlot]);
    }

    const HalDeviceDescriptor& desc = dev->getDescriptor();

    {
        // Determine I2S TX port from bus configuration
        // Bus index 2 → ES8311 secondary (I2S2); all others → primary I2S0
        uint8_t port = (desc.bus.index == 2) ? 2 : 0;
        _writePortForSlot[sinkSlot] = port;

        // Resolve driver — use legacyId from descriptor, fall back to PCM5102A
        uint16_t deviceId = desc.legacyId;
        if (deviceId == DAC_ID_NONE) deviceId = DAC_ID_PCM5102A;

        // Create the driver
        if (_driverForSlot[sinkSlot]) {
            _driverForSlot[sinkSlot]->deinit();
            delete _driverForSlot[sinkSlot];
            _driverForSlot[sinkSlot] = nullptr;
        }

        const DacRegistryEntry* entry = dac_registry_find_by_id(deviceId);
        if (!entry) {
            LOG_E("[DAC] No driver for device ID 0x%04X (slot %u)", deviceId, sinkSlot);
            return false;
        }
        _driverForSlot[sinkSlot] = entry->factory();
        if (!_driverForSlot[sinkSlot]) {
            LOG_E("[DAC] Driver factory returned null (slot %u)", sinkSlot);
            return false;
        }

        // Resolve pins: HalDeviceConfig override > descriptor default > board default
#ifndef NATIVE_TEST
        HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(dev->getSlot());
#else
        HalDeviceConfig* cfg = nullptr;
#endif
        DacPinConfig pins = {
            _dac_resolve_pin(cfg ? cfg->pinData : -1,
                             desc.bus.pinA > 0 ? desc.bus.pinA : -1,
                             (port == 2) ? ES8311_I2S_DSDIN_PIN : I2S_TX_DATA_PIN),
            _dac_resolve_pin(cfg ? cfg->pinSda : -1, -1,
                             (port == 2) ? ES8311_I2C_SDA_PIN : DAC_I2C_SDA_PIN),
            _dac_resolve_pin(cfg ? cfg->pinScl : -1, -1,
                             (port == 2) ? ES8311_I2C_SCL_PIN : DAC_I2C_SCL_PIN),
            0  // Shared MCLK with ADC
        };

        if (!_driverForSlot[sinkSlot]->init(pins)) {
            LOG_E("[DAC] Driver init failed (slot %u, device '%s')", sinkSlot, desc.name);
            delete _driverForSlot[sinkSlot];
            _driverForSlot[sinkSlot] = nullptr;
            return false;
        }

        AppState& as = AppState::getInstance();
        uint32_t sampleRate = as.audio.sampleRate;
#ifndef NATIVE_TEST
        if (cfg && cfg->sampleRate > 0) sampleRate = cfg->sampleRate;
#endif

        if (!_dac_enable_i2s_tx_for_port(port, sampleRate)) {
            LOG_E("[DAC] I2S TX enable failed (slot %u, port %u)", sinkSlot, port);
            _driverForSlot[sinkSlot]->deinit();
            delete _driverForSlot[sinkSlot];
            _driverForSlot[sinkSlot] = nullptr;
            return false;
        }

        if (!_driverForSlot[sinkSlot]->configure(sampleRate, 32)) {
            LOG_W("[DAC] Driver configure failed (slot %u)", sinkSlot);
        }

        // Apply initial volume from settings
        uint8_t initVolume = 100;
#ifndef NATIVE_TEST
        if (cfg) initVolume = cfg->volume;
        else if (sinkSlot == 0) initVolume = as.dac.volume;
        else if (sinkSlot == AUDIO_SINK_SLOT_ES8311) initVolume = as.dac.es8311Volume;
#endif
        _volumeGainForSlot[sinkSlot] = dac_volume_to_linear(initVolume);
        if (_driverForSlot[sinkSlot]->getCapabilities().hasHardwareVolume) {
            _driverForSlot[sinkSlot]->setVolume(100);  // Keep HW at 0 dB; use SW curve
        }

        // Apply initial mute from settings
        bool initMute = false;
#ifndef NATIVE_TEST
        if (cfg) initMute = cfg->mute;
        else if (sinkSlot == 0) initMute = as.dac.mute;
        else if (sinkSlot == AUDIO_SINK_SLOT_ES8311) initMute = as.dac.es8311Mute;
#endif
        _driverForSlot[sinkSlot]->setMute(initMute);

        // Update AppState for slot 0 (primary DAC backward compat)
        if (sinkSlot == 0) {
            as.dac.detected = true;
            as.dac.ready = true;
            as.dac.txUnderruns = 0;
            _prevTxUnderruns = 0;
            _txWriteCount = 0;
            _txBytesWritten = 0;
            _txBytesExpected = 0;
            _lastDacDumpMs = millis();
        } else if (sinkSlot == AUDIO_SINK_SLOT_ES8311) {
            as.dac.es8311Ready = true;
        }

        // Register with HAL Device Manager (create HalDacAdapter if needed)
        HalDacAdapter* adapter = dynamic_cast<HalDacAdapter*>(dev);
        if (!adapter) {
            // Not yet an adapter — create one and (re-)register
            if (!_adapterForSlot[sinkSlot]) {
                HalDeviceDescriptor adapterDesc = desc;
                if (sinkSlot == 0 && !adapterDesc.capabilities) {
                    // Ensure DAC_PATH capability is set
                    adapterDesc.capabilities |= HAL_CAP_DAC_PATH;
                }
                adapter = new HalDacAdapter(_driverForSlot[sinkSlot], adapterDesc, true);
                int regSlot = HalDeviceManager::instance().registerDevice(adapter, HAL_DISC_MANUAL);
                if (regSlot >= 0) {
                    LOG_I("[HAL] DAC (slot %u) registered in HAL slot %d", sinkSlot, regSlot);
                }
                _adapterForSlot[sinkSlot] = adapter;
            } else {
                // Re-enable existing adapter
                _adapterForSlot[sinkSlot]->_ready = true;
                HalDeviceState oldState = _adapterForSlot[sinkSlot]->_state;
                _adapterForSlot[sinkSlot]->_state = HAL_STATE_AVAILABLE;
                hal_pipeline_state_change(dev->getSlot(), oldState, HAL_STATE_AVAILABLE);
            }
        } else {
            // Already a HalDacAdapter — re-enable
            _adapterForSlot[sinkSlot] = adapter;
            adapter->_ready = true;
            HalDeviceState oldState = adapter->_state;
            adapter->_state = HAL_STATE_AVAILABLE;
            hal_pipeline_state_change(dev->getSlot(), oldState, HAL_STATE_AVAILABLE);
        }

        LOG_I("[DAC] Activated: '%s' at slot %u, port %u, gain=%.4f",
              desc.name, sinkSlot, port, _volumeGainForSlot[sinkSlot]);
    }

    {
        // Register AudioOutputSink with the pipeline using slot-indexed API and thunks
        AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
        sink.name          = desc.name;
        sink.firstChannel  = (uint8_t)(sinkSlot * 2);  // slot 0→ch0,1 / slot 1→ch2,3 / etc.
        sink.channelCount  = 2;
        sink.write         = _dac_slot_write_fn[sinkSlot];
        sink.isReady       = _dac_slot_ready_fn[sinkSlot];
        sink.halSlot       = _adapterForSlot[sinkSlot] ? _adapterForSlot[sinkSlot]->getSlot() : 0xFF;
        audio_pipeline_set_sink((int)sinkSlot, &sink);
    }

    return true;
}

// ===== dac_deactivate_for_hal() =====
// Remove the sink and tear down hardware for a specific HAL device.
// Pauses the audio task, deinits the driver, disables I2S TX if no other slot
// uses that port, removes the AudioOutputSink from the pipeline. Idempotent.
void dac_deactivate_for_hal(HalDevice* dev) {
    if (!dev) return;

    uint8_t sinkSlot = _dac_find_slot_for_device(dev);
    if (sinkSlot == AUDIO_OUT_MAX_SINKS) {
        LOG_W("[DAC] dac_deactivate_for_hal: device not found in any slot");
        return;
    }

    AppState& as = AppState::getInstance();

#ifndef NATIVE_TEST
    // Pause audio task before touching I2S / deleting the driver
    as.audio.paused = true;
    if (as.audio.taskPausedAck) {
        xSemaphoreTake(as.audio.taskPausedAck, pdMS_TO_TICKS(50));
    }
#endif

    // Update HAL adapter state (bridge callback may already have removed the sink)
    if (_adapterForSlot[sinkSlot]) {
        _adapterForSlot[sinkSlot]->_ready = false;
        if (_adapterForSlot[sinkSlot]->_state != HAL_STATE_MANUAL) {
            HalDeviceState oldState = _adapterForSlot[sinkSlot]->_state;
            _adapterForSlot[sinkSlot]->_state = HAL_STATE_UNAVAILABLE;
            hal_pipeline_state_change(dev->getSlot(), oldState, HAL_STATE_UNAVAILABLE);
        }
    }

    // Deinit and free the driver
    if (_driverForSlot[sinkSlot]) {
        _driverForSlot[sinkSlot]->deinit();
        delete _driverForSlot[sinkSlot];
        _driverForSlot[sinkSlot] = nullptr;
    }

    // Disable I2S TX port if no other slot is still using it
    uint8_t port = _writePortForSlot[sinkSlot];
    bool portStillNeeded = false;
    for (uint8_t s = 0; s < AUDIO_OUT_MAX_SINKS; s++) {
        if (s != sinkSlot && _driverForSlot[s] && _writePortForSlot[s] == port) {
            portStillNeeded = true;
            break;
        }
    }
    if (!portStillNeeded) {
        _dac_disable_i2s_tx_for_port(port);
    }

    // Update AppState ready flags
    if (sinkSlot == 0) {
        as.dac.ready = false;
    } else if (sinkSlot == AUDIO_SINK_SLOT_ES8311) {
        as.dac.es8311Ready = false;
    }

    // Clear slot state
    _adapterForSlot[sinkSlot]  = nullptr;
    _volumeGainForSlot[sinkSlot] = 1.0f;
    _writePortForSlot[sinkSlot]  = 0;

#ifndef NATIVE_TEST
    as.audio.paused = false;
#endif
    LOG_I("[DAC] Deactivated slot %u ('%s')", sinkSlot, dev->getDescriptor().name);
}

// ===== DAC Output Write (Legacy — dispatches via slot 0 thunk) =====
void dac_output_write(const int32_t* buffer, int stereo_frames) {
    if (!_i2sTxEnabled || !buffer || stereo_frames <= 0) return;

    AppState& as = AppState::getInstance();
    int total_samples = stereo_frames * 2;

    // Apply software volume if the driver has no hardware volume
    bool needSoftwareVolume = true;
    if (_driver && _driver->getCapabilities().hasHardwareVolume) {
        needSoftwareVolume = false;
    }

    // Mute ramp: fade out/in over ~10ms instead of hard cut (prevents click)
    bool muteNow = as.dac.mute || (_volumeGain == 0.0f);
    if (muteNow != _prevDacMute) {
        _prevDacMute = muteNow;
        // On mute disengage: start from silence (prevents click if signal level jumped)
        if (!muteNow) _muteGain = 0.0f;
    }
    if (muteNow) {
        _muteGain -= MUTE_RAMP_STEP;
        if (_muteGain <= 0.0f) {
            _muteGain = 0.0f;
            return;  // Fully muted — let tx_desc_auto_clear fill DMA with zeros
        }
    } else {
        if (_muteGain < 1.0f) {
            _muteGain += MUTE_RAMP_STEP;
            if (_muteGain > 1.0f) _muteGain = 1.0f;
        }
    }

#ifndef NATIVE_TEST
    _txWriteCount++;
    size_t totalExpected = (size_t)total_samples * sizeof(int32_t);
    _txBytesExpected += totalExpected;

    // Track peak sample and zero frames for diagnostics
    {
        bool allZero = true;
        for (int i = 0; i < total_samples && i < 64; i++) {  // Check first 32 stereo frames
            int32_t absVal = buffer[i] < 0 ? -buffer[i] : buffer[i];
            if (absVal > _txPeakSample) _txPeakSample = absVal;
            if (buffer[i] != 0) allZero = false;
        }
        if (allZero) _txZeroFrames++;
    }

    // Effective gain = volume × mute ramp (both applied via software volume path)
    float effectiveGain = _volumeGain * _muteGain;
    if (needSoftwareVolume && effectiveGain < 1.0f) {
        // Convert int32 → float, apply volume, convert back
        // Buffers allocated in PSRAM to save ~4KB internal SRAM
        static float *fBuf = nullptr;     // 512 floats = 256 stereo frames
        static int32_t *txBuf = nullptr;  // 512 int32s for I2S TX
        if (!fBuf) {
            fBuf = (float *)heap_caps_calloc(512, sizeof(float), MALLOC_CAP_SPIRAM);
            if (!fBuf) fBuf = (float *)calloc(512, sizeof(float));
        }
        if (!txBuf) {
            txBuf = (int32_t *)heap_caps_calloc(512, sizeof(int32_t), MALLOC_CAP_SPIRAM);
            if (!txBuf) txBuf = (int32_t *)calloc(512, sizeof(int32_t));
        }
        if (!fBuf || !txBuf) return;  // Allocation failed

        const int32_t* src = buffer;
        int remaining = total_samples;

        while (remaining > 0) {
            int chunk = (remaining > 512) ? 512 : remaining;
            // Convert int32 to float normalized
            for (int i = 0; i < chunk; i++) {
                fBuf[i] = (float)src[i] / 2147483647.0f;
            }
            dac_apply_software_volume(fBuf, chunk, effectiveGain);
            // Convert back to int32
            for (int i = 0; i < chunk; i++) {
                txBuf[i] = (int32_t)(fBuf[i] * 2147483647.0f);
            }
            size_t bytes_written = 0;
            i2s_audio_write(txBuf, (size_t)chunk * sizeof(int32_t), &bytes_written, 20);
            _txBytesWritten += bytes_written;
            if (bytes_written < (size_t)(chunk * sizeof(int32_t))) {
                as.dac.txUnderruns++;
            }
            src += chunk;
            remaining -= chunk;
        }
    } else {
        // Unity gain — write buffer directly
        size_t bytes_written = 0;
        i2s_audio_write(buffer, (size_t)total_samples * sizeof(int32_t), &bytes_written, 20);
        _txBytesWritten += bytes_written;
        if (bytes_written < (size_t)(total_samples * sizeof(int32_t))) {
            as.dac.txUnderruns++;
        }
    }
#else
    (void)total_samples;
    (void)needSoftwareVolume;
#endif
}

// ===== Secondary DAC (ES8311 on P4) =====
#if CONFIG_IDF_TARGET_ESP32P4
#include "drivers/dac_es8311.h"
#include "drivers/es8311_regs.h"
// _secondaryDriver is now _driverForSlot[AUDIO_SINK_SLOT_ES8311]
// These aliases keep legacy code within this file compiling without changes.
#define _secondaryDriver _driverForSlot[AUDIO_SINK_SLOT_ES8311]
#define _secondaryI2sTxEnabled _i2sTxEnabledFor[2]
#define _secondaryVolumeGain _volumeGainForSlot[AUDIO_SINK_SLOT_ES8311]
#define _halSecondaryAdapter _adapterForSlot[AUDIO_SINK_SLOT_ES8311]
#endif

// Legacy sink thunks (kept so existing _primary_sink_write / _secondary_sink_write
// callers inside this file still compile — they are referenced in dac_output_init()
// and dac_secondary_init() below).
static void _primary_sink_write(const int32_t *buf, int stereoFrames) {
    dac_output_write(buf, stereoFrames);
}
static bool _primary_sink_ready() {
    return dac_output_is_ready();
}
static void _secondary_sink_write(const int32_t *buf, int stereoFrames) {
    dac_secondary_write(buf, stereoFrames);
}
static bool _secondary_sink_ready() {
    return dac_secondary_is_ready();
}

void dac_secondary_init() {
#if CONFIG_IDF_TARGET_ESP32P4
    AppState& as = AppState::getInstance();
    if (!as.dac.es8311Enabled) {
        LOG_I("[DAC] ES8311 disabled in settings, skipping secondary init");
        return;
    }

#ifndef NATIVE_TEST
    // Delegation guard: if HalEs8311 has already been initialised by the HAL
    // manager (via hal_register_builtins → initAll), skip the legacy path.
    {
        HalDevice* halDev = HalDeviceManager::instance().findByCompatible("everest-semi,es8311");
        if (!halDev) halDev = HalDeviceManager::instance().findByCompatible("evergrande,es8311");
        if (halDev && halDev->_state == HAL_STATE_AVAILABLE) {
            LOG_I("[DAC] ES8311 already initialised by HAL — skipping legacy path");
            return;
        }
    }
#endif

    _secondaryDriver = createDacEs8311();
    if (!_secondaryDriver) {
        LOG_E("[DAC] Failed to create ES8311 driver");
        return;
    }

#ifndef NATIVE_TEST
    HalDeviceConfig* _halEsCfg = hal_get_config_for_type(HAL_DEV_CODEC);
#else
    HalDeviceConfig* _halEsCfg = nullptr;
#endif
    DacPinConfig esPins = {
        (_halEsCfg && _halEsCfg->pinData > 0) ? (int)_halEsCfg->pinData : ES8311_I2S_DSDIN_PIN,
        (_halEsCfg && _halEsCfg->pinSda  > 0) ? (int)_halEsCfg->pinSda  : ES8311_I2C_SDA_PIN,
        (_halEsCfg && _halEsCfg->pinScl  > 0) ? (int)_halEsCfg->pinScl  : ES8311_I2C_SCL_PIN,
        (_halEsCfg && _halEsCfg->pinMclk > 0) ? (int)_halEsCfg->pinMclk : ES8311_I2S_MCLK_PIN
    };
    if (!_secondaryDriver->init(esPins)) {
        LOG_E("[DAC] ES8311 init failed");
        delete _secondaryDriver;
        _secondaryDriver = nullptr;
        return;
    }

    if (!i2s_audio_enable_es8311_tx(as.audio.sampleRate)) {
        LOG_E("[DAC] ES8311 I2S2 TX enable failed");
        _secondaryDriver->deinit();
        delete _secondaryDriver;
        _secondaryDriver = nullptr;
        return;
    }
    _secondaryI2sTxEnabled = true;

    if (!_secondaryDriver->configure(as.audio.sampleRate, 32)) {
        LOG_W("[DAC] ES8311 configure failed");
    }

    // Keep ES8311 hardware at 0 dB — software volume provides the perceptual
    // log curve (same as primary DAC) for consistent volume across all outputs.
    _secondaryDriver->setVolume(100);
    _secondaryDriver->setMute(as.dac.es8311Mute);
    _secondaryVolumeGain = dac_volume_to_linear(as.dac.es8311Volume);

    as.dac.es8311Ready = true;
    LOG_I("[DAC] ES8311 secondary output initialized, vol=%d%% (SW gain=%.4f)", as.dac.es8311Volume, _secondaryVolumeGain);

    // Register with HAL Device Manager
    if (!_halSecondaryAdapter) {
        HalDeviceDescriptor desc;
        memset(&desc, 0, sizeof(desc));
        strncpy(desc.compatible, "evergrande,es8311", 31);
        strncpy(desc.name, "ES8311", 32);
        strncpy(desc.manufacturer, "Evergrande", 32);
        desc.type = HAL_DEV_CODEC;
        desc.legacyId = 0x0004;  // DAC_ID_ES8311
        desc.channelCount = 2;
        desc.i2cAddr = 0x18;
        desc.bus.type = HAL_BUS_I2S;
        desc.bus.index = 2;
        desc.capabilities = HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_DAC_PATH;

        _halSecondaryAdapter = new HalDacAdapter(_secondaryDriver, desc, true);
        int slot = HalDeviceManager::instance().registerDevice(_halSecondaryAdapter, HAL_DISC_BUILTIN);
        if (slot >= 0) {
            LOG_I("[HAL] ES8311 registered in slot %d", slot);
        }
    } else {
        // Re-enable: update state — bridge callback fires and records mapping
        _halSecondaryAdapter->_ready = true;
        HalDeviceState oldState = _halSecondaryAdapter->_state;
        _halSecondaryAdapter->_state = HAL_STATE_AVAILABLE;
        hal_pipeline_state_change(_halSecondaryAdapter->getSlot(), oldState, HAL_STATE_AVAILABLE);
    }

    // Register/update pipeline output sink at fixed slot (ch 2,3)
    AudioOutputSink secondarySink = AUDIO_OUTPUT_SINK_INIT;
    secondarySink.name = "ES8311";
    secondarySink.firstChannel = 2;
    secondarySink.channelCount = 2;
    secondarySink.write = _secondary_sink_write;
    secondarySink.isReady = _secondary_sink_ready;
    secondarySink.halSlot = _halSecondaryAdapter ? _halSecondaryAdapter->getSlot() : 0xFF;
    audio_pipeline_set_sink(AUDIO_SINK_SLOT_ES8311, &secondarySink);
#endif
}

void dac_secondary_deinit() {
#if CONFIG_IDF_TARGET_ESP32P4
    AppState& as = AppState::getInstance();

    // Same race condition as primary deinit: pause audio task before
    // deleting the secondary I2S channel to avoid Core 1 preemption crash.
#ifndef NATIVE_TEST
    as.audio.paused = true;
    if (as.audio.taskPausedAck) {
        // Wait up to 50ms for audio task to acknowledge pause
        xSemaphoreTake(as.audio.taskPausedAck, pdMS_TO_TICKS(50));
    }
#endif

    // Sink was already removed by the bridge state change callback when
    // hal_apply_config() set _state = HAL_STATE_MANUAL.
    if (_halSecondaryAdapter) {
        _halSecondaryAdapter->_ready = false;
        if (_halSecondaryAdapter->_state != HAL_STATE_MANUAL) {
            HalDeviceState oldState = _halSecondaryAdapter->_state;
            _halSecondaryAdapter->_state = HAL_STATE_UNAVAILABLE;
            hal_pipeline_state_change(_halSecondaryAdapter->getSlot(), oldState, HAL_STATE_UNAVAILABLE);
        }
    }

    if (_secondaryDriver) {
        _secondaryDriver->deinit();
        delete _secondaryDriver;
        _secondaryDriver = nullptr;
    }
    if (_secondaryI2sTxEnabled) {
        i2s_audio_disable_es8311_tx();
        _secondaryI2sTxEnabled = false;
    }
    as.dac.es8311Ready = false;

#ifndef NATIVE_TEST
    as.audio.paused = false;
#endif
    LOG_I("[DAC] ES8311 secondary output deinitialized");
#endif
}

bool dac_secondary_is_ready() {
#if CONFIG_IDF_TARGET_ESP32P4
    return _secondaryI2sTxEnabled && _secondaryDriver && _secondaryDriver->isReady() &&
           AppState::getInstance().dac.es8311Enabled;
#else
    return false;
#endif
}

void dac_secondary_write(const int32_t* buffer, int stereo_frames) {
#if CONFIG_IDF_TARGET_ESP32P4
    if (!_secondaryI2sTxEnabled || !buffer || stereo_frames <= 0) return;
    if (!_secondaryDriver || !_secondaryDriver->isReady()) return;

    AppState& as = AppState::getInstance();
    int total_samples = stereo_frames * 2;

    // Mute: skip write entirely (DMA auto-clear fills zeros)
    if (as.dac.es8311Mute || _secondaryVolumeGain == 0.0f) return;

    // Apply software volume (same perceptual curve as primary DAC)
    size_t bytes_written = 0;
    if (_secondaryVolumeGain < 1.0f) {
        static float *fBuf = nullptr;
        static int32_t *txBuf = nullptr;
        if (!fBuf) {
            fBuf = (float *)heap_caps_calloc(512, sizeof(float), MALLOC_CAP_SPIRAM);
            if (!fBuf) fBuf = (float *)calloc(512, sizeof(float));
        }
        if (!txBuf) {
            txBuf = (int32_t *)heap_caps_calloc(512, sizeof(int32_t), MALLOC_CAP_SPIRAM);
            if (!txBuf) txBuf = (int32_t *)calloc(512, sizeof(int32_t));
        }
        if (!fBuf || !txBuf) return;

        const int32_t* src = buffer;
        int remaining = total_samples;
        while (remaining > 0) {
            int chunk = (remaining > 512) ? 512 : remaining;
            for (int i = 0; i < chunk; i++) {
                fBuf[i] = (float)src[i] / 2147483647.0f;
            }
            dac_apply_software_volume(fBuf, chunk, _secondaryVolumeGain);
            for (int i = 0; i < chunk; i++) {
                txBuf[i] = (int32_t)(fBuf[i] * 2147483647.0f);
            }
            size_t bw = 0;
            i2s_audio_write_es8311(txBuf, (size_t)chunk * sizeof(int32_t), &bw, 20);
            bytes_written += bw;
            src += chunk;
            remaining -= chunk;
        }
    } else {
        // Unity gain — write buffer directly
        size_t total_bytes = (size_t)total_samples * sizeof(int32_t);
        i2s_audio_write_es8311(buffer, total_bytes, &bytes_written, 20);
    }
#endif
}

void dac_secondary_set_volume(uint8_t percent) {
#if CONFIG_IDF_TARGET_ESP32P4
    if (_secondaryDriver) {
        // Software volume only — ES8311 hardware stays at 0 dB for consistent
        // perceptual curve matching the primary DAC
        _secondaryVolumeGain = dac_volume_to_linear(percent);
        LOG_I("[DAC] (ES8311) volume: %d%% (SW gain=%.4f)", percent, _secondaryVolumeGain);
    }
#endif
}

void dac_secondary_set_mute(bool mute) {
#if CONFIG_IDF_TARGET_ESP32P4
    if (_secondaryDriver) {
        _secondaryDriver->setMute(mute);
    }
#endif
}

// ===== TX Diagnostics Snapshot =====
DacTxDiag dac_get_tx_diagnostics() {
    DacTxDiag d = {};
    d.i2sTxEnabled = _i2sTxEnabled;
    d.volumeGain = _volumeGain;
    d.writeCount = _txWriteCount;
    d.bytesWritten = _txBytesWritten;
    d.bytesExpected = _txBytesExpected;
    d.peakSample = _txPeakSample;
    d.zeroFrames = _txZeroFrames;
    d.underruns = AppState::getInstance().dac.txUnderruns;
    return d;
}

// ===== Settings Persistence =====
void dac_load_settings() {
#ifndef NATIVE_TEST
    AppState& as = AppState::getInstance();

    File f = LittleFS.open("/dac_config.json", "r");
    if (!f) {
        LOG_I("[DAC] No settings file, using defaults");
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, f)) {
        LOG_W("[DAC] Settings parse error, using defaults");
        f.close();
        return;
    }
    f.close();

    if (doc["enabled"].is<bool>()) as.dac.enabled = doc["enabled"].as<bool>();
    if (doc["volume"].is<int>()) {
        int v = doc["volume"].as<int>();
        if (v >= 0 && v <= 100) as.dac.volume = (uint8_t)v;
    }
    if (doc["mute"].is<bool>()) as.dac.mute = doc["mute"].as<bool>();
    if (doc["deviceId"].is<int>()) as.dac.deviceId = (uint16_t)doc["deviceId"].as<int>();
    if (doc["modelName"].is<const char*>()) {
        strncpy(as.dac.modelName, doc["modelName"].as<const char*>(), sizeof(as.dac.modelName) - 1);
        as.dac.modelName[sizeof(as.dac.modelName) - 1] = '\0';
    }
    if (doc["filterMode"].is<int>()) as.dac.filterMode = (uint8_t)doc["filterMode"].as<int>();

    // ES8311 secondary DAC settings (P4 onboard codec)
    if (doc["es8311Enabled"].is<bool>()) as.dac.es8311Enabled = doc["es8311Enabled"].as<bool>();
    if (doc["es8311Volume"].is<int>()) {
        int v = doc["es8311Volume"].as<int>();
        if (v >= 0 && v <= 100) as.dac.es8311Volume = (uint8_t)v;
    }
    if (doc["es8311Mute"].is<bool>()) as.dac.es8311Mute = doc["es8311Mute"].as<bool>();

    _volumeGain = dac_volume_to_linear(as.dac.volume);
    LOG_I("[DAC] Settings loaded: enabled=%d vol=%d mute=%d device=0x%04X (%s) es8311=%d/%d%%/%s",
          as.dac.enabled, as.dac.volume, as.dac.mute, as.dac.deviceId, as.dac.modelName,
          as.dac.es8311Enabled, as.dac.es8311Volume, as.dac.es8311Mute ? "muted" : "unmuted");
#endif
}

// ===== Deferred DAC Save =====
static bool _dacSavePending = false;
static unsigned long _lastDacSaveRequest = 0;
static const unsigned long DAC_SAVE_DEBOUNCE_MS = 2000;

void dac_save_settings_deferred() {
    _dacSavePending = true;
    _lastDacSaveRequest = millis();
}

void dac_check_deferred_save() {
    if (_dacSavePending && (millis() - _lastDacSaveRequest >= DAC_SAVE_DEBOUNCE_MS)) {
        dac_save_settings();
        _dacSavePending = false;
    }
}

void dac_save_settings() {
#ifndef NATIVE_TEST
    AppState& as = AppState::getInstance();

    JsonDocument doc;
    doc["enabled"] = as.dac.enabled;
    doc["volume"] = as.dac.volume;
    doc["mute"] = as.dac.mute;
    doc["deviceId"] = as.dac.deviceId;
    doc["modelName"] = as.dac.modelName;
    doc["filterMode"] = as.dac.filterMode;

    // ES8311 secondary DAC settings (P4 onboard codec)
    doc["es8311Enabled"] = as.dac.es8311Enabled;
    doc["es8311Volume"] = as.dac.es8311Volume;
    doc["es8311Mute"] = as.dac.es8311Mute;

    File f = LittleFS.open("/dac_config.json", "w");
    if (!f) {
        LOG_E("[DAC] Failed to open settings file for writing");
        return;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[DAC] Settings saved");
#endif
}

// ===== Driver Management =====
bool dac_select_driver(uint16_t deviceId) {
    // Destroy existing driver
    if (_driver) {
        _driver->deinit();
        delete _driver;
        _driver = nullptr;
    }

    const DacRegistryEntry* entry = dac_registry_find_by_id(deviceId);
    if (!entry) {
        LOG_W("[DAC] No driver found for device ID 0x%04X", deviceId);
        AppState::getInstance().dac.ready = false;
        return false;
    }

    _driver = entry->factory();
    if (!_driver) {
        LOG_E("[DAC] Factory returned null for %s", entry->name);
        AppState::getInstance().dac.ready = false;
        return false;
    }

    AppState& as = AppState::getInstance();
    as.dac.deviceId = deviceId;
    strncpy(as.dac.modelName, entry->name, sizeof(as.dac.modelName) - 1);
    as.dac.modelName[sizeof(as.dac.modelName) - 1] = '\0';
    as.dac.outputChannels = _driver->getCapabilities().maxChannels;

    LOG_I("[DAC] Driver selected: %s (0x%04X)", entry->name, deviceId);
    return true;
}

DacDriver* dac_get_driver() {
    return _driver;
}

bool dac_output_is_ready() {
    return _i2sTxEnabled && _driver && _driver->isReady() &&
           AppState::getInstance().dac.enabled;
}

// ===== Legacy dac_output_init() / dac_output_deinit() =====
// Deprecated but preserved for backward compatibility until Phase 4.
// The _halPrimaryAdapter variable that used to live here is now _adapterForSlot[0].
// We use the local alias _halPrimaryAdapter defined via the macro below.
#define _halPrimaryAdapter _adapterForSlot[0]

void dac_output_init() {
    AppState& as = AppState::getInstance();

    // Boot-time one-shot preparation (settings load, EEPROM scan)
    dac_boot_prepare();

    // Update volume gain (boot_prepare already does this, but keep for clarity)
    _volumeGain = dac_volume_to_linear(as.dac.volume);
    LOG_I("[DAC] Volume gain: %d%% -> %.4f linear", as.dac.volume, _volumeGain);

    if (!as.dac.enabled) {
        LOG_I("[DAC] DAC disabled in settings, skipping init");
        return;
    }

#ifndef NATIVE_TEST
    // Delegation guard: handle HAL auto-provisioned PCM5102A placeholder.
    {
        HalDevice* halDev = HalDeviceManager::instance().findByCompatible("ti,pcm5102a");
        if (halDev) {
            if (halDev->_state == HAL_STATE_AVAILABLE) {
                LOG_I("[DAC] PCM5102A already AVAILABLE in HAL — skipping legacy path");
                return;
            }
            // If this is our own HalDacAdapter being re-enabled, keep it in its slot
            if ((HalDevice*)_halPrimaryAdapter == halDev) {
                LOG_I("[DAC] Re-enabling existing HalDacAdapter (slot %u)", halDev->getSlot());
                // Don't remove — fall through to driver reinit, reuse slot
            } else {
                // Probe-only placeholder — remove to allow HalDacAdapter registration
                uint8_t oldSlot = halDev->getSlot();
                HalDeviceManager::instance().removeDevice(oldSlot);
                LOG_I("[DAC] Removed PCM5102A placeholder (slot %u) — HalDacAdapter taking over", oldSlot);
            }
        }
    }
#endif

    // Select driver from saved device ID (default: PCM5102A)
    if (as.dac.deviceId == DAC_ID_NONE) {
        as.dac.deviceId = DAC_ID_PCM5102A;
    }

    if (!dac_select_driver(as.dac.deviceId)) {
        LOG_E("[DAC] Failed to select driver for 0x%04X, falling back to PCM5102A",
              as.dac.deviceId);
        if (!dac_select_driver(DAC_ID_PCM5102A)) {
            LOG_E("[DAC] PCM5102A fallback also failed — DAC disabled");
            as.dac.enabled = false;
            return;
        }
    }

    // Init driver — allow HAL config to override individual pins
#ifndef NATIVE_TEST
    HalDeviceConfig* _halDacCfg = hal_get_config_for_type(HAL_DEV_DAC);
#else
    HalDeviceConfig* _halDacCfg = nullptr;
#endif
    DacPinConfig pins = {
        (_halDacCfg && _halDacCfg->pinData > 0) ? (int)_halDacCfg->pinData : I2S_TX_DATA_PIN,
        (_halDacCfg && _halDacCfg->pinSda  > 0) ? (int)_halDacCfg->pinSda  : DAC_I2C_SDA_PIN,
        (_halDacCfg && _halDacCfg->pinScl  > 0) ? (int)_halDacCfg->pinScl  : DAC_I2C_SCL_PIN,
        0  // Shared MCLK with ADC
    };
    if (!_driver->init(pins)) {
        LOG_E("[DAC] Driver init failed");
        as.dac.ready = false;
        return;
    }

    // Enable I2S TX full-duplex
    if (!dac_enable_i2s_tx(as.audio.sampleRate)) {
        LOG_E("[DAC] I2S TX enable failed — DAC unavailable");
        _driver->deinit();
        as.dac.ready = false;
        return;
    }

    // Configure driver with current sample rate
    if (!_driver->configure(as.audio.sampleRate, 32)) {
        LOG_W("[DAC] Driver configure failed for %lu Hz", (unsigned long)as.audio.sampleRate);
        as.dac.ready = false;
        return;
    }

    as.dac.detected = true;
    as.dac.ready = true;
    as.dac.txUnderruns = 0;
    _prevTxUnderruns = 0;
    _txWriteCount = 0;
    _txBytesWritten = 0;
    _txBytesExpected = 0;
    _lastDacDumpMs = millis();

    const DacCapabilities& caps = _driver->getCapabilities();
    LOG_I("[DAC] Output initialized: %s by %s (0x%04X)",
          as.dac.modelName, caps.manufacturer, as.dac.deviceId);
    LOG_I("[DAC]   Rate=%luHz Ch=%d Vol=%d%% (gain=%.4f) Mute=%s",
          (unsigned long)as.audio.sampleRate, as.dac.outputChannels,
          as.dac.volume, _volumeGain, as.dac.mute ? "yes" : "no");
    LOG_I("[DAC]   HW vol=%s I2C=%s Filters=%s IndepClk=%s",
          caps.hasHardwareVolume ? "yes" : "no",
          caps.hasI2cControl ? "yes" : "no",
          caps.hasFilterModes ? "yes" : "no",
          caps.needsIndependentClock ? "yes" : "no");

    // ===== Register with HAL Device Manager =====
    if (!_halPrimaryAdapter) {
        HalDeviceDescriptor desc;
        memset(&desc, 0, sizeof(desc));
        strncpy(desc.compatible, "ti,pcm5102a", 31);
        strncpy(desc.name, as.dac.modelName, 32);
        strncpy(desc.manufacturer, caps.manufacturer ? caps.manufacturer : "Unknown", 32);
        desc.type = HAL_DEV_DAC;
        desc.legacyId = as.dac.deviceId;
        desc.channelCount = as.dac.outputChannels;
        desc.i2cAddr = caps.i2cAddress;
        desc.bus.type = HAL_BUS_I2S;
        desc.bus.index = 0;
        if (caps.hasHardwareVolume) desc.capabilities |= HAL_CAP_HW_VOLUME;
        if (caps.hasFilterModes) desc.capabilities |= HAL_CAP_FILTERS;

        _halPrimaryAdapter = new HalDacAdapter(_driver, desc, true);
        int slot = HalDeviceManager::instance().registerDevice(_halPrimaryAdapter, HAL_DISC_MANUAL);
        if (slot >= 0) {
            LOG_I("[HAL] Primary DAC registered in slot %d", slot);
        }
    } else {
        // Re-enable: update state — bridge callback fires and records mapping
        _halPrimaryAdapter->_ready = true;
        HalDeviceState oldState = _halPrimaryAdapter->_state;
        _halPrimaryAdapter->_state = HAL_STATE_AVAILABLE;
        hal_pipeline_state_change(_halPrimaryAdapter->getSlot(), oldState, HAL_STATE_AVAILABLE);
    }

    // Register/update pipeline output sink at fixed slot (ch 0,1)
    AudioOutputSink primarySink = AUDIO_OUTPUT_SINK_INIT;
    primarySink.name = as.dac.modelName;
    primarySink.firstChannel = 0;
    primarySink.channelCount = 2;
    primarySink.write = _primary_sink_write;
    primarySink.isReady = _primary_sink_ready;
    primarySink.halSlot = _halPrimaryAdapter ? _halPrimaryAdapter->getSlot() : 0xFF;
    audio_pipeline_set_sink(AUDIO_SINK_SLOT_PRIMARY, &primarySink);
}

void dac_output_deinit() {
    AppState& as = AppState::getInstance();

    // Pause the audio task BEFORE touching I2S or deleting the driver.
#ifndef NATIVE_TEST
    as.audio.paused = true;
    if (as.audio.taskPausedAck) {
        xSemaphoreTake(as.audio.taskPausedAck, pdMS_TO_TICKS(50));
    }
#endif

    // Sink was already removed by the bridge state change callback when
    // hal_apply_config() set _state = HAL_STATE_MANUAL.
    if (_halPrimaryAdapter) {
        _halPrimaryAdapter->_ready = false;
        if (_halPrimaryAdapter->_state != HAL_STATE_MANUAL) {
            HalDeviceState oldState = _halPrimaryAdapter->_state;
            _halPrimaryAdapter->_state = HAL_STATE_UNAVAILABLE;
            hal_pipeline_state_change(_halPrimaryAdapter->getSlot(), oldState, HAL_STATE_UNAVAILABLE);
        }
    }

    if (_driver) {
        _driver->deinit();
        delete _driver;
        _driver = nullptr;
    }
    dac_disable_i2s_tx();   // safe — audio task is paused
    as.dac.ready = false;

#ifndef NATIVE_TEST
    as.audio.paused = false;
#endif
    LOG_I("[DAC] Output deinitialized");
}

// Undefine local macros so they don't escape this compilation unit
#undef _halPrimaryAdapter
#undef _driver
#undef _i2sTxEnabled
#undef _volumeGain
#if CONFIG_IDF_TARGET_ESP32P4
#undef _secondaryDriver
#undef _secondaryI2sTxEnabled
#undef _secondaryVolumeGain
#undef _halSecondaryAdapter
#endif

#endif // DAC_ENABLED
