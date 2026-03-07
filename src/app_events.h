#pragma once

#ifdef UNIT_TEST
// Native test stubs — all no-ops
#include <stdint.h>
typedef uint32_t EventBits_t;
#define app_events_init()           ((void)0)
#define app_events_signal(bits)     ((void)0)
#define app_events_wait(timeout_ms) (0U)
#else
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
void app_events_init();
void app_events_signal(EventBits_t bits);
EventBits_t app_events_wait(uint32_t timeout_ms);
#endif

// Event bit definitions — one per live dirty flag (13 total)
#define EVT_OTA          (1UL <<  0)
#define EVT_DISPLAY      (1UL <<  1)
#define EVT_BUZZER       (1UL <<  2)
#define EVT_SIGGEN       (1UL <<  3)
#define EVT_DSP_CONFIG   (1UL <<  4)
#define EVT_DAC          (1UL <<  5)
#define EVT_EEPROM       (1UL <<  6)
#define EVT_USB_AUDIO    (1UL <<  7)
#define EVT_USB_VU       (1UL <<  8)
#define EVT_SETTINGS     (1UL <<  9)
#define EVT_ADC_ENABLED  (1UL << 10)
// Bits 15-23 reserved for future use (9 spare bits in 24-bit event group)
#define EVT_ETHERNET     (1UL << 11)
#define EVT_IO_REGISTRY  (1UL << 12)
#define EVT_DAC_SETTINGS (1UL << 13)
#define EVT_HAL_DEVICE   (1UL << 14)
#define EVT_CHANNEL_MAP  (1UL << 15)
#define EVT_ANY          (0xFFFFUL)
