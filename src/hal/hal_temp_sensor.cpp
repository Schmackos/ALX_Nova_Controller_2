#ifdef DAC_ENABLED

#include "hal_temp_sensor.h"
#include "../debug_serial.h"
#include <string.h>

#ifndef NATIVE_TEST
#if CONFIG_IDF_TARGET_ESP32P4
#include <driver/temperature_sensor.h>
#endif
#endif

HalTempSensor::HalTempSensor()
    : _lastTemp(0.0f)
{
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "espressif,esp32p4-temp", 31);
    strncpy(_descriptor.name, "Chip Temperature", 32);
    _descriptor.type = HAL_DEV_SENSOR;
    _descriptor.bus.type = HAL_BUS_INTERNAL;
    _descriptor.bus.index = 0;
    _descriptor.channelCount = 1;
    _initPriority = HAL_PRIORITY_LATE;

#ifndef NATIVE_TEST
#if CONFIG_IDF_TARGET_ESP32P4
    _handle = nullptr;
#endif
#endif
}

bool HalTempSensor::probe()
{
#ifndef NATIVE_TEST
#if CONFIG_IDF_TARGET_ESP32P4
    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    temperature_sensor_handle_t handle = nullptr;
    esp_err_t err = temperature_sensor_install(&cfg, &handle);
    if (err != ESP_OK || handle == nullptr) {
        LOG_W("[TempSensor] probe failed: err=0x%x", err);
        _state = HAL_STATE_REMOVED;
        return false;
    }
    _handle = (void*)handle;
    _state = HAL_STATE_DETECTED;
    LOG_I("[TempSensor] probe OK — ESP32-P4 internal temperature sensor");
    return true;
#else
    // Non-P4 ESP32 targets: sensor API differs, not supported here
    _state = HAL_STATE_REMOVED;
    return false;
#endif
#else
    // NATIVE_TEST: no hardware
    _state = HAL_STATE_REMOVED;
    return false;
#endif
}

bool HalTempSensor::init()
{
#ifndef NATIVE_TEST
#if CONFIG_IDF_TARGET_ESP32P4
    if (_handle == nullptr) {
        LOG_E("[TempSensor] init called without successful probe");
        _state = HAL_STATE_ERROR;
        return false;
    }

    temperature_sensor_handle_t handle = (temperature_sensor_handle_t)_handle;
    esp_err_t err = temperature_sensor_enable(handle);
    if (err != ESP_OK) {
        LOG_E("[TempSensor] enable failed: err=0x%x", err);
        _state = HAL_STATE_ERROR;
        return false;
    }

    // Read initial value
    float temp = 0.0f;
    err = temperature_sensor_get_celsius(handle, &temp);
    if (err == ESP_OK) {
        _lastTemp = temp;
        LOG_I("[TempSensor] init OK — initial temp: %.1f C", _lastTemp);
    } else {
        LOG_W("[TempSensor] init OK but initial read failed: err=0x%x", err);
    }

    _ready = true;
    _state = HAL_STATE_AVAILABLE;
    return true;
#else
    _state = HAL_STATE_ERROR;
    return false;
#endif
#else
    _state = HAL_STATE_ERROR;
    return false;
#endif
}

void HalTempSensor::deinit()
{
#ifndef NATIVE_TEST
#if CONFIG_IDF_TARGET_ESP32P4
    if (_handle != nullptr) {
        temperature_sensor_handle_t handle = (temperature_sensor_handle_t)_handle;
        temperature_sensor_disable(handle);
        temperature_sensor_uninstall(handle);
        _handle = nullptr;
        LOG_I("[TempSensor] deinit — sensor uninstalled");
    }
#endif
#endif
    _ready = false;
    _state = HAL_STATE_REMOVED;
    _lastTemp = 0.0f;
}

void HalTempSensor::dumpConfig()
{
    LOG_I("[TempSensor] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[TempSensor]   bus: INTERNAL, state: %d, ready: %d",
          (int)_state, (int)_ready);
    LOG_I("[TempSensor]   last temperature: %.1f C", _lastTemp);
}

bool HalTempSensor::healthCheck()
{
#ifndef NATIVE_TEST
#if CONFIG_IDF_TARGET_ESP32P4
    if (_handle == nullptr) {
        _state = HAL_STATE_UNAVAILABLE;
        _ready = false;
        return false;
    }

    float temp = 0.0f;
    temperature_sensor_handle_t handle = (temperature_sensor_handle_t)_handle;
    esp_err_t err = temperature_sensor_get_celsius(handle, &temp);
    if (err != ESP_OK) {
        LOG_W("[TempSensor] healthCheck read failed: err=0x%x", err);
        _state = HAL_STATE_UNAVAILABLE;
        _ready = false;
        return false;
    }

    // Sanity range check: -30 to 125 degrees Celsius
    if (temp < -30.0f || temp > 125.0f) {
        LOG_W("[TempSensor] healthCheck out of range: %.1f C", temp);
        _state = HAL_STATE_UNAVAILABLE;
        _ready = false;
        return false;
    }

    _lastTemp = temp;
    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    return true;
#else
    return false;
#endif
#else
    return false;
#endif
}

#endif // DAC_ENABLED
