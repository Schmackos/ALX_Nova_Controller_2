#ifdef DAC_ENABLED
// HalI2cBus -- per-bus I2C singleton with FreeRTOS recursive mutex.
//
// CRITICAL bus → Wire mapping on ESP32-P4 (matches hal_discovery.cpp + dac_eeprom):
//   Bus 0  HAL_I2C_BUS_EXT      GPIO 48/54   Wire1   (SDIO conflict risk with WiFi)
//   Bus 1  HAL_I2C_BUS_ONBOARD  GPIO 7/8     Wire    (ES8311 onboard codec)
//   Bus 2  HAL_I2C_BUS_EXP      GPIO 28/29   Wire2   (expansion mezzanine, always safe)
//
// Wire2 is defined here and extern'd through hal_i2c_bus.h — remove all other
// TwoWire Wire2(2) definitions from the codebase.

#include "hal_i2c_bus.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../debug_serial.h"
// Forward declare to avoid circular include with hal_discovery.h
bool hal_wifi_sdio_active();

// Wire2 is not predefined in the ESP32-P4 Arduino framework — define it here.
// All other files that previously declared TwoWire Wire2(2) have been migrated
// to use HalI2cBus::get(HAL_I2C_BUS_EXP) instead.
TwoWire Wire2(2);

// Map bus index to the correct Wire instance.
// IMPORTANT: Bus 0 uses Wire1 on P4 (GPIO 48/54), NOT Wire.
static TwoWire* _busToWire(uint8_t busIndex) {
    switch (busIndex) {
        case HAL_I2C_BUS_EXT:     return &Wire1;   // GPIO 48/54
        case HAL_I2C_BUS_ONBOARD: return &Wire;    // GPIO 7/8
        case HAL_I2C_BUS_EXP:     return &Wire2;   // GPIO 28/29
        default:                  return &Wire2;
    }
}

#else // NATIVE_TEST
// ===== Native test stubs — no FreeRTOS, use Wire mock directly =====
#include "../test_mocks/Wire.h"
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)

static WireClass* _busToWire(uint8_t busIndex) {
    switch (busIndex) {
        case HAL_I2C_BUS_EXT:     return &Wire1;
        case HAL_I2C_BUS_ONBOARD: return &Wire;
        case HAL_I2C_BUS_EXP:     return &Wire2;
        default:                  return &Wire2;
    }
}
#endif // NATIVE_TEST

// ===== Static instances =====

HalI2cBus& HalI2cBus::get(uint8_t busIndex) {
    if (busIndex > 2) busIndex = 2;
    // Meyer's singleton: function-local statics are initialized on first call.
    // Defined inside a static member function, which has access to the private constructor.
    static HalI2cBus bus0(HAL_I2C_BUS_EXT);
    static HalI2cBus bus1(HAL_I2C_BUS_ONBOARD);
    static HalI2cBus bus2(HAL_I2C_BUS_EXP);
    static HalI2cBus* const instances[3] = { &bus0, &bus1, &bus2 };
    return *instances[busIndex];
}

// ===== Mutex helpers (hardware builds only) =====

#ifndef NATIVE_TEST

bool HalI2cBus::_acquireMutex() {
    if (!_mutex) return true;  // Not yet created (should not happen after begin())
    return xSemaphoreTakeRecursive((SemaphoreHandle_t)_mutex, pdMS_TO_TICKS(50)) == pdTRUE;
}

void HalI2cBus::_releaseMutex() {
    if (_mutex) xSemaphoreGiveRecursive((SemaphoreHandle_t)_mutex);
}

#endif // NATIVE_TEST

// ===== begin() / end() =====

bool HalI2cBus::begin(int8_t sda, int8_t scl, uint32_t freqHz) {
#ifndef NATIVE_TEST
    // Create mutex on first begin()
    if (!_mutex) {
        _mutex = (void*)xSemaphoreCreateRecursiveMutex();
        if (!_mutex) {
            LOG_E("[I2C:%u] Mutex creation failed", _busIndex);
            return false;
        }
    }

    TwoWire* wire = _busToWire(_busIndex);
    if (!wire->begin((int)sda, (int)scl, freqHz)) {
        LOG_W("[I2C:%u] Wire.begin(%d,%d) failed", _busIndex, (int)sda, (int)scl);
        return false;
    }
    _begun = true;
    LOG_I("[I2C:%u] Initialized (SDA=%d SCL=%d freq=%lu Hz)",
          _busIndex, (int)sda, (int)scl, (unsigned long)freqHz);
    return true;
#else
    // Native test: record bus init state through mock
    _busToWire(_busIndex)->begin((int)sda, (int)scl, freqHz);
    _begun = true;
    return true;
#endif
}

void HalI2cBus::end() {
#ifndef NATIVE_TEST
    if (_begun) {
        _busToWire(_busIndex)->end();
        _begun = false;
    }
#else
    _begun = false;
#endif
}

// ===== setTimeout =====

void HalI2cBus::setTimeout(uint32_t ms) {
#ifndef NATIVE_TEST
    _busToWire(_busIndex)->setTimeout(ms);
#else
    (void)ms;
#endif
}

// ===== SDIO guard =====

bool HalI2cBus::isSdioBlocked() const {
    if (_busIndex != HAL_I2C_BUS_EXT) return false;
#ifndef NATIVE_TEST
    return hal_wifi_sdio_active();
#else
    return false;
#endif
}

// ===== I2C operations =====

bool HalI2cBus::writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
#ifndef NATIVE_TEST
    if (!_acquireMutex()) {
        LOG_W("[I2C:%u] Mutex timeout on writeReg(0x%02X, 0x%02X)", _busIndex, addr, reg);
        return false;
    }
    TwoWire* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    wire->write(reg);
    wire->write(val);
    uint8_t err = wire->endTransmission();
    _releaseMutex();
    if (err != 0) {
        LOG_E("[I2C:%u] writeReg failed: addr=0x%02X reg=0x%02X val=0x%02X err=%d",
              _busIndex, addr, reg, val, err);
        return false;
    }
    return true;
#else
    auto* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    wire->write(reg);
    wire->write(val);
    return wire->endTransmission() == 0;
#endif
}

uint8_t HalI2cBus::readReg(uint8_t addr, uint8_t reg) {
#ifndef NATIVE_TEST
    if (!_acquireMutex()) {
        LOG_W("[I2C:%u] Mutex timeout on readReg(0x%02X, 0x%02X)", _busIndex, addr, reg);
        return 0xFF;
    }
    TwoWire* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    wire->write(reg);
    wire->endTransmission(false);
    wire->requestFrom(addr, (uint8_t)1);
    uint8_t val = 0xFF;
    if (wire->available()) val = wire->read();
    else LOG_E("[I2C:%u] readReg failed: addr=0x%02X reg=0x%02X", _busIndex, addr, reg);
    _releaseMutex();
    return val;
#else
    auto* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    wire->write(reg);
    wire->endTransmission(false);
    wire->requestFrom(addr, (uint8_t)1);
    if (wire->available()) return wire->read();
    return 0xFF;
#endif
}

bool HalI2cBus::writeReg16(uint8_t addr, uint8_t regLsb, uint16_t val) {
    bool ok = writeReg(addr, regLsb,     (uint8_t)(val & 0xFF));
    ok      = ok && writeReg(addr, regLsb + 1, (uint8_t)((val >> 8) & 0xFF));
    return ok;
}

bool HalI2cBus::writeRegPaged(uint8_t addr, uint16_t reg, uint8_t val) {
#ifndef NATIVE_TEST
    if (!_acquireMutex()) {
        LOG_W("[I2C:%u] Mutex timeout on writeRegPaged(0x%02X, 0x%04X)", _busIndex, addr, reg);
        return false;
    }
    TwoWire* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    wire->write((uint8_t)((reg >> 8) & 0xFF));  // Address high byte
    wire->write((uint8_t)(reg & 0xFF));          // Address low byte
    wire->write(val);
    uint8_t err = wire->endTransmission();
    _releaseMutex();
    if (err != 0) {
        LOG_E("[I2C:%u] writeRegPaged failed: addr=0x%02X reg=0x%04X val=0x%02X err=%d",
              _busIndex, addr, reg, val, err);
        return false;
    }
    return true;
#else
    auto* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    wire->write((uint8_t)((reg >> 8) & 0xFF));
    wire->write((uint8_t)(reg & 0xFF));
    wire->write(val);
    return wire->endTransmission() == 0;
#endif
}

uint8_t HalI2cBus::readRegPaged(uint8_t addr, uint16_t reg) {
#ifndef NATIVE_TEST
    if (!_acquireMutex()) {
        LOG_W("[I2C:%u] Mutex timeout on readRegPaged(0x%02X, 0x%04X)", _busIndex, addr, reg);
        return 0xFF;
    }
    TwoWire* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    wire->write((uint8_t)((reg >> 8) & 0xFF));
    wire->write((uint8_t)(reg & 0xFF));
    wire->endTransmission(false);
    wire->requestFrom(addr, (uint8_t)1);
    uint8_t val = 0xFF;
    if (wire->available()) val = wire->read();
    else LOG_E("[I2C:%u] readRegPaged failed: addr=0x%02X reg=0x%04X", _busIndex, addr, reg);
    _releaseMutex();
    return val;
#else
    auto* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    wire->write((uint8_t)((reg >> 8) & 0xFF));
    wire->write((uint8_t)(reg & 0xFF));
    wire->endTransmission(false);
    wire->requestFrom(addr, (uint8_t)1);
    if (wire->available()) return wire->read();
    return 0xFF;
#endif
}

bool HalI2cBus::probe(uint8_t addr) {
#ifndef NATIVE_TEST
    if (!_acquireMutex()) return false;
    TwoWire* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    uint8_t err = wire->endTransmission();
    _releaseMutex();
    return err == 0;
#else
    auto* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    return wire->endTransmission() == 0;
#endif
}

uint8_t HalI2cBus::probeGetError(uint8_t addr) {
#ifndef NATIVE_TEST
    if (!_acquireMutex()) return 4;  // return timeout on mutex failure
    TwoWire* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    uint8_t err = wire->endTransmission();
    _releaseMutex();
    return err;
#else
    auto* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    return wire->endTransmission();
#endif
}

bool HalI2cBus::writeBytes(uint8_t addr, const uint8_t* data, uint8_t len) {
    if (!data || len == 0) return false;
#ifndef NATIVE_TEST
    if (!_acquireMutex()) {
        LOG_W("[I2C:%u] Mutex timeout on writeBytes(0x%02X)", _busIndex, addr);
        return false;
    }
    TwoWire* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    wire->write(data, len);
    uint8_t err = wire->endTransmission();
    _releaseMutex();
    return err == 0;
#else
    auto* wire = _busToWire(_busIndex);
    wire->beginTransmission(addr);
    wire->write(data, len);
    return wire->endTransmission() == 0;
#endif
}

uint8_t HalI2cBus::readBytes(uint8_t addr, uint8_t* buf, uint8_t len) {
    if (!buf || len == 0) return 0;
#ifndef NATIVE_TEST
    if (!_acquireMutex()) {
        LOG_W("[I2C:%u] Mutex timeout on readBytes(0x%02X)", _busIndex, addr);
        return 0;
    }
    TwoWire* wire = _busToWire(_busIndex);
    uint8_t received = (uint8_t)wire->requestFrom(addr, len);
    for (uint8_t i = 0; i < received; i++) {
        buf[i] = wire->available() ? (uint8_t)wire->read() : 0;
    }
    _releaseMutex();
    return received;
#else
    auto* wire = _busToWire(_busIndex);
    uint8_t received = (uint8_t)wire->requestFrom(addr, len);
    for (uint8_t i = 0; i < received; i++) {
        buf[i] = wire->available() ? (uint8_t)wire->read() : 0;
    }
    return received;
#endif
}

#endif // DAC_ENABLED
