#pragma once
#ifdef NATIVE_TEST
#include <cstring>
#include <map>
#include <vector>
#include <stdint.h>
#include <cstdio>

namespace WireMock {
    // Register map: [i2cAddress][regIndex] = value
    static std::map<uint8_t, std::map<uint8_t, uint8_t>> registerMap;
    static std::vector<uint8_t> txBuffer;
    static std::vector<uint8_t> rxBuffer;
    static uint8_t currentAddr = 0;
    static bool txInProgress = false;
    static int rxIndex = 0;
    static bool busInitialized[3];  // [0]=EXT (GPIO48/54), [1]=ONBOARD (GPIO7/8), [2]=EXP (GPIO28/29)
    static int pinSDA[3];
    static int pinSCL[3];
    static std::map<uint8_t, int> addressToBusIndex;  // i2cAddr -> busIndex

    inline void reset() {
        registerMap.clear();
        txBuffer.clear();
        rxBuffer.clear();
        currentAddr = 0;
        txInProgress = false;
        rxIndex = 0;
        memset(busInitialized, 0, sizeof(busInitialized));
        for (int i = 0; i < 3; i++) { pinSDA[i] = -1; pinSCL[i] = -1; }
        addressToBusIndex.clear();
    }

    // Pre-populate a mock device at i2cAddr on busIdx with initial register values
    inline void registerDevice(uint8_t addr, int busIdx,
                                const uint8_t* initRegs = nullptr, size_t len = 0) {
        addressToBusIndex[addr] = busIdx;
        if (initRegs) {
            for (size_t i = 0; i < len; i++) registerMap[addr][(uint8_t)i] = initRegs[i];
        }
    }

    inline bool isBusInitialized(int busIdx) {
        return busIdx >= 0 && busIdx < 3 && busInitialized[busIdx];
    }

    inline bool pinClaimed(int gpio) {
        for (int i = 0; i < 3; i++) {
            if (pinSDA[i] == gpio || pinSCL[i] == gpio) return true;
        }
        return false;
    }

    inline int determineBusIndex(int sda, int scl) {
        if (sda == 48 || scl == 54) return 0;
        if (sda == 7  || scl == 8)  return 1;
        if (sda == 28 || scl == 29) return 2;
        return 0;
    }
}

class WireClass {
public:
    bool begin(int sda = -1, int scl = -1, uint32_t freq = 100000) {
        int idx = WireMock::determineBusIndex(sda, scl);
        if (idx >= 0 && idx < 3) {
            WireMock::busInitialized[idx] = true;
            WireMock::pinSDA[idx] = sda;
            WireMock::pinSCL[idx] = scl;
        }
        return true;
    }

    void beginTransmission(uint8_t addr) {
        WireMock::currentAddr = addr;
        WireMock::txBuffer.clear();
        WireMock::txInProgress = true;
    }

    size_t write(uint8_t b) {
        if (WireMock::txInProgress) WireMock::txBuffer.push_back(b);
        return 1;
    }

    size_t write(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; i++) write(data[i]);
        return len;
    }

    uint8_t endTransmission(bool sendStop = true) {
        (void)sendStop;
        if (!WireMock::txInProgress) return 5;
        // Address not registered -> NACK
        if (WireMock::addressToBusIndex.find(WireMock::currentAddr) ==
            WireMock::addressToBusIndex.end()) {
            WireMock::txInProgress = false;
            return 2;  // NACK
        }
        // Multi-byte write: txBuffer[0] = register addr, txBuffer[1..n] = values
        if (WireMock::txBuffer.size() >= 2) {
            uint8_t reg = WireMock::txBuffer[0];
            for (size_t i = 1; i < WireMock::txBuffer.size(); i++) {
                WireMock::registerMap[WireMock::currentAddr][(uint8_t)(reg + i - 1)] =
                    WireMock::txBuffer[i];
            }
        }
        WireMock::txInProgress = false;
        return 0;  // ACK
    }

    uint8_t requestFrom(uint8_t addr, uint8_t len, bool stop = true) {
        (void)stop;
        if (WireMock::addressToBusIndex.find(addr) == WireMock::addressToBusIndex.end()) return 0;
        WireMock::rxBuffer.clear();
        for (uint8_t i = 0; i < len; i++) {
            auto& regmap = WireMock::registerMap[addr];
            auto it = regmap.find(i);
            WireMock::rxBuffer.push_back(it != regmap.end() ? it->second : 0);
        }
        WireMock::rxIndex = 0;
        return len;
    }

    int available() {
        return (int)WireMock::rxBuffer.size() - WireMock::rxIndex;
    }

    int read() {
        if (WireMock::rxIndex < (int)WireMock::rxBuffer.size())
            return WireMock::rxBuffer[WireMock::rxIndex++];
        return -1;
    }

    void end() {}
    void setClock(uint32_t freq) { (void)freq; }
};

static WireClass Wire;
static WireClass Wire1;  // Second I2C bus alias
#endif // NATIVE_TEST
