#include "hal_device_manager.h"
#include <string.h>

// ===== Singleton =====
HalDeviceManager& HalDeviceManager::instance() {
    static HalDeviceManager mgr;
    return mgr;
}

HalDeviceManager::HalDeviceManager() : _count(0) {
    memset(_devices, 0, sizeof(_devices));
    memset(_configs, 0, sizeof(_configs));
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        _configs[i].valid = false;
        _configs[i].pinSda = -1;
        _configs[i].pinScl = -1;
        _configs[i].pinMclk = -1;
        _configs[i].pinData = -1;
        _configs[i].i2sPort = 255;
        _configs[i].enabled = true;
    }
    for (int i = 0; i < HAL_MAX_PINS; i++) {
        _pins[i].gpio = -1;
    }
}

// ===== Registration =====
int HalDeviceManager::registerDevice(HalDevice* device, HalDiscovery discovery) {
    if (!device || _count >= HAL_MAX_DEVICES) return -1;

    // Find first free slot
    int slot = -1;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (!_devices[i]) { slot = i; break; }
    }
    if (slot < 0) return -1;

    device->_slot = static_cast<uint8_t>(slot);
    device->_discovery = discovery;
    _devices[slot] = device;
    _count++;
    return slot;
}

bool HalDeviceManager::removeDevice(uint8_t slot) {
    if (slot >= HAL_MAX_DEVICES || !_devices[slot]) return false;

    HalDeviceState oldState = _devices[slot]->_state;
    _devices[slot]->_ready = false;
    _devices[slot]->_state = HAL_STATE_REMOVED;
    if (_stateChangeCb) _stateChangeCb(slot, oldState, HAL_STATE_REMOVED);
    _devices[slot] = nullptr;
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
            if (dev->init()) {
                dev->_state = HAL_STATE_AVAILABLE;
                dev->_ready = true;
                if (_stateChangeCb) _stateChangeCb(dev->_slot, HAL_STATE_CONFIGURING, HAL_STATE_AVAILABLE);
            } else {
                dev->_state = HAL_STATE_ERROR;
                dev->_ready = false;
                if (_stateChangeCb) _stateChangeCb(dev->_slot, HAL_STATE_CONFIGURING, HAL_STATE_ERROR);
            }
        }
    }
}

void HalDeviceManager::healthCheckAll() {
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (!_devices[i]) continue;
        HalDevice* dev = _devices[i];
        if (dev->_state != HAL_STATE_AVAILABLE && dev->_state != HAL_STATE_UNAVAILABLE) continue;

        if (dev->healthCheck()) {
            if (dev->_state == HAL_STATE_UNAVAILABLE) {
                HalDeviceState oldState = dev->_state;
                dev->_state = HAL_STATE_AVAILABLE;
                dev->_ready = true;
                if (_stateChangeCb) _stateChangeCb(static_cast<uint8_t>(i), oldState, HAL_STATE_AVAILABLE);
            }
        } else {
            if (dev->_state != HAL_STATE_UNAVAILABLE) {
                HalDeviceState oldState = dev->_state;
                dev->_state = HAL_STATE_UNAVAILABLE;
                dev->_ready = false;
                if (_stateChangeCb) _stateChangeCb(static_cast<uint8_t>(i), oldState, HAL_STATE_UNAVAILABLE);
            } else {
                dev->_state = HAL_STATE_UNAVAILABLE;
                dev->_ready = false;
            }
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

// ===== State Change Callback =====
void HalDeviceManager::setStateChangeCallback(HalStateChangeCb cb) {
    _stateChangeCb = cb;
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
    }
    for (int i = 0; i < HAL_MAX_PINS; i++) {
        _pins[i].gpio = -1;
    }
    _count = 0;
    _stateChangeCb = nullptr;
}
