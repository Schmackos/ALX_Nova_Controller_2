#ifdef DAC_ENABLED

#include "dac_eeprom.h"
#include "debug_serial.h"
#include <string.h>

#ifndef NATIVE_TEST
#include <Wire.h>
#endif

// ===== EEPROM Format Layout =====
// 0x00  4B   Magic "ALXD"
// 0x04  1B   Format version (1)
// 0x05  2B   Device ID (uint16_t LE)
// 0x07  1B   Hardware revision
// 0x08  32B  Device name (null-terminated)
// 0x28  32B  Manufacturer (null-terminated)
// 0x48  1B   Max channels
// 0x49  1B   DAC I2C address (0=none)
// 0x4A  1B   Flags: bit0=independent clock, bit1=HW volume, bit2=filters
// 0x4B  1B   Number of sample rates
// 0x4C  16B  Supported rates (up to 4 × uint32_t LE)
// 0x5C  164B Reserved / driver-specific

bool dac_eeprom_parse(const uint8_t* rawData, int len, DacEepromData* out) {
    if (!rawData || !out || len < 0x5C) {
        if (out) out->valid = false;
        return false;
    }

    memset(out, 0, sizeof(DacEepromData));

    // Check magic
    if (memcmp(rawData, DAC_EEPROM_MAGIC, DAC_EEPROM_MAGIC_LEN) != 0) {
        out->valid = false;
        return false;
    }

    // Check version
    out->formatVersion = rawData[0x04];
    if (out->formatVersion != DAC_EEPROM_VERSION) {
        out->valid = false;
        return false;
    }

    // Device ID (little-endian uint16)
    out->deviceId = (uint16_t)rawData[0x05] | ((uint16_t)rawData[0x06] << 8);

    // Hardware revision
    out->hwRevision = rawData[0x07];

    // Device name (32 bytes at 0x08, null-terminated)
    memcpy(out->deviceName, &rawData[0x08], 32);
    out->deviceName[32] = '\0';

    // Manufacturer (32 bytes at 0x28, null-terminated)
    memcpy(out->manufacturer, &rawData[0x28], 32);
    out->manufacturer[32] = '\0';

    // Max channels
    out->maxChannels = rawData[0x48];

    // DAC I2C address
    out->dacI2cAddress = rawData[0x49];

    // Flags
    out->flags = rawData[0x4A];

    // Sample rates
    out->numSampleRates = rawData[0x4B];
    if (out->numSampleRates > DAC_EEPROM_MAX_RATES) {
        out->numSampleRates = DAC_EEPROM_MAX_RATES;
    }
    for (uint8_t i = 0; i < out->numSampleRates; i++) {
        int offset = 0x4C + i * 4;
        out->sampleRates[i] = (uint32_t)rawData[offset]
                            | ((uint32_t)rawData[offset + 1] << 8)
                            | ((uint32_t)rawData[offset + 2] << 16)
                            | ((uint32_t)rawData[offset + 3] << 24);
    }

    out->valid = true;
    return true;
}

// ===== Serialize DacEepromData to raw EEPROM bytes =====
int dac_eeprom_serialize(const DacEepromData* data, uint8_t* outBuf, int bufLen) {
    if (!data || !outBuf || bufLen < DAC_EEPROM_DATA_SIZE) return 0;

    memset(outBuf, 0, DAC_EEPROM_DATA_SIZE);

    // Magic
    memcpy(&outBuf[0x00], DAC_EEPROM_MAGIC, DAC_EEPROM_MAGIC_LEN);

    // Format version
    outBuf[0x04] = DAC_EEPROM_VERSION;

    // Device ID (little-endian uint16)
    outBuf[0x05] = (uint8_t)(data->deviceId & 0xFF);
    outBuf[0x06] = (uint8_t)((data->deviceId >> 8) & 0xFF);

    // Hardware revision
    outBuf[0x07] = data->hwRevision;

    // Device name (32 bytes, zero-padded)
    size_t nameLen = strlen(data->deviceName);
    if (nameLen > 32) nameLen = 32;
    memcpy(&outBuf[0x08], data->deviceName, nameLen);

    // Manufacturer (32 bytes, zero-padded)
    size_t mfrLen = strlen(data->manufacturer);
    if (mfrLen > 32) mfrLen = 32;
    memcpy(&outBuf[0x28], data->manufacturer, mfrLen);

    // Max channels
    outBuf[0x48] = data->maxChannels;

    // DAC I2C address
    outBuf[0x49] = data->dacI2cAddress;

    // Flags
    outBuf[0x4A] = data->flags;

    // Sample rates
    uint8_t numRates = data->numSampleRates;
    if (numRates > DAC_EEPROM_MAX_RATES) numRates = DAC_EEPROM_MAX_RATES;
    outBuf[0x4B] = numRates;

    for (uint8_t i = 0; i < numRates; i++) {
        int offset = 0x4C + i * 4;
        outBuf[offset]     = (uint8_t)(data->sampleRates[i] & 0xFF);
        outBuf[offset + 1] = (uint8_t)((data->sampleRates[i] >> 8) & 0xFF);
        outBuf[offset + 2] = (uint8_t)((data->sampleRates[i] >> 16) & 0xFF);
        outBuf[offset + 3] = (uint8_t)((data->sampleRates[i] >> 24) & 0xFF);
    }

    return DAC_EEPROM_DATA_SIZE;
}

#ifndef NATIVE_TEST

// Read a block of bytes from AT24C02 EEPROM
static bool eeprom_read_block(uint8_t i2cAddr, uint8_t memAddr, uint8_t* buf, int len) {
    Wire.beginTransmission(i2cAddr);
    Wire.write(memAddr);
    if (Wire.endTransmission(false) != 0) return false;

    int received = Wire.requestFrom(i2cAddr, (uint8_t)len);
    if (received != len) return false;

    for (int i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
    return true;
}

// ===== I2C Bus Recovery =====
// If a previous I2C transaction was interrupted (power glitch, reset mid-transfer),
// the slave device (EEPROM) can hold SDA low indefinitely. Recovery: toggle SCL up
// to 9 times while SDA is low, then issue a STOP condition to release the bus.
static void i2c_bus_recovery() {
    LOG_D("[DAC] I2C bus recovery: toggling SCL on GPIO %d/%d", DAC_I2C_SDA_PIN, DAC_I2C_SCL_PIN);

    // Temporarily use GPIO mode (not I2C peripheral)
    pinMode(DAC_I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(DAC_I2C_SCL_PIN, OUTPUT);
    digitalWrite(DAC_I2C_SCL_PIN, HIGH);
    delayMicroseconds(5);

    // Toggle SCL up to 9 times to clock out any stuck byte
    for (int i = 0; i < 9; i++) {
        if (digitalRead(DAC_I2C_SDA_PIN) == HIGH) {
            break; // SDA released, bus is free
        }
        digitalWrite(DAC_I2C_SCL_PIN, LOW);
        delayMicroseconds(5);
        digitalWrite(DAC_I2C_SCL_PIN, HIGH);
        delayMicroseconds(5);
    }

    // Generate STOP condition: SDA low→high while SCL is high
    pinMode(DAC_I2C_SDA_PIN, OUTPUT);
    digitalWrite(DAC_I2C_SDA_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(DAC_I2C_SCL_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(DAC_I2C_SDA_PIN, HIGH);
    delayMicroseconds(5);

    // Release pins back to input (Wire.begin will reconfigure them)
    pinMode(DAC_I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(DAC_I2C_SCL_PIN, INPUT_PULLUP);
}

// ===== I2C Init Helper =====
static void i2c_init() {
    // Check pin state before anything (verify pull-ups are working)
    pinMode(DAC_I2C_SDA_PIN, INPUT);
    pinMode(DAC_I2C_SCL_PIN, INPUT);
    int sdaPre = digitalRead(DAC_I2C_SDA_PIN);
    int sclPre = digitalRead(DAC_I2C_SCL_PIN);

    // Bus recovery if lines are stuck low
    if (sdaPre == LOW || sclPre == LOW) {
        LOG_W("[DAC] I2C bus stuck (SDA=%s SCL=%s) — recovering",
              sdaPre ? "HIGH" : "LOW", sclPre ? "HIGH" : "LOW");
        i2c_bus_recovery();
    }

    // Ensure clean Wire state
    Wire.end();
    delay(1);

    bool ok = Wire.begin(DAC_I2C_SDA_PIN, DAC_I2C_SCL_PIN);
    if (!ok) {
        LOG_W("[DAC] Wire.begin failed, retrying");
        Wire.end();
        delay(10);
        ok = Wire.begin(DAC_I2C_SDA_PIN, DAC_I2C_SCL_PIN);
    }
    if (!ok) {
        LOG_E("[DAC] Wire.begin(SDA=%d, SCL=%d) failed", DAC_I2C_SDA_PIN, DAC_I2C_SCL_PIN);
        return;
    }

    Wire.setClock(100000);  // 100kHz standard mode
    Wire.setTimeOut(100);   // 100ms timeout
    delay(2);               // Bus stabilization
}

bool dac_eeprom_scan(DacEepromData* out) {
    if (!out) return false;
    memset(out, 0, sizeof(DacEepromData));

    i2c_init();

    for (uint8_t addr = DAC_EEPROM_ADDR_START; addr <= DAC_EEPROM_ADDR_END; addr++) {
        // Try to read the magic bytes first
        uint8_t magic[DAC_EEPROM_MAGIC_LEN];
        if (!eeprom_read_block(addr, 0x00, magic, DAC_EEPROM_MAGIC_LEN)) {
            continue;
        }

        if (memcmp(magic, DAC_EEPROM_MAGIC, DAC_EEPROM_MAGIC_LEN) != 0) {
            LOG_D("[DAC] EEPROM 0x%02X: no ALXD magic (%02X %02X %02X %02X)",
                  addr, magic[0], magic[1], magic[2], magic[3]);
            continue;
        }

        LOG_I("[DAC] EEPROM with ALXD magic found at 0x%02X", addr);

        // Read the full header (0x5C = 92 bytes)
        uint8_t rawData[0x5C];
        memcpy(rawData, magic, DAC_EEPROM_MAGIC_LEN);

        // Read remaining bytes in chunks (AT24C02 page size = 8)
        int remaining = 0x5C - DAC_EEPROM_MAGIC_LEN;
        int offset = DAC_EEPROM_MAGIC_LEN;
        while (remaining > 0) {
            int chunk = (remaining > 16) ? 16 : remaining;
            if (!eeprom_read_block(addr, (uint8_t)offset, &rawData[offset], chunk)) {
                LOG_W("[DAC] EEPROM read failed at offset 0x%02X", offset);
                break;
            }
            offset += chunk;
            remaining -= chunk;
        }

        if (remaining > 0) continue; // Read incomplete

        if (dac_eeprom_parse(rawData, 0x5C, out)) {
            out->i2cAddress = addr;
            LOG_I("[DAC] EEPROM parsed: %s by %s (ID=0x%04X, rev=%d)",
                  out->deviceName, out->manufacturer, out->deviceId, out->hwRevision);
            return true;
        }
    }

    LOG_I("[DAC] No EEPROM found on I2C bus");
    return false;
}

// ===== Public Read Raw (for hex dump) =====
bool dac_eeprom_read_raw(uint8_t i2cAddr, uint8_t memAddr, uint8_t* buf, int len) {
    if (!buf || len <= 0) return false;
    // Read in 16-byte chunks (AT24C02 can do sequential reads up to page boundary)
    int remaining = len;
    int offset = 0;
    while (remaining > 0) {
        int chunk = (remaining > 16) ? 16 : remaining;
        if (!eeprom_read_block(i2cAddr, (uint8_t)(memAddr + offset), &buf[offset], chunk)) {
            LOG_W("[DAC] EEPROM raw read failed at 0x%02X+0x%02X", i2cAddr, memAddr + offset);
            return false;
        }
        offset += chunk;
        remaining -= chunk;
    }
    return true;
}

// ===== ACK Polling =====
// After a page write, the AT24C02 goes busy for up to 10ms (typ 5ms).
// ACK polling: repeatedly address the device until it ACKs (write cycle complete).
static bool eeprom_wait_ready(uint8_t i2cAddr, int timeoutMs) {
    unsigned long start = millis();
    while ((millis() - start) < (unsigned long)timeoutMs) {
        Wire.beginTransmission(i2cAddr);
        if (Wire.endTransmission() == 0) return true;
        delay(1);
    }
    return false;
}

// ===== Page-Aware Write with Verification =====
bool dac_eeprom_write(uint8_t i2cAddr, const uint8_t* data, int len) {
    if (!data || len <= 0 || len > DAC_EEPROM_TOTAL_SIZE) return false;

    LOG_I("[DAC] EEPROM write: addr=0x%02X len=%d", i2cAddr, len);

    int offset = 0;
    while (offset < len) {
        // Calculate bytes remaining in current page
        int pageOffset = offset % DAC_EEPROM_PAGE_SIZE;
        int pageRemaining = DAC_EEPROM_PAGE_SIZE - pageOffset;
        int chunk = len - offset;
        if (chunk > pageRemaining) chunk = pageRemaining;

        Wire.beginTransmission(i2cAddr);
        Wire.write((uint8_t)offset);  // Memory address
        for (int i = 0; i < chunk; i++) {
            Wire.write(data[offset + i]);
        }
        if (Wire.endTransmission() != 0) {
            LOG_E("[DAC] EEPROM write failed at offset 0x%02X", offset);
            return false;
        }

        // ACK poll: wait for write cycle to complete (max 20ms, typ 5ms)
        if (!eeprom_wait_ready(i2cAddr, 20)) {
            LOG_E("[DAC] EEPROM not ready after write at offset 0x%02X", offset);
            return false;
        }

        LOG_D("[DAC] EEPROM wrote %d bytes at offset 0x%02X", chunk, offset);
        offset += chunk;
    }

    // Extra settling time before verify
    delay(10);

    // Verify by read-back
    uint8_t verifyBuf[DAC_EEPROM_TOTAL_SIZE];
    int verifyLen = (len > (int)sizeof(verifyBuf)) ? (int)sizeof(verifyBuf) : len;
    if (!dac_eeprom_read_raw(i2cAddr, 0, verifyBuf, verifyLen)) {
        LOG_E("[DAC] EEPROM verify read-back failed");
        return false;
    }
    if (memcmp(data, verifyBuf, verifyLen) != 0) {
        // Log first mismatch for debugging
        for (int i = 0; i < verifyLen; i++) {
            if (data[i] != verifyBuf[i]) {
                LOG_E("[DAC] EEPROM verify mismatch at byte %d: wrote 0x%02X read 0x%02X", i, data[i], verifyBuf[i]);
                break;
            }
        }
        return false;
    }

    LOG_I("[DAC] EEPROM write+verify OK (%d bytes)", len);
    return true;
}

// ===== Erase EEPROM (fill with 0xFF) =====
bool dac_eeprom_erase(uint8_t i2cAddr) {
    LOG_I("[DAC] EEPROM erase: addr=0x%02X (%d pages)", i2cAddr,
          DAC_EEPROM_TOTAL_SIZE / DAC_EEPROM_PAGE_SIZE);

    uint8_t ffPage[DAC_EEPROM_PAGE_SIZE];
    memset(ffPage, 0xFF, DAC_EEPROM_PAGE_SIZE);

    for (int page = 0; page < DAC_EEPROM_TOTAL_SIZE / DAC_EEPROM_PAGE_SIZE; page++) {
        uint8_t addr = (uint8_t)(page * DAC_EEPROM_PAGE_SIZE);
        Wire.beginTransmission(i2cAddr);
        Wire.write(addr);
        for (int i = 0; i < DAC_EEPROM_PAGE_SIZE; i++) {
            Wire.write(0xFF);
        }
        if (Wire.endTransmission() != 0) {
            LOG_E("[DAC] EEPROM erase failed at page %d (addr 0x%02X)", page, addr);
            return false;
        }
        delay(5);  // Write cycle time
    }

    // Verify first 8 bytes are 0xFF
    uint8_t check[8];
    if (dac_eeprom_read_raw(i2cAddr, 0, check, 8)) {
        for (int i = 0; i < 8; i++) {
            if (check[i] != 0xFF) {
                LOG_E("[DAC] EEPROM erase verify failed at byte %d", i);
                return false;
            }
        }
    }

    LOG_I("[DAC] EEPROM erase complete");
    return true;
}

// ===== Full I2C Bus Scan =====
int dac_i2c_scan(uint8_t* eepromMask) {
    if (eepromMask) *eepromMask = 0;
    int totalDevices = 0;

    i2c_init();
    LOG_I("[DAC] I2C bus scan starting (SDA=%d SCL=%d, 0x08-0x77)", DAC_I2C_SDA_PIN, DAC_I2C_SCL_PIN);

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            totalDevices++;
            LOG_I("[DAC] I2C device found at 0x%02X", addr);
            // Track EEPROM range
            if (eepromMask && addr >= DAC_EEPROM_ADDR_START && addr <= DAC_EEPROM_ADDR_END) {
                *eepromMask |= (1 << (addr - DAC_EEPROM_ADDR_START));
            }
        }
        // Log error details for EEPROM address range (helps diagnose wiring issues)
        else if (addr >= DAC_EEPROM_ADDR_START && addr <= DAC_EEPROM_ADDR_END) {
            LOG_D("[DAC] No ACK at 0x%02X (err=%d)", addr, err);
        }
    }

    LOG_I("[DAC] I2C scan: %d devices found (EEPROM mask=0x%02X)",
          totalDevices, eepromMask ? *eepromMask : 0);
    return totalDevices;
}

#else
// Native test stubs
bool dac_eeprom_scan(DacEepromData* out) {
    if (out) out->valid = false;
    return false;
}
bool dac_eeprom_read_raw(uint8_t i2cAddr, uint8_t memAddr, uint8_t* buf, int len) {
    (void)i2cAddr; (void)memAddr; (void)buf; (void)len;
    return false;
}
bool dac_eeprom_write(uint8_t i2cAddr, const uint8_t* data, int len) {
    (void)i2cAddr; (void)data; (void)len;
    return false;
}
bool dac_eeprom_erase(uint8_t i2cAddr) {
    (void)i2cAddr;
    return false;
}
int dac_i2c_scan(uint8_t* eepromMask) {
    if (eepromMask) *eepromMask = 0;
    return 0;
}
#endif

#endif // DAC_ENABLED
