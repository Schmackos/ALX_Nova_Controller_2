#ifdef DAC_ENABLED
#include "hal_custom_device.h"
#include "hal_device_manager.h"
#include "hal_types.h"
#include "hal_discovery.h"     // hal_wifi_sdio_active()
#include "hal_ess_sabre_adc_base.h" // for extern TwoWire Wire2
#include "../audio_pipeline.h"
#include "../sink_write_utils.h"
#include "../i2s_audio.h"

#ifndef NATIVE_TEST
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "../debug_serial.h"
#else
#define LOG_I(t, ...) ((void)0)
#define LOG_W(t, ...) ((void)0)
#define LOG_E(t, ...) ((void)0)
#endif

// ===== Static dispatch table for up to 4 concurrent custom DAC sinks =====
// Custom devices get their own table so they never collide with ESS SABRE devices.

static constexpr int HAL_CUSTOM_MAX_DAC_INSTANCES = 4;
static HalCustomDevice* _custom_dac_slot_dev[AUDIO_OUT_MAX_SINKS] = {};

static void _custom_dac_write(const int32_t* buf, int stereoFrames) {
    if (!buf || stereoFrames <= 0) return;

#ifndef NATIVE_TEST
    int totalSamples = stereoFrames * 2;
    float  fBuf[512];
    int32_t txBuf[512];
    const int32_t* src = buf;
    int remaining = totalSamples;

    // Find the owning device from the static table
    HalCustomDevice* dev = nullptr;
    uint8_t sinkSlot = 0;
    for (uint8_t s = 0; s < AUDIO_OUT_MAX_SINKS; s++) {
        if (_custom_dac_slot_dev[s]) {
            dev = _custom_dac_slot_dev[s];
            sinkSlot = s;
            break;
        }
    }

    while (remaining > 0) {
        int chunk = (remaining > 512) ? 512 : remaining;

        for (int i = 0; i < chunk; i++) {
            fBuf[i] = (float)src[i] / 2147483520.0f;
        }

        float volGain = audio_pipeline_get_sink_volume(sinkSlot);
        sink_apply_volume(fBuf, chunk, volGain);

        if (dev) {
            bool muted = audio_pipeline_is_sink_muted(sinkSlot);
            sink_apply_mute_ramp(fBuf, chunk, &dev->_muteRampState, muted);
        }

        sink_float_to_i2s_int32(fBuf, txBuf, chunk);

        size_t bytesWritten = 0;
        uint8_t txPort = 2;
        if (dev) {
            HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(dev->getSlot());
            if (cfg && cfg->valid && cfg->i2sPort != 255) txPort = cfg->i2sPort;
        }
        i2s_port_write(txPort, txBuf, (size_t)chunk * sizeof(int32_t), &bytesWritten, 20);

        src += chunk;
        remaining -= chunk;
    }
#else
    (void)stereoFrames;
#endif
}

// isReady callback template for custom DAC sinks
#define CUSTOM_DAC_READY_FN(N) \
    static bool _custom_dac_ready_##N(void) { \
        return _custom_dac_slot_dev[N] && _custom_dac_slot_dev[N]->_ready; \
    }

CUSTOM_DAC_READY_FN(0)
CUSTOM_DAC_READY_FN(1)
CUSTOM_DAC_READY_FN(2)
CUSTOM_DAC_READY_FN(3)
CUSTOM_DAC_READY_FN(4)
CUSTOM_DAC_READY_FN(5)
CUSTOM_DAC_READY_FN(6)
CUSTOM_DAC_READY_FN(7)
CUSTOM_DAC_READY_FN(8)
CUSTOM_DAC_READY_FN(9)
CUSTOM_DAC_READY_FN(10)
CUSTOM_DAC_READY_FN(11)
CUSTOM_DAC_READY_FN(12)
CUSTOM_DAC_READY_FN(13)
CUSTOM_DAC_READY_FN(14)
CUSTOM_DAC_READY_FN(15)

static bool (*const _custom_dac_ready_fn[AUDIO_OUT_MAX_SINKS])(void) = {
    _custom_dac_ready_0,  _custom_dac_ready_1,  _custom_dac_ready_2,  _custom_dac_ready_3,
    _custom_dac_ready_4,  _custom_dac_ready_5,  _custom_dac_ready_6,  _custom_dac_ready_7,
    _custom_dac_ready_8,  _custom_dac_ready_9,  _custom_dac_ready_10, _custom_dac_ready_11,
    _custom_dac_ready_12, _custom_dac_ready_13, _custom_dac_ready_14, _custom_dac_ready_15,
};

// ===== Static ADC read stub for custom I2S ADC devices =====
static uint32_t _custom_adc_read(int32_t* dst, uint32_t requestedFrames) {
#ifndef NATIVE_TEST
    // Find owning device: search through HAL manager for custom ADC
    // For now, provide silence — real implementation routes via i2s_port_read()
    // in the pipeline bridge which calls this after HAL init.
    (void)dst; (void)requestedFrames;
#else
    (void)dst; (void)requestedFrames;
#endif
    return 0;
}

static bool _custom_adc_is_active(void) {
    return false;
}

static uint32_t _custom_adc_get_sample_rate(void) {
    return 48000;
}

// ===== HalCustomDevice =====

HalCustomDevice::HalCustomDevice(const char* compatible, const char* name,
                                  uint16_t caps, HalBusType busType, HalDeviceType devType) {
    hal_safe_strcpy(_descriptor.compatible, sizeof(_descriptor.compatible), compatible);
    hal_safe_strcpy(_descriptor.name, sizeof(_descriptor.name), name);
    _descriptor.type         = devType;
    _descriptor.capabilities = caps;
    _descriptor.bus.type     = busType;
    _descriptor.channelCount = 2;
    memset(_initSeq, 0, sizeof(_initSeq));
    _initPriority = HAL_PRIORITY_HARDWARE;

    _inputSource = AUDIO_INPUT_SOURCE_INIT;
    _inputSourceValid = false;
}

// ===== I2C probe helper =====

bool HalCustomDevice::_probeI2c() {
#ifndef NATIVE_TEST
    uint8_t busIndex = _descriptor.bus.index;

    // Guard: Bus 0 (GPIO 48/54) shares SDIO with WiFi — skip if active
    if (busIndex == HAL_I2C_BUS_EXT && hal_wifi_sdio_active()) {
        setLastError("Bus 0 SDIO conflict: WiFi active");
        LOG_W("[HAL:Custom]", "Probe skipped for %s: Bus 0 SDIO conflict",
              _descriptor.compatible);
        return false;
    }

    // Resolve config I2C address and bus
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    uint8_t i2cAddr = _descriptor.i2cAddr;
    if (cfg && cfg->valid && cfg->i2cAddr != 0) {
        i2cAddr  = cfg->i2cAddr;
        busIndex = cfg->i2cBusIndex;
    }

    if (i2cAddr == 0) {
        // No address configured — cannot probe
        setLastError("No I2C address configured");
        return false;
    }

    // Select Wire instance for bus
    TwoWire* wire = &Wire;
    switch (busIndex) {
        case HAL_I2C_BUS_EXT:      wire = &Wire1;  break;
        case HAL_I2C_BUS_ONBOARD:  wire = &Wire;   break;
        case HAL_I2C_BUS_EXP:
        default:                   wire = &Wire2;  break;
    }

    wire->beginTransmission(i2cAddr);
    uint8_t err = wire->endTransmission();
    if (err != 0) {
        char msg[48];
        snprintf(msg, sizeof(msg), "I2C probe NACK at 0x%02X bus %u (err %u)",
                 i2cAddr, busIndex, err);
        setLastError(msg);
        LOG_W("[HAL:Custom]", "%s: %s", _descriptor.compatible, msg);
        return false;
    }

    LOG_I("[HAL:Custom]", "Probe OK: %s at 0x%02X bus %u",
          _descriptor.compatible, i2cAddr, busIndex);
    return true;
#else
    // NATIVE_TEST: report success for I2C devices that have an address in mock
    if (_descriptor.i2cAddr != 0) {
        Wire.beginTransmission(_descriptor.i2cAddr);
        uint8_t err = Wire.endTransmission();
        if (err != 0) {
            setLastError("I2C probe NACK (mock)");
            return false;
        }
    }
    return true;
#endif
}

// ===== Init register sequence =====

bool HalCustomDevice::_runInitSequence() {
    if (_initSeqCount == 0) return true;

#ifndef NATIVE_TEST
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    uint8_t busIndex = _descriptor.bus.index;
    uint8_t i2cAddr  = _descriptor.i2cAddr;
    if (cfg && cfg->valid) {
        if (cfg->i2cAddr    != 0) i2cAddr  = cfg->i2cAddr;
        if (cfg->i2cBusIndex != 0) busIndex = cfg->i2cBusIndex;
    }

    TwoWire* wire = &Wire;
    switch (busIndex) {
        case HAL_I2C_BUS_EXT:      wire = &Wire1; break;
        case HAL_I2C_BUS_ONBOARD:  wire = &Wire;  break;
        case HAL_I2C_BUS_EXP:
        default:                   wire = &Wire2; break;
    }

    // Cap per-write timeout to 10 ms to prevent a non-responsive device from
    // blocking the main loop for up to 1.6 s (32 writes × default 50 ms timeout).
    uint32_t savedTimeout = wire->getTimeout();
    wire->setTimeout(10);

    bool seqOk = true;
    for (int i = 0; i < _initSeqCount; i++) {
        wire->beginTransmission(i2cAddr);
        wire->write(_initSeq[i].reg);
        wire->write(_initSeq[i].val);
        uint8_t err = wire->endTransmission();
        if (err != 0) {
            char msg[48];
            snprintf(msg, sizeof(msg), "Init seq write failed at reg 0x%02X (err %u)",
                     _initSeq[i].reg, err);
            setLastError(msg);
            LOG_E("[HAL:Custom]", "%s: %s", _descriptor.name, msg);
            seqOk = false;
            break;
        }
    }

    wire->setTimeout(savedTimeout);

    if (!seqOk) return false;
    LOG_I("[HAL:Custom]", "%s: init sequence complete (%d regs)", _descriptor.name, _initSeqCount);
    return true;
#else
    // NATIVE_TEST: simulate the writes through Wire mock
    for (int i = 0; i < _initSeqCount; i++) {
        Wire.beginTransmission(_descriptor.i2cAddr);
        Wire.write(_initSeq[i].reg);
        Wire.write(_initSeq[i].val);
        uint8_t err = Wire.endTransmission();
        if (err != 0) {
            char msg[48];
            snprintf(msg, sizeof(msg), "Init seq write failed at reg 0x%02X (err %u)",
                     _initSeq[i].reg, err);
            setLastError(msg);
            return false;
        }
    }
    return true;
#endif
}

// ===== Lifecycle =====

bool HalCustomDevice::probe() {
    if (_descriptor.bus.type == HAL_BUS_I2C) {
        return _probeI2c();
    }
    // I2S-only, GPIO, and other non-I2C devices: no hardware probe needed
    return true;
}

HalInitResult HalCustomDevice::init() {
    // Apply config overrides (I2C address, I2S port) from HalDeviceConfig
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);

    // Execute Tier 2 init register sequence (I2C bus only)
    // I2S-only devices have no register bus to write to
    if (_descriptor.bus.type == HAL_BUS_I2C && !_runInitSequence()) {
        setReady(false);
        _state = HAL_STATE_ERROR;
        return hal_init_fail(DIAG_HAL_INIT_FAILED, _lastError);
    }

    // For DAC-path I2S devices: enable I2S TX output and claim GPIO pins
    if ((_descriptor.capabilities & HAL_CAP_DAC_PATH) &&
        _descriptor.bus.type == HAL_BUS_I2S) {
#ifndef NATIVE_TEST
        uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;
        gpio_num_t dout = GPIO_NUM_NC;
        if (cfg && cfg->valid && cfg->pinData >= 0) dout = (gpio_num_t)cfg->pinData;

        I2sPortConfig i2sCfg = {};
        if (cfg && cfg->valid) {
            i2sCfg.format       = cfg->i2sFormat;
            i2sCfg.bitDepth     = cfg->bitDepth;
            i2sCfg.mclkMultiple = cfg->mclkMultiple;
        }

        if (!i2s_port_enable_tx(port, I2S_MODE_STD, 0, dout,
                                GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC, &i2sCfg)) {
            LOG_W("[HAL:Custom]", "%s: I2S TX enable failed (port %u)", _descriptor.name, port);
            // Non-fatal: device can still be registered without audio output
        }

        // Claim GPIO pins used by this I2S device so the pin conflict tracker
        // prevents another driver from reusing them.  Released in deinit().
        HalDeviceManager& pinMgr = HalDeviceManager::instance();
        if (cfg && cfg->valid) {
            if (cfg->pinData >= 0) pinMgr.claimPin(cfg->pinData, HAL_BUS_I2S, port, _slot);
            if (cfg->pinBck  >= 0) pinMgr.claimPin(cfg->pinBck,  HAL_BUS_I2S, port, _slot);
            if (cfg->pinLrc  >= 0) pinMgr.claimPin(cfg->pinLrc,  HAL_BUS_I2S, port, _slot);
            if (cfg->pinMclk >= 0) pinMgr.claimPin(cfg->pinMclk, HAL_BUS_I2S, port, _slot);
        }
#endif
    }

    // For ADC-path I2S devices: build the input source descriptor
    if ((_descriptor.capabilities & HAL_CAP_ADC_PATH) &&
        _descriptor.bus.type == HAL_BUS_I2S) {
        _inputSource = AUDIO_INPUT_SOURCE_INIT;
        _inputSource.name          = _descriptor.name;
        _inputSource.read          = _custom_adc_read;
        _inputSource.isActive      = _custom_adc_is_active;
        _inputSource.getSampleRate = _custom_adc_get_sample_rate;
        _inputSource.halSlot       = _slot;
        _inputSource.isHardwareAdc = true;
        _inputSourceValid = true;
    }

    _initialized = true;
    setReady(true);
    _state = HAL_STATE_AVAILABLE;
    LOG_I("[HAL:Custom]", "Custom device init: %s", _descriptor.name);
    return hal_init_ok();
}

void HalCustomDevice::deinit() {
    // Disable I2S TX and release claimed GPIO pins
    if ((_descriptor.capabilities & HAL_CAP_DAC_PATH) &&
        _descriptor.bus.type == HAL_BUS_I2S) {
#ifndef NATIVE_TEST
        HalDeviceManager& mgr = HalDeviceManager::instance();
        HalDeviceConfig* cfg = mgr.getConfig(_slot);
        uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;
        i2s_port_disable_tx(port);

        // Release GPIO pins claimed during init()
        if (cfg && cfg->valid) {
            if (cfg->pinData >= 0) mgr.releasePin(cfg->pinData);
            if (cfg->pinBck  >= 0) mgr.releasePin(cfg->pinBck);
            if (cfg->pinLrc  >= 0) mgr.releasePin(cfg->pinLrc);
            if (cfg->pinMclk >= 0) mgr.releasePin(cfg->pinMclk);
        }
#endif
    }

    // Clear static slot entry if we were registered there
    for (int s = 0; s < AUDIO_OUT_MAX_SINKS; s++) {
        if (_custom_dac_slot_dev[s] == this) {
            _custom_dac_slot_dev[s] = nullptr;
        }
    }

    _initialized = false;
    _inputSourceValid = false;
    setReady(false);
    _state = HAL_STATE_REMOVED;
}

void HalCustomDevice::dumpConfig() {
    LOG_I("[HAL:Custom]", "Custom: %s (%s) bus=%u caps=0x%04X",
          _descriptor.name, _descriptor.compatible,
          (unsigned)_descriptor.bus.type, (unsigned)_descriptor.capabilities);
}

bool HalCustomDevice::healthCheck() {
    return _initialized;
}

bool HalCustomDevice::configure(uint32_t sampleRate, uint8_t bitDepth) {
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (cfg) {
        cfg->sampleRate = sampleRate;
        cfg->bitDepth   = bitDepth;
    }
    return true;
}

bool HalCustomDevice::setVolume(uint8_t percent) {
    if (percent > 100) percent = 100;
    _volume = percent;
    return true;
}

bool HalCustomDevice::setMute(bool mute) {
    _muted = mute;
    return true;
}

// ===== Init sequence =====

void HalCustomDevice::setInitSequence(const HalInitRegPair* seq, int count) {
    if (count > HAL_CUSTOM_MAX_INIT_REGS) count = HAL_CUSTOM_MAX_INIT_REGS;
    for (int i = 0; i < count; i++) _initSeq[i] = seq[i];
    _initSeqCount = count;
}

// ===== buildSink (DAC path) =====

bool HalCustomDevice::buildSink(uint8_t sinkSlot, AudioOutputSink* out) {
    if (!out) return false;
    if (sinkSlot >= AUDIO_OUT_MAX_SINKS) return false;
    if (!(_descriptor.capabilities & HAL_CAP_DAC_PATH)) return false;

    *out = AUDIO_OUTPUT_SINK_INIT;
    out->name         = _descriptor.name;
    uint8_t fc = (uint8_t)(sinkSlot * 2);
    if (fc + _descriptor.channelCount > AUDIO_PIPELINE_MATRIX_SIZE) return false;
    out->firstChannel = fc;
    out->channelCount = _descriptor.channelCount;
    out->halSlot      = _slot;
    out->write        = _custom_dac_write;
    out->isReady      = _custom_dac_ready_fn[sinkSlot];
    out->ctx          = this;

    _custom_dac_slot_dev[sinkSlot] = this;
    return true;
}

// ===== getInputSource (ADC path) =====

const AudioInputSource* HalCustomDevice::getInputSource() const {
    if (_inputSourceValid) return &_inputSource;
    return nullptr;
}

// ===== hal_load_custom_devices =====

void hal_load_custom_devices() {
    HalDeviceManager& mgr = HalDeviceManager::instance();

    // Remove previously-loaded custom devices (HAL_DISC_MANUAL) to avoid duplicates
    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
        HalDevice* dev = mgr.getDevice(i);
        if (dev && dev->getDiscovery() == HAL_DISC_MANUAL) {
            dev->deinit();
            mgr.removeDevice(i);
        }
    }

#ifndef NATIVE_TEST
    if (!LittleFS.exists("/hal/custom")) {
        LittleFS.mkdir("/hal/custom");
        return;
    }

    File dir = LittleFS.open("/hal/custom");
    if (!dir || !dir.isDirectory()) return;

    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            String content = f.readString();
            JsonDocument doc;
            if (deserializeJson(doc, content) != DeserializationError::Ok) {
                LOG_W("[HAL:Custom]", "Bad JSON in %s — skipping", f.name());
                f = dir.openNextFile();
                continue;
            }

            const char* compatible = doc["compatible"] | "";
            const char* name = doc["name"] | compatible;

            if (strlen(compatible) == 0) {
                LOG_W("[HAL:Custom]", "Schema missing 'compatible' field — skipping");
                f = dir.openNextFile();
                continue;
            }

            // Skip if already registered as a builtin
            if (mgr.findByCompatible(compatible)) {
                LOG_I("[HAL:Custom]", "Already registered: %s — skipping", compatible);
                f = dir.openNextFile();
                continue;
            }

            // Determine device type from "type" field
            HalDeviceType devType = HAL_DEV_DAC;
            const char* typeStr = doc["type"] | "dac";
            if (strcmp(typeStr, "adc") == 0)   devType = HAL_DEV_ADC;
            else if (strcmp(typeStr, "codec") == 0) devType = HAL_DEV_CODEC;
            else if (strcmp(typeStr, "amp") == 0)   devType = HAL_DEV_AMP;
            // default: HAL_DEV_DAC

            // Determine capabilities from JSON array
            uint16_t caps = 0;
            JsonArray capArr = doc["capabilities"].as<JsonArray>();
            for (const char* cap : capArr) {
                if (strcmp(cap, "volume_control") == 0) caps |= HAL_CAP_HW_VOLUME;
                if (strcmp(cap, "mute") == 0)           caps |= HAL_CAP_MUTE;
                if (strcmp(cap, "adc_path") == 0)       caps |= HAL_CAP_ADC_PATH;
                if (strcmp(cap, "dac_path") == 0)       caps |= HAL_CAP_DAC_PATH;
                if (strcmp(cap, "filters") == 0)        caps |= HAL_CAP_FILTERS;
                if (strcmp(cap, "pga_control") == 0)    caps |= HAL_CAP_PGA_CONTROL;
                if (strcmp(cap, "hpf_control") == 0)    caps |= HAL_CAP_HPF_CONTROL;
            }

            // Determine bus type
            const char* busTypeStr = doc["bus"] | "i2s";
            HalBusType busType = (strcmp(busTypeStr, "i2c") == 0) ? HAL_BUS_I2C : HAL_BUS_I2S;

            HalCustomDevice* dev = new HalCustomDevice(compatible, name, caps, busType, devType);
            if (!dev) {
                LOG_E("[HAL:Custom]", "Out of memory allocating device: %s", compatible);
                f = dir.openNextFile();
                continue;
            }

            // Parse initSequence (Tier 2)
            if (doc["initSequence"].is<JsonArray>()) {
                JsonArray seqArr = doc["initSequence"].as<JsonArray>();
                HalInitRegPair pairs[HAL_CUSTOM_MAX_INIT_REGS];
                int pairCount = 0;
                for (JsonObject entry : seqArr) {
                    if (pairCount >= HAL_CUSTOM_MAX_INIT_REGS) break;
                    pairs[pairCount].reg = entry["reg"] | (uint8_t)0;
                    pairs[pairCount].val = entry["val"] | (uint8_t)0;
                    pairCount++;
                }
                if (pairCount > 0) {
                    dev->setInitSequence(pairs, pairCount);
                }
            }

            int slot = mgr.registerDevice(dev, HAL_DISC_MANUAL);
            if (slot < 0) {
                LOG_E("[HAL:Custom]", "No free slots for device: %s", compatible);
                delete dev;
                f = dir.openNextFile();
                continue;
            }

            // Apply defaults from schema to config
            HalDeviceConfig* cfg = mgr.getConfig((uint8_t)slot);
            if (cfg) {
                cfg->valid        = true;
                cfg->sampleRate   = doc["defaults"]["sample_rate"]    | 48000U;
                cfg->bitDepth     = doc["defaults"]["bits_per_sample"] | (uint8_t)16;
                cfg->mclkMultiple = doc["defaults"]["mclk_multiple"]  | (uint16_t)256;
                cfg->enabled      = true;
                cfg->paControlPin = -1;

                // I2C / I2S address and port overrides from schema
                uint32_t addrRaw = (uint32_t)strtoul(doc["i2cAddr"] | "0x00", nullptr, 0);
                if (addrRaw != 0) cfg->i2cAddr = (uint8_t)addrRaw;
                if (doc["i2cBus"].is<uint8_t>())  cfg->i2cBusIndex = doc["i2cBus"].as<uint8_t>();
                if (doc["i2sPort"].is<uint8_t>()) cfg->i2sPort     = doc["i2sPort"].as<uint8_t>();
                if (doc["channels"].is<uint8_t>()) {
                    // Patch descriptor channel count from schema
                    // (descriptor was set to 2 by default in constructor)
                }
            }

            // Set I2C bus index on descriptor for probe() SDIO guard
            if (busType == HAL_BUS_I2C && cfg) {
                dev->setI2cConfig(cfg->i2cAddr, cfg->i2cBusIndex);
            }

            dev->init();
            LOG_I("[HAL:Custom]", "Loaded custom device: %s (slot %d)", name, slot);
        }
        f = dir.openNextFile();
    }
#else
    LOG_I("[HAL:Custom]", "NATIVE_TEST: skipping LittleFS scan");
#endif // NATIVE_TEST
}

// ===== hal_save_custom_schema =====

bool hal_save_custom_schema(const char* schemaJson, char* outCompatible, size_t compatLen) {
#ifndef NATIVE_TEST
    if (!schemaJson) return false;

    JsonDocument doc;
    if (deserializeJson(doc, schemaJson) != DeserializationError::Ok) return false;

    const char* compatible = doc["compatible"] | "";
    if (strlen(compatible) == 0) return false;

    if (!LittleFS.exists("/hal/custom")) {
        LittleFS.mkdir("/hal/custom");
    }

    // Build safe filename (replace commas with underscores)
    char filename[80];
    snprintf(filename, sizeof(filename), "/hal/custom/%s.json", compatible);
    for (char* p = filename; *p; p++) {
        if (*p == ',') *p = '_';
    }

    File f = LittleFS.open(filename, "w");
    if (!f) return false;
    f.print(schemaJson);
    f.close();

    if (outCompatible && compatLen > 0) {
        hal_safe_strcpy(outCompatible, compatLen, compatible);
    }
    return true;
#else
    (void)schemaJson;
    if (outCompatible && compatLen > 0) outCompatible[0] = '\0';
    return true;
#endif
}

#endif // DAC_ENABLED
