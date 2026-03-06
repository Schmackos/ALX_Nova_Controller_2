#ifdef DAC_ENABLED

#include "dac_hal.h"
#include "dac_registry.h"
#include "dac_eeprom.h"
#include "app_state.h"
#include "debug_serial.h"
#include "config.h"
#include "audio_pipeline.h"
#include "audio_output_sink.h"
#include "hal/hal_dac_adapter.h"
#include "hal/hal_device_manager.h"
#include "hal/hal_pipeline_bridge.h"

#ifndef NATIVE_TEST
#include "i2s_audio.h"
#include "hal/hal_settings.h"
#include "hal/hal_types.h"
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

// ===== Module State =====
static DacDriver* _driver = nullptr;
static bool _i2sTxEnabled = false;
static float _volumeGain = 1.0f;  // Current linear gain from volume setting

// ===== HAL Adapter Instances =====
static HalDacAdapter* _halPrimaryAdapter = nullptr;
static HalDacAdapter* _halSecondaryAdapter = nullptr;

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

// ===== Volume Update (with logging) =====
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
    if (!as.dacEnabled) return;

    uint32_t newUnderruns = as.dacTxUnderruns - _prevTxUnderruns;
    _prevTxUnderruns = as.dacTxUnderruns;

    LOG_I("[DAC] %s ready=%d vol=%d%%%s gain=%.4f wr=%lu ur=%lu(+%lu)",
          as.dacModelName,
          as.dacReady,
          as.dacVolume,
          as.dacMute ? " MUTE" : "",
          _volumeGain,
          (unsigned long)_txWriteCount,
          (unsigned long)as.dacTxUnderruns,
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

// ===== DAC Output Write =====
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
    bool muteNow = as.dacMute || (_volumeGain == 0.0f);
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
    size_t totalExpected = total_samples * sizeof(int32_t);
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
            i2s_audio_write(txBuf, chunk * sizeof(int32_t), &bytes_written, 20);
            _txBytesWritten += bytes_written;
            if (bytes_written < (size_t)(chunk * sizeof(int32_t))) {
                as.dacTxUnderruns++;
            }
            src += chunk;
            remaining -= chunk;
        }
    } else {
        // Unity gain — write buffer directly
        size_t bytes_written = 0;
        i2s_audio_write(buffer, total_samples * sizeof(int32_t), &bytes_written, 20);
        _txBytesWritten += bytes_written;
        if (bytes_written < (size_t)(total_samples * sizeof(int32_t))) {
            as.dacTxUnderruns++;
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
static DacDriver* _secondaryDriver = nullptr;
static bool _secondaryI2sTxEnabled = false;
static float _secondaryVolumeGain = 1.0f;
#endif

// ===== Sink Thunks (static wrappers for pipeline sink dispatch) =====
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
    if (!as.es8311Enabled) {
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

    if (!i2s_audio_enable_es8311_tx(as.audioSampleRate)) {
        LOG_E("[DAC] ES8311 I2S2 TX enable failed");
        _secondaryDriver->deinit();
        delete _secondaryDriver;
        _secondaryDriver = nullptr;
        return;
    }
    _secondaryI2sTxEnabled = true;

    if (!_secondaryDriver->configure(as.audioSampleRate, 32)) {
        LOG_W("[DAC] ES8311 configure failed");
    }

    // Keep ES8311 hardware at 0 dB — software volume provides the perceptual
    // log curve (same as primary DAC) for consistent volume across all outputs.
    _secondaryDriver->setVolume(100);
    _secondaryDriver->setMute(as.es8311Mute);
    _secondaryVolumeGain = dac_volume_to_linear(as.es8311Volume);

    as.es8311Ready = true;
    LOG_I("[DAC] ES8311 secondary output initialized, vol=%d%% (SW gain=%.4f)", as.es8311Volume, _secondaryVolumeGain);

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
            hal_pipeline_on_device_available(slot);
            as.markHalDeviceDirty();
            LOG_I("[HAL] ES8311 registered in slot %d", slot);
        }

        // Register as pipeline output sink (ch 2,3)
        AudioOutputSink secondarySink = AUDIO_OUTPUT_SINK_INIT;
        secondarySink.name = "ES8311";
        secondarySink.firstChannel = 2;
        secondarySink.channelCount = 2;
        secondarySink.write = _secondary_sink_write;
        secondarySink.isReady = _secondary_sink_ready;
        audio_pipeline_register_sink(&secondarySink);
    } else {
        _halSecondaryAdapter->_ready = true;
        _halSecondaryAdapter->_state = HAL_STATE_AVAILABLE;
    }
#endif
}

void dac_secondary_deinit() {
#if CONFIG_IDF_TARGET_ESP32P4
    AppState& as = AppState::getInstance();

    // Same race condition as primary deinit: pause audio task before
    // deleting the secondary I2S channel to avoid Core 1 preemption crash.
#ifndef NATIVE_TEST
    as.audioPaused = true;
    vTaskDelay(pdMS_TO_TICKS(40));
#endif

    if (_halSecondaryAdapter) {
        uint8_t slot = _halSecondaryAdapter->getSlot();
        _halSecondaryAdapter->_ready = false;
        _halSecondaryAdapter->_state = HAL_STATE_UNAVAILABLE;
        hal_pipeline_on_device_removed(slot);
        as.markHalDeviceDirty();
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
    as.es8311Ready = false;

#ifndef NATIVE_TEST
    as.audioPaused = false;
#endif
    LOG_I("[DAC] ES8311 secondary output deinitialized");
#endif
}

bool dac_secondary_is_ready() {
#if CONFIG_IDF_TARGET_ESP32P4
    return _secondaryI2sTxEnabled && _secondaryDriver && _secondaryDriver->isReady() &&
           AppState::getInstance().es8311Enabled;
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
    if (as.es8311Mute || _secondaryVolumeGain == 0.0f) return;

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
            i2s_audio_write_es8311(txBuf, chunk * sizeof(int32_t), &bw, 20);
            bytes_written += bw;
            src += chunk;
            remaining -= chunk;
        }
    } else {
        // Unity gain — write buffer directly
        size_t total_bytes = total_samples * sizeof(int32_t);
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
    d.underruns = AppState::getInstance().dacTxUnderruns;
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

    if (doc["enabled"].is<bool>()) as.dacEnabled = doc["enabled"].as<bool>();
    if (doc["volume"].is<int>()) {
        int v = doc["volume"].as<int>();
        if (v >= 0 && v <= 100) as.dacVolume = (uint8_t)v;
    }
    if (doc["mute"].is<bool>()) as.dacMute = doc["mute"].as<bool>();
    if (doc["deviceId"].is<int>()) as.dacDeviceId = (uint16_t)doc["deviceId"].as<int>();
    if (doc["modelName"].is<const char*>()) {
        strncpy(as.dacModelName, doc["modelName"].as<const char*>(), sizeof(as.dacModelName) - 1);
        as.dacModelName[sizeof(as.dacModelName) - 1] = '\0';
    }
    if (doc["filterMode"].is<int>()) as.dacFilterMode = (uint8_t)doc["filterMode"].as<int>();

    // ES8311 secondary DAC settings (P4 onboard codec)
    if (doc["es8311Enabled"].is<bool>()) as.es8311Enabled = doc["es8311Enabled"].as<bool>();
    if (doc["es8311Volume"].is<int>()) {
        int v = doc["es8311Volume"].as<int>();
        if (v >= 0 && v <= 100) as.es8311Volume = (uint8_t)v;
    }
    if (doc["es8311Mute"].is<bool>()) as.es8311Mute = doc["es8311Mute"].as<bool>();

    _volumeGain = dac_volume_to_linear(as.dacVolume);
    LOG_I("[DAC] Settings loaded: enabled=%d vol=%d mute=%d device=0x%04X (%s) es8311=%d/%d%%/%s",
          as.dacEnabled, as.dacVolume, as.dacMute, as.dacDeviceId, as.dacModelName,
          as.es8311Enabled, as.es8311Volume, as.es8311Mute ? "muted" : "unmuted");
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
    doc["enabled"] = as.dacEnabled;
    doc["volume"] = as.dacVolume;
    doc["mute"] = as.dacMute;
    doc["deviceId"] = as.dacDeviceId;
    doc["modelName"] = as.dacModelName;
    doc["filterMode"] = as.dacFilterMode;

    // ES8311 secondary DAC settings (P4 onboard codec)
    doc["es8311Enabled"] = as.es8311Enabled;
    doc["es8311Volume"] = as.es8311Volume;
    doc["es8311Mute"] = as.es8311Mute;

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
        AppState::getInstance().dacReady = false;
        return false;
    }

    _driver = entry->factory();
    if (!_driver) {
        LOG_E("[DAC] Factory returned null for %s", entry->name);
        AppState::getInstance().dacReady = false;
        return false;
    }

    AppState& as = AppState::getInstance();
    as.dacDeviceId = deviceId;
    strncpy(as.dacModelName, entry->name, sizeof(as.dacModelName) - 1);
    as.dacModelName[sizeof(as.dacModelName) - 1] = '\0';
    as.dacOutputChannels = _driver->getCapabilities().maxChannels;

    LOG_I("[DAC] Driver selected: %s (0x%04X)", entry->name, deviceId);
    return true;
}

DacDriver* dac_get_driver() {
    return _driver;
}

bool dac_output_is_ready() {
    return _i2sTxEnabled && _driver && _driver->isReady() &&
           AppState::getInstance().dacEnabled;
}

// ===== Init / Deinit =====
void dac_output_init() {
    AppState& as = AppState::getInstance();

    // Initialize I2C mutex for thread-safe EEPROM access
    dac_eeprom_init_mutex();

    // Load persisted settings only on first boot.
    // Runtime re-init (via web UI toggle) must NOT reload — the deferred save
    // hasn't flushed yet, so loading would clobber the just-set dacEnabled=true.
    static bool _settingsLoaded = false;
    if (!_settingsLoaded) {
        _settingsLoaded = true;
        dac_load_settings();
    }

    // Update volume gain
    _volumeGain = dac_volume_to_linear(as.dacVolume);
    LOG_I("[DAC] Volume gain: %d%% -> %.4f linear", as.dacVolume, _volumeGain);

#ifndef NATIVE_TEST
    // Scan I2C bus and look for EEPROM — only on first boot.
    // Skip re-scan on runtime re-init: GPIO 54 (DAC_I2C_SCL) is shared with
    // SDIO WiFi reset (CONFIG_ESP_SDIO_GPIO_RESET_SLAVE=54 on P4).
    // Calling Wire1.begin(48,54) at runtime reconfigures GPIO 54, killing WiFi.
    static bool _eepromScanned = false;
    if (!_eepromScanned) {
    _eepromScanned = true;
    {
        AppState::EepromDiag& ed = as.eepromDiag;
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
            if (eepData.deviceId != 0 && eepData.deviceId != as.dacDeviceId) {
                LOG_I("[DAC] EEPROM auto-select: device 0x%04X -> 0x%04X",
                      as.dacDeviceId, eepData.deviceId);
                as.dacDeviceId = eepData.deviceId;
            }
        } else {
            ed.found = false;
        }
        as.markEepromDirty();
    }
    } // !_eepromScanned
#endif

    if (!as.dacEnabled) {
        LOG_I("[DAC] DAC disabled in settings, skipping init");
        return;
    }

#ifndef NATIVE_TEST
    // Delegation guard: handle HAL auto-provisioned PCM5102A placeholder.
    // hal_load_auto_devices() may have registered a probe-only HalPcm5102a in DETECTED state.
    // Remove it so HalDacAdapter can take its place (and register the audio pipeline sink).
    // If already AVAILABLE (native HAL init completed), skip entirely.
    {
        HalDevice* halDev = HalDeviceManager::instance().findByCompatible("ti,pcm5102a");
        if (halDev) {
            if (halDev->_state == HAL_STATE_AVAILABLE) {
                LOG_I("[DAC] PCM5102A already AVAILABLE in HAL — skipping legacy path");
                return;
            }
            // Probe-only placeholder — remove to allow HalDacAdapter registration
            uint8_t oldSlot = halDev->getSlot();
            HalDeviceManager::instance().removeDevice(oldSlot);
            LOG_I("[DAC] Removed PCM5102A placeholder (slot %u) — HalDacAdapter taking over", oldSlot);
        }
    }
#endif

    // Select driver from saved device ID (default: PCM5102A)
    if (as.dacDeviceId == DAC_ID_NONE) {
        as.dacDeviceId = DAC_ID_PCM5102A;
    }

    if (!dac_select_driver(as.dacDeviceId)) {
        LOG_E("[DAC] Failed to select driver for 0x%04X, falling back to PCM5102A",
              as.dacDeviceId);
        if (!dac_select_driver(DAC_ID_PCM5102A)) {
            LOG_E("[DAC] PCM5102A fallback also failed — DAC disabled");
            as.dacEnabled = false;
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
        as.dacReady = false;
        return;
    }

    // Enable I2S TX full-duplex
    if (!dac_enable_i2s_tx(as.audioSampleRate)) {
        LOG_E("[DAC] I2S TX enable failed — DAC unavailable");
        _driver->deinit();
        as.dacReady = false;
        return;
    }

    // Configure driver with current sample rate
    if (!_driver->configure(as.audioSampleRate, 32)) {
        LOG_W("[DAC] Driver configure failed for %lu Hz", (unsigned long)as.audioSampleRate);
        as.dacReady = false;
        return;
    }

    as.dacDetected = true;
    as.dacReady = true;
    as.dacTxUnderruns = 0;
    _prevTxUnderruns = 0;
    _txWriteCount = 0;
    _txBytesWritten = 0;
    _txBytesExpected = 0;
    _lastDacDumpMs = millis();

    const DacCapabilities& caps = _driver->getCapabilities();
    LOG_I("[DAC] Output initialized: %s by %s (0x%04X)",
          as.dacModelName, caps.manufacturer, as.dacDeviceId);
    LOG_I("[DAC]   Rate=%luHz Ch=%d Vol=%d%% (gain=%.4f) Mute=%s",
          (unsigned long)as.audioSampleRate, as.dacOutputChannels,
          as.dacVolume, _volumeGain, as.dacMute ? "yes" : "no");
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
        strncpy(desc.name, as.dacModelName, 32);
        strncpy(desc.manufacturer, caps.manufacturer ? caps.manufacturer : "Unknown", 32);
        desc.type = HAL_DEV_DAC;
        desc.legacyId = as.dacDeviceId;
        desc.channelCount = as.dacOutputChannels;
        desc.i2cAddr = caps.i2cAddress;
        desc.bus.type = HAL_BUS_I2S;
        desc.bus.index = 0;
        if (caps.hasHardwareVolume) desc.capabilities |= HAL_CAP_HW_VOLUME;
        if (caps.hasFilterModes) desc.capabilities |= HAL_CAP_FILTERS;

        _halPrimaryAdapter = new HalDacAdapter(_driver, desc, true);
        int slot = HalDeviceManager::instance().registerDevice(_halPrimaryAdapter, HAL_DISC_MANUAL);
        if (slot >= 0) {
            hal_pipeline_on_device_available(slot);
            as.markHalDeviceDirty();
            LOG_I("[HAL] Primary DAC registered in slot %d", slot);
        }

        // Register as pipeline output sink (ch 0,1)
        AudioOutputSink primarySink = AUDIO_OUTPUT_SINK_INIT;
        primarySink.name = as.dacModelName;
        primarySink.firstChannel = 0;
        primarySink.channelCount = 2;
        primarySink.write = _primary_sink_write;
        primarySink.isReady = _primary_sink_ready;
        audio_pipeline_register_sink(&primarySink);
    } else {
        _halPrimaryAdapter->_ready = true;
        _halPrimaryAdapter->_state = HAL_STATE_AVAILABLE;
    }
}

void dac_output_deinit() {
    AppState& as = AppState::getInstance();

    // Pause the audio task BEFORE touching I2S or deleting the driver.
    // Both loopTask and audio_pipeline_task run on Core 1; the audio task
    // (priority 3) can be blocking inside i2s_channel_write() when loopTask
    // (priority 1) calls this function.  Setting audioPaused=true causes the
    // audio task to yield at the top of its loop after the current DMA write
    // completes (~5 ms at 48 kHz / 256-frame buffer).  40 ms >> 5 ms.
#ifndef NATIVE_TEST
    as.audioPaused = true;
    vTaskDelay(pdMS_TO_TICKS(40));
#endif

    // Remove sinks so pipeline_write_output() won't call _primary_sink_write()
    // after we free the driver and I2S channel below.
    audio_pipeline_clear_sinks();

    if (_halPrimaryAdapter) {
        uint8_t slot = _halPrimaryAdapter->getSlot();
        _halPrimaryAdapter->_ready = false;
        _halPrimaryAdapter->_state = HAL_STATE_UNAVAILABLE;
        hal_pipeline_on_device_removed(slot);
        as.markHalDeviceDirty();
    }

    if (_driver) {
        _driver->deinit();
        delete _driver;
        _driver = nullptr;
    }
    dac_disable_i2s_tx();   // safe — audio task is paused
    as.dacReady = false;

#ifndef NATIVE_TEST
    as.audioPaused = false;
#endif
    LOG_I("[DAC] Output deinitialized");
}

#endif // DAC_ENABLED
