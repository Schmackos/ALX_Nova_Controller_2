#pragma once
#ifdef DAC_ENABLED

/**
 * HalI2cBus -- Per-bus I2C singleton with FreeRTOS mutex protection.
 *
 * Three static instances, one per physical I2C bus on ESP32-P4:
 *
 *   Bus 0  HAL_I2C_BUS_EXT      GPIO 48/54   Wire1   External (SDIO conflict risk)
 *   Bus 1  HAL_I2C_BUS_ONBOARD  GPIO 7/8     Wire    Onboard  (ES8311 dedicated)
 *   Bus 2  HAL_I2C_BUS_EXP      GPIO 28/29   Wire2   Expansion mezzanine (always safe)
 *
 * All I2C operations acquire a per-bus recursive mutex before touching the Wire
 * instance, preventing cross-driver bus contention when called from different
 * FreeRTOS tasks.  The mutex is created on the first call to begin().
 *
 * Usage:
 *   HalI2cBus& bus = HalI2cBus::get(HAL_I2C_BUS_EXP);
 *   bus.writeReg(0x48, 0x01, 0xA0);
 *
 * TwoWire Wire2 is defined in hal_i2c_bus.cpp — do not declare it elsewhere.
 */

#include "hal_types.h"
#include <stdint.h>
#include <stddef.h>

// Static assert: exactly 3 buses supported (HAL_I2C_BUS_EXT=0, ONBOARD=1, EXP=2)
static_assert(HAL_I2C_BUS_EXT == 0 && HAL_I2C_BUS_ONBOARD == 1 && HAL_I2C_BUS_EXP == 2,
              "HalI2cBus expects bus indices 0/1/2");

class HalI2cBus {
public:
    // Singleton accessor — clamps busIndex to 0-2
    static HalI2cBus& get(uint8_t busIndex);

    // Initialise the Wire instance and create the mutex (idempotent)
    bool begin(int8_t sda, int8_t scl, uint32_t freqHz = 100000);

    // Release the Wire instance
    void end();

    // ===== Mutex-protected I2C operations =====

    // Write single 8-bit register
    bool writeReg(uint8_t addr, uint8_t reg, uint8_t val);

    // Read single 8-bit register (returns 0xFF on error)
    uint8_t readReg(uint8_t addr, uint8_t reg);

    // Write 16-bit value: LSB at regLsb, MSB at regLsb+1 (MSB write latches both)
    bool writeReg16(uint8_t addr, uint8_t regLsb, uint16_t val);

    // Cirrus Logic paged register: 2-byte address (high, low), 1-byte data
    bool writeRegPaged(uint8_t addr, uint16_t reg, uint8_t val);

    // Cirrus Logic paged register read (returns 0xFF on error)
    uint8_t readRegPaged(uint8_t addr, uint16_t reg);

    // I2C address probe — ACK check only (returns true if device responds)
    bool probe(uint8_t addr);

    // I2C address probe — returns Wire endTransmission error code (0=ACK, 2=NACK, 4/5=timeout)
    uint8_t probeGetError(uint8_t addr);

    // Raw multi-byte write: addr, then len bytes from data[]
    bool writeBytes(uint8_t addr, const uint8_t* data, uint8_t len);

    // Raw multi-byte read into buf[]; returns bytes actually read
    uint8_t readBytes(uint8_t addr, uint8_t* buf, uint8_t len);

    // ===== SDIO guard (Bus 0 only) =====
    // Returns true when Bus 0 must not be accessed (WiFi SDIO sharing GPIO 48/54)
    bool isSdioBlocked() const;

    // Set Wire timeout in milliseconds (per-transaction)
    void setTimeout(uint32_t ms);

    uint8_t getBusIndex() const { return _busIndex; }

private:
    // Private constructor — use get()
    explicit HalI2cBus(uint8_t busIndex) : _busIndex(busIndex) {}

    uint8_t _busIndex;
    bool    _begun = false;

#ifndef NATIVE_TEST
    // FreeRTOS recursive mutex handle — created once in begin()
    void* _mutex = nullptr;   // type-erased SemaphoreHandle_t (avoids pulling in FreeRTOS headers here)

    bool _acquireMutex();
    void _releaseMutex();
#endif
};

#endif // DAC_ENABLED
