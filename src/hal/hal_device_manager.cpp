#include "hal_device_manager.h"
#include "../diag_journal.h"
#include <string.h>

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "../debug_serial.h"
#else
#include "../../test/test_mocks/Arduino.h"
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#endif

// Maximum retries before permanent ERROR
static const uint8_t HAL_MAX_RETRIES = 3;

// ===== Singleton =====
HalDeviceManager& HalDeviceManager::instance() {
    static HalDeviceManager mgr;
    return mgr;
}

HalDeviceManager::HalDeviceManager() : _count(0) {
    memset(_devices, 0, sizeof(_devices));
    memset(_configs, 0, sizeof(_configs));
    memset(_retryState, 0, sizeof(_retryState));
    memset(_faultCount, 0, sizeof(_faultCount));
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        _configs[i].valid = false;
        _configs[i].pinSda = -1;
        _configs[i].pinScl = -1;
        _configs[i].pinMclk = -1;
        _configs[i].pinData = -1;
        _configs[i].i2sPort = 255;
        _configs[i].enabled = true;
        _configs[i].gpioA = -1;
        _configs[i].gpioB = -1;
        _configs[i].gpioC = -1;
        _configs[i].gpioD = -1;
        _configs[i].usbPid = 0;
    }
    for (int i = 0; i < HAL_MAX_PINS; i++) {
        _pins[i].gpio = -1;
    }
}

// ===== Registration =====
int HalDeviceManager::registerDevice(HalDevice* device, HalDiscovery discovery) {
    if (!device || _count >= HAL_MAX_DEVICES) {
        if (device) {
            LOG_W("[HAL] Device slots full (%d/%d): %s", _count, HAL_MAX_DEVICES, device->getDescriptor().name);
            diag_emit(DIAG_HAL_SLOT_FULL, DIAG_SEV_ERROR, 0, device->getDescriptor().name, "slots full");
        }
        return -1;
    }

    // Find first free slot
    int slot = -1;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (!_devices[i]) { slot = i; break; }
    }
    if (slot < 0) return -1;

    device->_slot = static_cast<uint8_t>(slot);
    device->_discovery = discovery;

    // Auto-assign instanceId by counting existing devices with the same compatible string
    device->_descriptor.instanceId = countByCompatible(device->getDescriptor().compatible);

    _devices[slot] = device;
    _resetRetryState(static_cast<uint8_t>(slot));
    _count++;

    diag_emit(DIAG_HAL_DEVICE_DETECTED, DIAG_SEV_INFO,
              static_cast<uint8_t>(slot), device->getDescriptor().name, "registered");
    return slot;
}

bool HalDeviceManager::removeDevice(uint8_t slot) {
    if (slot >= HAL_MAX_DEVICES || !_devices[slot]) return false;

    HalDeviceState oldState = _devices[slot]->_state;
    const char* name = _devices[slot]->getDescriptor().name;
    _devices[slot]->_ready = false;
    _devices[slot]->_state = HAL_STATE_REMOVED;

    diag_emit(DIAG_HAL_DEVICE_REMOVED, DIAG_SEV_WARN,
              slot, name, "removed");

    if (_stateChangeCb) _stateChangeCb(slot, oldState, HAL_STATE_REMOVED);
    _devices[slot] = nullptr;
    _resetRetryState(slot);
    _count--;
    return true;
}

// ===== Lookup =====
HalDevice* HalDeviceManager::getDevice(uint8_t slot) {
    if (slot >= HAL_MAX_DEVICES) return nullptr;
    return _devices[slot];
}

HalDevice* HalDeviceManager::findByCompatible(const char* compatible) {
    if (!compatible) return nullptr;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_devices[i] && strcmp(_devices[i]->getDescriptor().compatible, compatible) == 0) {
            return _devices[i];
        }
    }
    return nullptr;
}

HalDevice* HalDeviceManager::findByCompatible(const char* compatible, uint8_t instanceId) {
    if (!compatible) return nullptr;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_devices[i] &&
            strcmp(_devices[i]->getDescriptor().compatible, compatible) == 0 &&
            _devices[i]->getDescriptor().instanceId == instanceId) {
            return _devices[i];
        }
    }
    return nullptr;
}

uint8_t HalDeviceManager::countByCompatible(const char* compatible) const {
    if (!compatible) return 0;
    uint8_t count = 0;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_devices[i] && strcmp(_devices[i]->getDescriptor().compatible, compatible) == 0) {
            count++;
        }
    }
    return count;
}

HalDevice* HalDeviceManager::findByType(HalDeviceType type, uint8_t nth) {
    uint8_t found = 0;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_devices[i] && _devices[i]->getType() == type) {
            if (found == nth) return _devices[i];
            found++;
        }
    }
    return nullptr;
}

uint8_t HalDeviceManager::getCount() const {
    return _count;
}

// ===== Lifecycle =====
void HalDeviceManager::initAll() {
    // Collect non-null devices into a sortable array
    HalDevice* sorted[HAL_MAX_DEVICES];
    int n = 0;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_devices[i]) sorted[n++] = _devices[i];
    }

    // Insertion sort by priority descending (stable, small N)
    for (int i = 1; i < n; i++) {
        HalDevice* key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j]->getInitPriority() < key->getInitPriority()) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    // Init in priority order
    for (int i = 0; i < n; i++) {
        HalDevice* dev = sorted[i];
        if (dev->_state == HAL_STATE_UNKNOWN || dev->_state == HAL_STATE_CONFIGURING) {
            HalDeviceState oldState = dev->_state;
            dev->_state = HAL_STATE_CONFIGURING;
            if (_stateChangeCb && oldState != HAL_STATE_CONFIGURING) {
                _stateChangeCb(dev->_slot, oldState, HAL_STATE_CONFIGURING);
            }

            HalInitResult result = dev->init();
            if (result.success) {
                dev->clearLastError();
                dev->_state = HAL_STATE_AVAILABLE;
                dev->_ready = true;
                _resetRetryState(dev->_slot);
                if (_stateChangeCb) _stateChangeCb(dev->_slot, HAL_STATE_CONFIGURING, HAL_STATE_AVAILABLE);
            } else {
                dev->setLastError(result);
                dev->_state = HAL_STATE_ERROR;
                dev->_ready = false;
                _retryState[dev->_slot].lastErrorCode = result.errorCode;
                diag_emit((DiagErrorCode)result.errorCode, DIAG_SEV_ERROR,
                          dev->_slot, dev->getDescriptor().name, result.reason);
                if (_stateChangeCb) _stateChangeCb(dev->_slot, HAL_STATE_CONFIGURING, HAL_STATE_ERROR);
            }
        }
    }
}

void HalDeviceManager::healthCheckAll() {
    uint32_t now = millis();

    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (!_devices[i]) continue;
        HalDevice* dev = _devices[i];

        // --- Non-blocking retry for UNAVAILABLE/ERROR devices ---
        if (dev->_state == HAL_STATE_UNAVAILABLE || dev->_state == HAL_STATE_ERROR) {
            HalRetryState& rs = _retryState[i];
            if (rs.count >= HAL_MAX_RETRIES) continue; // Exhausted — permanent ERROR
            if (now < rs.nextRetryMs) continue;         // Not time yet

            // Attempt reinit
            dev->deinit();
            HalInitResult result = dev->init();

            if (result.success) {
                // Recovery succeeded
                HalDeviceState oldState = dev->_state;
                dev->clearLastError();
                dev->_state = HAL_STATE_AVAILABLE;
                dev->_ready = true;
                _resetRetryState(static_cast<uint8_t>(i));
                diag_emit(DIAG_HAL_REINIT_OK, DIAG_SEV_INFO,
                          static_cast<uint8_t>(i), dev->getDescriptor().name, "reinit OK");
                if (_stateChangeCb) _stateChangeCb(static_cast<uint8_t>(i), oldState, HAL_STATE_AVAILABLE);
            } else {
                // Recovery failed — store the latest error reason
                dev->setLastError(result);
                rs.count++;
                rs.lastErrorCode = result.errorCode;
                // Exponential backoff: 1s, 2s, 4s
                rs.nextRetryMs = now + (1000UL << (rs.count - 1));

                if (rs.count >= HAL_MAX_RETRIES) {
                    // Exhausted — permanent ERROR
                    HalDeviceState oldState = dev->_state;
                    dev->_state = HAL_STATE_ERROR;
                    dev->_ready = false;
                    _faultCount[i]++;
                    diag_emit(DIAG_HAL_REINIT_EXHAUSTED, DIAG_SEV_CRIT,
                              static_cast<uint8_t>(i), dev->getDescriptor().name, "retries exhausted");
                    if (_stateChangeCb && oldState != HAL_STATE_ERROR) {
                        _stateChangeCb(static_cast<uint8_t>(i), oldState, HAL_STATE_ERROR);
                    }
                } else {
                    diag_emit(DIAG_HAL_HEALTH_FAIL, DIAG_SEV_WARN,
                              static_cast<uint8_t>(i), dev->getDescriptor().name, result.reason);
                }
            }
            continue;
        }

        // --- Normal health check for AVAILABLE devices ---
        if (dev->_state != HAL_STATE_AVAILABLE) continue;

        if (!dev->healthCheck()) {
            HalDeviceState oldState = dev->_state;
            dev->_state = HAL_STATE_UNAVAILABLE;
            dev->_ready = false;
            // Start retry sequence
            HalRetryState& rs = _retryState[i];
            rs.count = 0;
            rs.nextRetryMs = now + 1000; // First retry in 1s
            diag_emit(DIAG_HAL_HEALTH_FAIL, DIAG_SEV_WARN,
                      static_cast<uint8_t>(i), dev->getDescriptor().name, "health check failed");
            if (_stateChangeCb) _stateChangeCb(static_cast<uint8_t>(i), oldState, HAL_STATE_UNAVAILABLE);
        }
    }
}

// ===== Iteration =====
void HalDeviceManager::forEach(HalDeviceCallback cb, void* ctx) {
    if (!cb) return;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_devices[i]) cb(_devices[i], ctx);
    }
}

// ===== Pin Claim Tracking =====
bool HalDeviceManager::claimPin(int8_t gpio, HalBusType bus, uint8_t busIndex, uint8_t slot) {
    if (gpio < 0) return false;
    if (gpio > HAL_GPIO_MAX) {
        LOG_W("[HAL] claimPin: GPIO%d exceeds max (%d)", gpio, HAL_GPIO_MAX);
        return false;
    }

    // Check not already claimed
    for (int i = 0; i < HAL_MAX_PINS; i++) {
        if (_pins[i].gpio == gpio) return false;  // Already claimed
    }

    // Find free pin slot
    for (int i = 0; i < HAL_MAX_PINS; i++) {
        if (_pins[i].gpio < 0) {
            _pins[i].gpio = gpio;
            _pins[i].bus = bus;
            _pins[i].busIndex = busIndex;
            _pins[i].slot = slot;
            return true;
        }
    }
    LOG_W("[HAL] claimPin: pin table full (%d slots), GPIO%d not tracked", HAL_MAX_PINS, gpio);
    return false;  // Pin table full
}

bool HalDeviceManager::releasePin(int8_t gpio) {
    for (int i = 0; i < HAL_MAX_PINS; i++) {
        if (_pins[i].gpio == gpio) {
            _pins[i].gpio = -1;
            return true;
        }
    }
    return false;
}

bool HalDeviceManager::isPinClaimed(int8_t gpio) const {
    if (gpio < 0 || gpio > HAL_GPIO_MAX) return false;
    for (int i = 0; i < HAL_MAX_PINS; i++) {
        if (_pins[i].gpio == gpio) return true;
    }
    return false;
}

// ===== Per-device Config =====
HalDeviceConfig* HalDeviceManager::getConfig(uint8_t slot) {
    if (slot >= HAL_MAX_DEVICES) return nullptr;
    return &_configs[slot];
}

bool HalDeviceManager::setConfig(uint8_t slot, const HalDeviceConfig& cfg) {
    if (slot >= HAL_MAX_DEVICES) return false;
    _configs[slot] = cfg;
    _configs[slot].valid = true;
    return true;
}

// ===== Retry State Accessors =====
const HalRetryState* HalDeviceManager::getRetryState(uint8_t slot) const {
    if (slot >= HAL_MAX_DEVICES) return nullptr;
    return &_retryState[slot];
}

uint8_t HalDeviceManager::getFaultCount(uint8_t slot) const {
    if (slot >= HAL_MAX_DEVICES) return 0;
    return _faultCount[slot];
}

// ===== State Change Callback =====
void HalDeviceManager::setStateChangeCallback(HalStateChangeCb cb) {
    _stateChangeCb = cb;
}

// ===== Internal helpers =====
void HalDeviceManager::_resetRetryState(uint8_t slot) {
    if (slot >= HAL_MAX_DEVICES) return;
    _retryState[slot].count = 0;
    _retryState[slot].nextRetryMs = 0;
    _retryState[slot].lastErrorCode = 0;
}

// ===== Reset (testing) =====
void HalDeviceManager::reset() {
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        _devices[i] = nullptr;
    }
    memset(_configs, 0, sizeof(_configs));
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        _configs[i].valid = false;
        _configs[i].pinSda = -1;
        _configs[i].pinScl = -1;
        _configs[i].pinMclk = -1;
        _configs[i].pinData = -1;
        _configs[i].i2sPort = 255;
        _configs[i].enabled = true;
        _configs[i].gpioA = -1;
        _configs[i].gpioB = -1;
        _configs[i].gpioC = -1;
        _configs[i].gpioD = -1;
        _configs[i].usbPid = 0;
    }
    for (int i = 0; i < HAL_MAX_PINS; i++) {
        _pins[i].gpio = -1;
    }
    memset(_retryState, 0, sizeof(_retryState));
    memset(_faultCount, 0, sizeof(_faultCount));
    _count = 0;
    _stateChangeCb = nullptr;
}
