// ESP-DSP Lite — error type shim (replaces ESP-IDF esp_err.h)
#ifndef DSP_ERR_H
#define DSP_ERR_H

#ifndef ESP_OK
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_DSP_INVALID_LENGTH 0x100
#define ESP_ERR_DSP_INVALID_PARAM  0x101
#endif

#endif // DSP_ERR_H
