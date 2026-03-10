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
#include "drivers/es8311_regs.h"

#ifndef NATIVE_TEST
#include "i2s_audio.h"
#include "hal/hal_settings.h"
#include "hal/hal_device_db.h"
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

// Forward declaration — defined later in this file, called from dac_boot_prepare()
static void dac_load_settings();

// ===== Module State — Phase 1 Slot-Indexed Arrays =====
// Each index corresponds to an AUDIO_OUT_MAX_SINKS pipeline sink slot.
static DacDriver*    _driverForSlot[AUDIO_OUT_MAX_SINKS]  = {};  // Driver instance per slot
static HalDacAdapter* _adapterForSlot[AUDIO_OUT_MAX_SINKS] = {};  // HAL adapter per slot
// Which I2S write port each slot uses: 0 = i2s_audio_write (primary), 2 = i2s_audio_write_es8311
static uint8_t       _writePortForSlot[AUDIO_OUT_MAX_SINKS] = {};

// I2S TX enable state per port index (0 = primary RX+TX, 2 = ES8311 I2S2)
// Port 1 is ADC2 RX-only — never TX. Index 0/2 map to the two TX-capable ports.
static bool _i2sTxEnabledFor[3] = {};

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

// ===== Periodic DAC Runtime Dump =====
// Called from audio task alongside ADC periodic dump (every 5s)
void dac_periodic_log() {
#ifndef NATIVE_TEST
    AppState& as = AppState::getInstance();
    unsigned long now = millis();
    if (now - _lastDacDumpMs < 5000) return;
    _lastDacDumpMs = now;

    // Only log if a primary DAC driver is active
    if (!_driverForSlot[0]) return;  // No active primary DAC

    uint32_t newUnderruns = as.dac.txUnderruns - _prevTxUnderruns;
    _prevTxUnderruns = as.dac.txUnderruns;

    LOG_I("[DAC] slot0 ready=%d gain=%.4f%s wr=%lu ur=%lu(+%lu)",
          (_driverForSlot[0] && _driverForSlot[0]->isReady()) ? 1 : 0,
          audio_pipeline_get_sink_volume(0),
          audio_pipeline_is_sink_muted(0) ? " MUTE" : "",
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
    if (_i2sTxEnabledFor[0]) return true;
    bool ok = i2s_audio_enable_tx(sampleRate);
    if (ok) {
        _i2sTxEnabledFor[0] = true;
        LOG_I("[DAC] I2S TX full-duplex enabled via bridge");
    } else {
        LOG_E("[DAC] I2S TX enable failed");
    }
    return ok;
}

void dac_disable_i2s_tx() {
    if (!_i2sTxEnabledFor[0]) return;
    i2s_audio_disable_tx();
    _i2sTxEnabledFor[0] = false;
    LOG_I("[DAC] I2S TX disabled via bridge");
}

#else
// Native test stubs
bool dac_enable_i2s_tx(uint32_t sampleRate) { (void)sampleRate; _i2sTxEnabledFor[0] = true; return true; }
void dac_disable_i2s_tx() { _i2sTxEnabledFor[0] = false; }
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
    float volGain   = audio_pipeline_get_sink_volume(slot);
    int total_samples = stereo_frames * 2;

    // Decide whether software volume is needed
    bool needSoftwareVolume = true;
    if (drv && drv->getCapabilities().hasHardwareVolume) {
        needSoftwareVolume = false;
    }

    // For slot 0 only: apply shared mute ramp (maintains existing behavior)
    float effectiveMuteGain = 1.0f;
    if (slot == 0) {
        bool muteNow = audio_pipeline_is_sink_muted(slot) || (volGain == 0.0f);
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
    return _i2sTxEnabledFor[_writePortForSlot[0]] && _driverForSlot[0] && _driverForSlot[0]->isReady();
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

    // Pre-compute slot 0 volume gain from HAL config
    uint8_t bootVolume = 80;  // default
#ifndef NATIVE_TEST
    HalDevice* pcmBoot = HalDeviceManager::instance().findByCompatible("ti,pcm5102a");
    HalDeviceConfig* pcmBootCfg = pcmBoot ? HalDeviceManager::instance().getConfig(pcmBoot->getSlot()) : nullptr;
    if (pcmBootCfg) bootVolume = pcmBootCfg->volume;
#endif
    audio_pipeline_set_sink_volume(0, dac_volume_to_linear(bootVolume));
    LOG_I("[DAC] Volume gain: %d%% -> %.4f linear", bootVolume, audio_pipeline_get_sink_volume(0));

    AppState& as = AppState::getInstance();

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

            // EEPROM device ID is stored in ed.deviceId (set above)
            if (eepData.deviceId != 0) {
                LOG_I("[DAC] EEPROM: device ID 0x%04X detected", eepData.deviceId);
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
            0,   // Shared MCLK with ADC
            (cfg && cfg->paControlPin > 0) ? (int)cfg->paControlPin : -1  // PA enable GPIO
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
        uint8_t initVolume = (cfg) ? cfg->volume : 100;
        audio_pipeline_set_sink_volume(sinkSlot, dac_volume_to_linear(initVolume));
        if (_driverForSlot[sinkSlot]->getCapabilities().hasHardwareVolume) {
            _driverForSlot[sinkSlot]->setVolume(100);  // Keep HW at 0 dB; use SW curve
        }

        // Apply initial mute from settings
        bool initMute = (cfg) ? cfg->mute : false;
        audio_pipeline_set_sink_muted(sinkSlot, initMute);
        _driverForSlot[sinkSlot]->setMute(initMute);

        // Reset per-slot diagnostic counters
        if (sinkSlot == 0) {
            _prevTxUnderruns = 0;
            _txWriteCount = 0;
            _txBytesWritten = 0;
            _txBytesExpected = 0;
            _lastDacDumpMs = millis();
        }

        // Register with HAL Device Manager (create HalDacAdapter if needed).
        // Check if the passed device is the existing adapter for this slot
        // (pointer comparison replaces dynamic_cast since RTTI is disabled).
        HalDacAdapter* existingAdapter = _adapterForSlot[sinkSlot];
        if (existingAdapter && static_cast<HalDevice*>(existingAdapter) == dev) {
            // Already a HalDacAdapter — re-enable
            existingAdapter->_ready = true;
            HalDeviceState oldState = existingAdapter->_state;
            existingAdapter->_state = HAL_STATE_AVAILABLE;
            hal_pipeline_state_change(dev->getSlot(), oldState, HAL_STATE_AVAILABLE);
        } else if (existingAdapter) {
            // Re-enable existing adapter (dev is a different HalDevice)
            existingAdapter->_ready = true;
            HalDeviceState oldState = existingAdapter->_state;
            existingAdapter->_state = HAL_STATE_AVAILABLE;
            hal_pipeline_state_change(dev->getSlot(), oldState, HAL_STATE_AVAILABLE);
        } else {
            // No adapter for this slot yet — create one and register
            HalDeviceDescriptor adapterDesc = desc;
            if (sinkSlot == 0 && !adapterDesc.capabilities) {
                // Ensure DAC_PATH capability is set
                adapterDesc.capabilities |= HAL_CAP_DAC_PATH;
            }
            HalDacAdapter* adapter = new HalDacAdapter(_driverForSlot[sinkSlot], adapterDesc, true);
            int regSlot = HalDeviceManager::instance().registerDevice(adapter, HAL_DISC_MANUAL);
            if (regSlot >= 0) {
                LOG_I("[HAL] DAC (slot %u) registered in HAL slot %d", sinkSlot, regSlot);
            }
            _adapterForSlot[sinkSlot] = adapter;
        }

        LOG_I("[DAC] Activated: '%s' at slot %u, port %u, gain=%.4f",
              desc.name, sinkSlot, port, audio_pipeline_get_sink_volume(sinkSlot));
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

    // Clear slot state
    _adapterForSlot[sinkSlot]  = nullptr;
    audio_pipeline_set_sink_volume(sinkSlot, 1.0f);
    _writePortForSlot[sinkSlot]  = 0;
    audio_pipeline_set_sink_muted(sinkSlot, false);

    // HC-6: Reset mute ramp state to prevent stale gain on re-enable
    if (sinkSlot == 0) {
        _muteGain = 1.0f;
        _prevDacMute = false;
    }

#ifndef NATIVE_TEST
    as.audio.paused = false;
#endif
    LOG_I("[DAC] Deactivated slot %u ('%s')", sinkSlot, dev->getDescriptor().name);
}

// ===== TX Diagnostics Snapshot =====
DacTxDiag dac_get_tx_diagnostics() {
    DacTxDiag d = {};
    d.i2sTxEnabled = _i2sTxEnabledFor[0];
    d.volumeGain = audio_pipeline_get_sink_volume(0);
    d.writeCount = _txWriteCount;
    d.bytesWritten = _txBytesWritten;
    d.bytesExpected = _txBytesExpected;
    d.peakSample = _txPeakSample;
    d.zeroFrames = _txZeroFrames;
    d.underruns = AppState::getInstance().dac.txUnderruns;
    return d;
}

// ===== Settings Persistence =====
// One-time migration: /dac_config.json → /hal_config.json
// If the legacy file exists, read it, map fields to HAL device configs,
// save HAL config, and delete the legacy file. Subsequent boots skip this.
static void dac_load_settings() {
#ifndef NATIVE_TEST
    if (!LittleFS.exists("/dac_config.json")) return;

    File f = LittleFS.open("/dac_config.json", "r");
    if (!f) return;

    JsonDocument doc;
    if (deserializeJson(doc, f)) {
        LOG_W("[DAC] Legacy settings parse error, skipping migration");
        f.close();
        return;
    }
    f.close();

    HalDeviceManager& mgr = HalDeviceManager::instance();

    // Migrate PCM5102A settings
    HalDevice* pcm = mgr.findByCompatible("ti,pcm5102a");
    if (pcm) {
        HalDeviceConfig* cfg = mgr.getConfig(pcm->getSlot());
        if (cfg) {
            if (doc["enabled"].is<bool>()) cfg->enabled = doc["enabled"].as<bool>();
            if (doc["volume"].is<int>()) {
                int v = doc["volume"].as<int>();
                if (v >= 0 && v <= 100) cfg->volume = (uint8_t)v;
            }
            if (doc["mute"].is<bool>()) cfg->mute = doc["mute"].as<bool>();
            hal_save_device_config(pcm->getSlot());
            LOG_I("[DAC] Migrated PCM5102A: en=%d vol=%d mute=%d",
                  cfg->enabled, cfg->volume, cfg->mute);
        }
    }

    // Migrate ES8311 settings
    HalDevice* es = mgr.findByCompatible("everest-semi,es8311");
    if (es) {
        HalDeviceConfig* cfg = mgr.getConfig(es->getSlot());
        if (cfg) {
            if (doc["es8311Enabled"].is<bool>()) cfg->enabled = doc["es8311Enabled"].as<bool>();
            if (doc["es8311Volume"].is<int>()) {
                int v = doc["es8311Volume"].as<int>();
                if (v >= 0 && v <= 100) cfg->volume = (uint8_t)v;
            }
            if (doc["es8311Mute"].is<bool>()) cfg->mute = doc["es8311Mute"].as<bool>();
            hal_save_device_config(es->getSlot());
            LOG_I("[DAC] Migrated ES8311: en=%d vol=%d mute=%d",
                  cfg->enabled, cfg->volume, cfg->mute);
        }
    }

    // Delete legacy file
    LittleFS.remove("/dac_config.json");
    LOG_I("[DAC] Migration complete: /dac_config.json → /hal_config.json (legacy deleted)");
#endif
}

// DAC settings are now persisted in /hal_config.json via hal_save_device_config().
// The legacy /dac_config.json is migrated on first boot via dac_load_settings() above.

// ===== Slot-Indexed Driver Accessor =====
DacDriver* dac_get_driver_for_slot(uint8_t slot) {
    if (slot >= AUDIO_OUT_MAX_SINKS) return nullptr;
    return _driverForSlot[slot];
}

bool dac_output_is_ready() {
    return _i2sTxEnabledFor[0] && _driverForSlot[0] && _driverForSlot[0]->isReady();
}

// Mute ramp state accessors (for testing — HC-6 verification)
float dac_get_mute_gain()  { return _muteGain; }
bool  dac_get_prev_mute()  { return _prevDacMute; }

#endif // DAC_ENABLED
