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

// Event bit definitions — one per live dirty flag (bits 0-17 assigned, bits 5 & 13 freed, 18-23 spare)
#define EVT_OTA          (1UL <<  0)
#define EVT_DISPLAY      (1UL <<  1)
#define EVT_BUZZER       (1UL <<  2)
#define EVT_SIGGEN       (1UL <<  3)
#define EVT_DSP_CONFIG   (1UL <<  4)
#define EVT_EEPROM       (1UL <<  6)
#define EVT_USB_AUDIO    (1UL <<  7)
#define EVT_USB_VU       (1UL <<  8)
#define EVT_SETTINGS     (1UL <<  9)
#define EVT_ADC_ENABLED  (1UL << 10)
#define EVT_DIAG         (1UL << 12)
#define EVT_ETHERNET     (1UL << 11)
#define EVT_HAL_DEVICE   (1UL << 14)
#define EVT_CHANNEL_MAP  (1UL << 15)
#define EVT_HEAP_PRESSURE (1UL << 16)
#define EVT_HEALTH       (1UL << 17)
#define EVT_FORMAT_CHANGE (1UL << 18)  // Source/sink sample rate mismatch or DSD detection change
#define EVT_THD          (1UL << 19)   // THD+N measurement completed
// EVT_ANY covers all 24 usable FreeRTOS event group bits (bits 24-31 reserved by FreeRTOS)
#define EVT_ANY          (0x00FFFFFFUL)
