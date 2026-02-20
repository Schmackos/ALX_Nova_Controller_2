#ifdef DAC_ENABLED

#include "dac_hal.h"
#include "dac_registry.h"
#include "dac_eeprom.h"
#include "app_state.h"
#include "debug_serial.h"
#include "config.h"

#ifndef NATIVE_TEST
#include "i2s_audio.h"
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

// ===== I2S TX Full-Duplex =====
// Delegates to i2s_audio.cpp which owns the I2S channel handles.
#ifndef NATIVE_TEST

bool dac_enable_i2s_tx(uint32_t sampleRate) {
    if (_i2sTxEnabled) return true;

    LOG_I("[DAC] Enabling I2S TX full-duplex on I2S_NUM_0, data_out=GPIO%d", I2S_TX_DATA_PIN);

    // i2s_audio_enable_tx pauses the audio task, reinstalls I2S0 in TX+RX mode,
    // and resumes the audio task. dacEnabled && dacReady must both be true for
    // i2s_configure_adc1 (called internally) to allocate the TX channel.
    if (!i2s_audio_enable_tx(sampleRate)) {
        LOG_E("[DAC] I2S TX enable failed");
        return false;
    }

    _i2sTxEnabled = true;
    LOG_I("[DAC] I2S TX full-duplex enabled: rate=%luHz data_out=GPIO%d MCLK=%luHz DMA=%dx%d",
          (unsigned long)sampleRate, I2S_TX_DATA_PIN,
          (unsigned long)(sampleRate * 256),
          I2S_DMA_BUF_COUNT, I2S_DMA_BUF_LEN);
    return true;
}

void dac_disable_i2s_tx() {
    if (!_i2sTxEnabled) return;

    LOG_I("[DAC] Disabling I2S TX, reverting to RX-only");

    // Clear dacReady so i2s_configure_adc1 (called inside disable_tx) creates RX-only
    AppState::getInstance().dacReady = false;
    i2s_audio_disable_tx();

    _i2sTxEnabled = false;
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

    if (as.dacMute || _volumeGain == 0.0f) {
        // Muted — write silence (tx_desc_auto_clear handles this if we skip)
        return;
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

    if (needSoftwareVolume && _volumeGain < 1.0f) {
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
            dac_apply_software_volume(fBuf, chunk, _volumeGain);
            // Convert back to int32
            for (int i = 0; i < chunk; i++) {
                txBuf[i] = (int32_t)(fBuf[i] * 2147483647.0f);
            }
            size_t bytes_written = 0;
            i2s_audio_write_tx(txBuf, chunk * sizeof(int32_t), &bytes_written, 20);
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
        i2s_audio_write_tx(buffer, total_samples * sizeof(int32_t), &bytes_written, 20);
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

    _volumeGain = dac_volume_to_linear(as.dacVolume);
    LOG_I("[DAC] Settings loaded: enabled=%d vol=%d mute=%d device=0x%04X (%s)",
          as.dacEnabled, as.dacVolume, as.dacMute, as.dacDeviceId, as.dacModelName);
#endif
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

    // Load persisted settings
    dac_load_settings();

    // Update volume gain
    _volumeGain = dac_volume_to_linear(as.dacVolume);
    LOG_I("[DAC] Volume gain: %d%% -> %.4f linear", as.dacVolume, _volumeGain);

#ifndef NATIVE_TEST
    // Scan I2C bus and look for EEPROM
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
#endif

    if (!as.dacEnabled) {
        LOG_I("[DAC] DAC disabled in settings, skipping init");
        return;
    }

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

    // Init driver
    DacPinConfig pins = {
        I2S_TX_DATA_PIN,
        DAC_I2C_SDA_PIN,
        DAC_I2C_SCL_PIN,
        0  // Shared MCLK with ADC
    };
    if (!_driver->init(pins)) {
        LOG_E("[DAC] Driver init failed");
        as.dacReady = false;
        return;
    }

    // Enable I2S TX full-duplex.
    // dacReady must be true before calling so i2s_configure_adc1() creates the
    // full-duplex channel. Reset to false on failure.
    as.dacReady = true;
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
}

void dac_output_reinit() {
#ifndef NATIVE_TEST
    AppState& as = AppState::getInstance();
    if (!as.dacEnabled || !_driver) {
        LOG_D("[DAC] Reinit skipped: DAC not enabled");
        as.dacMute = false;
        return;
    }
    LOG_I("[DAC] Reinit: cycling I2S TX for USB reconnect");

    // Tear down TX (sets dacReady=false and _i2sTxEnabled=false internally)
    dac_disable_i2s_tx();

    // dacReady must be true so i2s_configure_adc1() creates the TX+RX channel
    as.dacReady = true;
    if (!dac_enable_i2s_tx(as.audioSampleRate)) {
        LOG_E("[DAC] Reinit: I2S TX re-enable failed");
        as.dacReady = false;
        as.dacMute = false;
        return;
    }

    // Reconfigure driver (DAC chip PLL relock after power-on)
    if (!_driver->configure(as.audioSampleRate, 32)) {
        LOG_W("[DAC] Reinit: driver reconfigure failed");
    }

    as.dacReady = true;

    // Allow DAC chip PLL to stabilize before unmuting
    delay(50);
    as.dacMute = false;
    LOG_I("[DAC] Reinit complete, DAC unmuted");
#endif
}

void dac_output_deinit() {
    if (_driver) {
        _driver->deinit();
        delete _driver;
        _driver = nullptr;
    }
    dac_disable_i2s_tx();
    AppState::getInstance().dacReady = false;
    LOG_I("[DAC] Output deinitialized");
}

#endif // DAC_ENABLED
