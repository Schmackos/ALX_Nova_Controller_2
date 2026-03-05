#ifndef HAL_TEMP_SENSOR_H
#define HAL_TEMP_SENSOR_H

#ifdef DAC_ENABLED

#include "hal_device.h"

class HalTempSensor : public HalDevice {
public:
    HalTempSensor();

    bool probe() override;
    bool init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

    float getTemperature() const { return _lastTemp; }

private:
    float _lastTemp;
#ifndef NATIVE_TEST
#if CONFIG_IDF_TARGET_ESP32P4
    void* _handle;  // temperature_sensor_handle_t (opaque to avoid exposing IDF headers)
#endif
#endif
};

#endif // DAC_ENABLED
#endif // HAL_TEMP_SENSOR_H
