#ifdef DAC_ENABLED

#include "hal_pipeline_bridge.h"
#include "hal_device_manager.h"
#include "hal_types.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../app_state.h"
#else
#define LOG_I(...)
#define LOG_W(...)
#endif

// Track which slots are pipeline-registered
static bool _pipelineOutputSlots[HAL_MAX_DEVICES] = {};
static bool _pipelineInputSlots[HAL_MAX_DEVICES] = {};

void hal_pipeline_sync() {
    HalDeviceManager& mgr = HalDeviceManager::instance();
    int outputs = 0;
    int inputs = 0;

    mgr.forEach([](HalDevice* dev, void* ctx) {
        int* counts = static_cast<int*>(ctx);
        uint8_t slot = dev->getSlot();

        if (dev->_state == HAL_STATE_AVAILABLE && dev->_ready) {
            HalDeviceType type = dev->getType();
            if (type == HAL_DEV_DAC || type == HAL_DEV_CODEC) {
                _pipelineOutputSlots[slot] = true;
                counts[0]++;
            }
            if (type == HAL_DEV_ADC || type == HAL_DEV_CODEC) {
                _pipelineInputSlots[slot] = true;
                counts[1]++;
            }
        }
    }, (void*)((int[]){0, 0}));

    // Recount properly
    outputs = 0;
    inputs = 0;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_pipelineOutputSlots[i]) outputs++;
        if (_pipelineInputSlots[i]) inputs++;
    }

    LOG_I("[HAL] Pipeline sync: %d output(s), %d input(s) registered", outputs, inputs);
}

void hal_pipeline_on_device_available(uint8_t slot) {
    if (slot >= HAL_MAX_DEVICES) return;

    HalDeviceManager& mgr = HalDeviceManager::instance();
    HalDevice* dev = mgr.getDevice(slot);
    if (!dev) return;

    HalDeviceType type = dev->getType();
    if (type == HAL_DEV_DAC || type == HAL_DEV_CODEC) {
        _pipelineOutputSlots[slot] = true;
        LOG_I("[HAL] Pipeline: output registered in slot %d (%s)", slot, dev->getDescriptor().name);
    }
    if (type == HAL_DEV_ADC || type == HAL_DEV_CODEC) {
        _pipelineInputSlots[slot] = true;
        LOG_I("[HAL] Pipeline: input registered in slot %d (%s)", slot, dev->getDescriptor().name);
    }
}

void hal_pipeline_on_device_removed(uint8_t slot) {
    if (slot >= HAL_MAX_DEVICES) return;

    if (_pipelineOutputSlots[slot]) {
        _pipelineOutputSlots[slot] = false;
        LOG_I("[HAL] Pipeline: output removed from slot %d", slot);
    }
    if (_pipelineInputSlots[slot]) {
        _pipelineInputSlots[slot] = false;
        LOG_I("[HAL] Pipeline: input removed from slot %d", slot);
    }
}

int hal_pipeline_output_count() {
    int count = 0;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_pipelineOutputSlots[i]) count++;
    }
    return count;
}

int hal_pipeline_input_count() {
    int count = 0;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_pipelineInputSlots[i]) count++;
    }
    return count;
}

#endif // DAC_ENABLED
