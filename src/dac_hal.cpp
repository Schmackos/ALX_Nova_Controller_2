#ifdef DAC_ENABLED

#include "dac_hal.h"
#include "dac_eeprom.h"
#include "app_state.h"
#include "globals.h"
#include "debug_serial.h"
#include "config.h"
#include "audio_pipeline.h"
#include "audio_output_sink.h"
#include "hal/hal_device_manager.h"
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

// Forward declaration -- defined later in this file, called from dac_boot_prepare()
static void dac_load_settings();

// I2S TX enable state per port index (0 = primary RX+TX, 2 = ES8311 I2S2)
// Port 1 is ADC2 RX-only -- never TX. Index 0/2 map to the two TX-capable ports.
static bool _i2sTxEnabledFor[3] = {};

// Mute ramp: prevents abrupt silence->audio or audio->silence transitions.
// Steps by 0.5f per buffer call (256 frames @ 48kHz = 5.33ms each -> ~10ms full ramp).
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
// Gives: 0->0.0, 50->0.056, 75->0.225, 100->1.0
float dac_volume_to_linear(uint8_t percent) {
    if (percent == 0) return 0.0f;
    if (percent >= 100) return 1.0f;
    // 10^(p/50): p=0->1, p=50->10, p=100->100
    // Normalize: (10^(p/50) - 1) / 99 -> 0.0 to 1.0
    float exponent = (float)percent / 50.0f;
    float power = 1.0f;
    // Manual pow10: 10^x = e^(x * ln10)
    float x = exponent * 2.302585f; // ln(10) = 2.302585
    // Taylor approximation of e^x (good enough for 0-4.6 range)
    // Use iterative: e^x = 1 + x + x^2/2 + x^3/6 + ...
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
    if (gain == 1.0f) return;  // Unity gain -- skip
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
    unsigned long now = millis();
    if (now - _lastDacDumpMs < 5000) return;
    _lastDacDumpMs = now;

    int sinkCount = audio_pipeline_get_sink_count();
    if (sinkCount == 0) return;

    AppState& as = AppState::getInstance();
    uint32_t newUnderruns = as.dac.txUnderruns - _prevTxUnderruns;
    _prevTxUnderruns = as.dac.txUnderruns;

    for (int slot = 0; slot < sinkCount; slot++) {
        const AudioOutputSink* sink = audio_pipeline_get_sink(slot);
        if (!sink || !sink->write) continue;
        const char* name = (sink->name && sink->name[0]) ? sink->name : "DAC";
        LOG_I("[DAC] sink%d (%s) gain=%.4f%s wr=%lu ur=%lu(+%lu)",
              slot, name,
              audio_pipeline_get_sink_volume(slot),
              audio_pipeline_is_sink_muted(slot) ? " MUTE" : "",
              (unsigned long)(slot == 0 ? _txWriteCount : 0),
              (unsigned long)(slot == 0 ? as.dac.txUnderruns : 0),
              (unsigned long)(slot == 0 ? newUnderruns : 0));
        if (slot == 0 && _txWriteCount > 0) {
            LOG_D("[DAC] TX: %lu writes, %luKB written / %luKB expected",
                  (unsigned long)_txWriteCount,
                  (unsigned long)(_txBytesWritten / 1024),
                  (unsigned long)(_txBytesExpected / 1024));
            LOG_I("[DAC] TX peak=0x%08lX (%ld) zeroFrames=%lu/%lu",
                  (unsigned long)_txPeakSample, (long)_txPeakSample,
                  (unsigned long)_txZeroFrames, (unsigned long)_txWriteCount);
        }
    }

    // Reset per-interval counters (slot 0 / primary I2S TX)
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

// ===== I2S TX for a Specific Port =====
// port 0 -> i2s_audio_enable_tx (primary RX+TX full-duplex)
// port 2 -> i2s_audio_enable_es8311_tx (I2S2 secondary TX)
#ifndef NATIVE_TEST
bool dac_enable_i2s_tx_for_port(uint8_t port, uint32_t sampleRate) {
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

void dac_disable_i2s_tx_for_port(uint8_t port) {
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
bool dac_enable_i2s_tx_for_port(uint8_t port, uint32_t sampleRate) {
    (void)sampleRate;
    if (port < 3) { _i2sTxEnabledFor[port] = true; return true; }
    return false;
}
void dac_disable_i2s_tx_for_port(uint8_t port) {
    if (port < 3) _i2sTxEnabledFor[port] = false;
}
#endif

bool dac_is_tx_enabled_for_port(uint8_t port) {
    if (port >= 3) return false;
    return _i2sTxEnabledFor[port];
}

// ===== dac_boot_prepare() =====
// Extracted from dac_output_init(): one-shot boot-time preparation.
// Initialises the EEPROM mutex, loads persisted settings (once),
// computes volume gain, and scans the I2C bus / EEPROM for device ID.
// All operations are guarded by static-once flags -- safe to call multiple times.
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
    // Scan I2C bus and look for EEPROM -- only on first boot.
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
// One-time migration: /dac_config.json -> /hal_config.json
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
            LOG_I("[DAC] Migrated primary DAC settings: en=%d vol=%d mute=%d",
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
            LOG_I("[DAC] Migrated secondary DAC settings: en=%d vol=%d mute=%d",
                  cfg->enabled, cfg->volume, cfg->mute);
        }
    }

    // Delete legacy file
    LittleFS.remove("/dac_config.json");
    LOG_I("[DAC] Migration complete: /dac_config.json -> /hal_config.json (legacy deleted)");
#endif
}

// DAC settings are now persisted in /hal_config.json via hal_save_device_config().
// The legacy /dac_config.json is migrated on first boot via dac_load_settings() above.

// Mute ramp state accessors (for testing -- HC-6 verification)
float dac_get_mute_gain()  { return _muteGain; }
bool  dac_get_prev_mute()  { return _prevDacMute; }

#endif // DAC_ENABLED
