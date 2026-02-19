# ESP-IDF Native Framework Migration Roadmap

**Status**: Future milestone — `framework = arduino` remains until this is complete.
**Prerequisite**: ESP-IDF 5.x API modernisation (pioarduino platform) must be complete first.
**Estimated scope**: Large. Affects every subsystem. Recommend 6–8 phases over multiple sprints.

---

## Context

The project currently runs on Arduino-ESP32 3.x (pioarduino, IDF 5.5.2). Migrating to pure ESP-IDF framework would:
- Remove Arduino abstraction overhead (faster boot, smaller binary, more deterministic timing)
- Give direct access to IDF 5.x component APIs (no wrapper compatibility shims)
- Enable menuconfig for fine-grained hardware control (lwIP stack, FreeRTOS tick rate, NVS encryption, etc.)
- Required for potential production/certification path (CE/FCC firmware validation often prefers IDF-native builds)

**When NOT to migrate**: Arduino ecosystem libraries (LovyanGFX, WebSockets, PubSubClient, ArduinoJson, arduinoFFT) are
Arduino-only. Migrating would require replacing each with IDF-native equivalents or custom ports. This is a significant
cost—weigh carefully against the benefit.

---

## Arduino → IDF5 Replacement Map

| Arduino abstraction | Files using it | IDF5 replacement | Notes |
|---|---|---|---|
| `WiFi.h` (WiFiMulti, softAP, scan) | `wifi_manager.cpp` | `esp_wifi.h` + `esp_netif.h` | Multi-AP scan/connect needs manual state machine |
| `Wire.h` (I2C master) | `dac_eeprom.cpp`, `dac_hal.cpp` | `driver/i2c_master.h` (IDF5 new bus API) | Old `driver/i2c.h` deprecated in IDF5 |
| `Preferences.h` (NVS) | Removed (migrated to LittleFS) | N/A | Already replaced |
| `LittleFS.h` | `settings_manager.cpp`, DAC, DSP, OTA | `esp_littlefs.h` + `esp_vfs.h` | Component available; POSIX API works with C++ streams |
| `HTTPClient.h` | `ota_updater.cpp` | `esp_http_client.h` | Mature IDF component; event-driven callbacks |
| `WebSockets` lib | `websocket_handler.cpp` | `esp_websocket_client.h` (client) / custom server | Server mode: implement on top of `esp_http_server.h` |
| `ESP32WebServer` / `WebServer` | `main.cpp` | `esp_http_server.h` | Well-supported; URI handler registration model |
| `PubSubClient` (MQTT) | `mqtt_handler.cpp` | `esp_mqtt_client.h` | Full HA discovery, QoS, TLS supported natively |
| `Serial` / `HardwareSerial` | `debug_serial.h`, `main.cpp` | `driver/uart.h` or `esp_log.h` | `esp_log.h` already used internally; Serial → `uart_driver_install` |
| `ArduinoJson` | Throughout | `cJSON.h` (IDF built-in) or keep lib | cJSON is less ergonomic but zero-dependency |
| `String` class | Scattered | `std::string` | Already using `std::string` in most new code |
| `millis()` / `micros()` | Throughout | `esp_timer_get_time()` (µs) | One-liner wrapper keeps call sites clean |
| `delay()` | Throughout | `vTaskDelay(pdMS_TO_TICKS(ms))` | Already partially replaced in tasks |
| `analogRead()` | `smart_sensing.cpp` (voltage fallback) | `adc_oneshot.h` (IDF5 new ADC API) | Old `driver/adc.h` deprecated |
| `ledcAttach/Write` | `buzzer_handler.cpp`, `signal_generator.cpp`, `gui_manager.cpp` | `driver/ledc.h` | Already close to IDF-native; channel management is explicit |
| `TinyUSB` / Arduino UAC2 | `usb_audio.cpp` | `tinyusb` IDF component (direct) | Remove Arduino TinyUSB wrapper layer |
| `SPIFFS.h` | Not used (LittleFS) | N/A | |
| `esp_task_wdt_init(s, panic)` | `main.cpp` | `esp_task_wdt_config_t` struct API (IDF5) | Already using IDF4 version; upgrade to config_t form |
| `xTaskCreatePinnedToCore` | `i2s_audio.cpp`, `gui_manager.cpp` | Same (FreeRTOS API unchanged) | No change needed |

---

## Recommended Phase Order

### Phase A — NVS + LittleFS (Lowest Risk, Highest Portability Value)
- `esp_littlefs.h` is nearly a drop-in — same POSIX file API
- Remove `LittleFS.h` Arduino wrapper, call `esp_vfs_littlefs_register()` directly
- Settings, DSP presets, crash log, signal gen settings all migrate together
- **Risk**: Low. No networking or UI impact. Test with full settings export/import cycle.

### Phase B — WiFi + MQTT (Highest Value)
- Replace `WiFiMulti` with `esp_wifi` station scan + connect loop
- Replace `PubSubClient` with `esp_mqtt_client`
- Keep HA discovery JSON generation logic — only transport changes
- **Risk**: Medium. WiFi reconnect logic is complex; write integration tests.

### Phase C — HTTP Server + WebSocket Server
- Replace Arduino WebServer with `esp_http_server`
- Implement WebSocket upgrade handler on top (`httpd_ws_send_frame`)
- REST endpoints map cleanly (URI handler per endpoint)
- **Risk**: Medium-High. Many endpoints; need regression testing of all API paths.

### Phase D — Serial + Logging
- Replace `Serial.print` with `esp_log_write()` / `ESP_LOGI` macros
- `debug_serial.h` already wraps LOG_* — just reroute to `esp_log`
- **Risk**: Low. Mostly cosmetic.

### Phase E — ArduinoJson → cJSON (Optional)
- cJSON is more verbose but built-in and zero-dependency
- Alternative: keep ArduinoJson as a component (it works without Arduino framework)
- **Recommendation**: Keep ArduinoJson — cost/benefit doesn't justify replacement

### Phase F — OTA + HTTPClient
- Replace `HTTPClient` with `esp_http_client`
- OTA download: `esp_ota_begin` / `esp_ota_write` / `esp_ota_end` (already IDF-native underneath)
- SHA256 verification: `mbedtls_md.h`
- **Risk**: Low. Well-tested IDF OTA path.

### Phase G — Display + GUI (Highest Complexity)
- LovyanGFX requires Arduino SPI framework — would need porting or replacement
- LVGL itself is framework-agnostic (has IDF component)
- Alternative: keep LovyanGFX as-is (it can compile against IDF with ESP-IDF SPI driver)
- **Recommendation**: Defer or keep LovyanGFX; focus IDF migration on backend only

### Phase H — USB Audio
- TinyUSB IDF component is available; Arduino wrapper can be removed
- UAC2 descriptor + custom driver approach stays the same
- **Risk**: Medium. Driver registration changes slightly without Arduino hooks.

---

## Scope Estimates

| Phase | Effort | Files Affected | Risk |
|---|---|---|---|
| A — NVS/LittleFS | ~1 day | settings_manager, dac, dsp | Low |
| B — WiFi/MQTT | ~3 days | wifi_manager, mqtt_handler, main | Medium |
| C — HTTP/WebSocket | ~4 days | main, websocket_handler, web_pages | High |
| D — Serial/Logging | ~0.5 days | debug_serial.h, main | Low |
| E — ArduinoJson | Skip (keep lib) | — | — |
| F — OTA/HTTP | ~1 day | ota_updater | Low |
| G — GUI/Display | Defer | gui/* | Very High |
| H — USB Audio | ~1.5 days | usb_audio | Medium |

**Total (excluding GUI)**: ~10–11 dev-days if done sequentially; ~7–8 with parallelism.

---

## Prerequisites Before Starting

1. All IDF 5.x API modernisation complete (I2S channel API, LEDC pin-based, FreeRTOS SMP affinity)
2. Full native test suite passing (790+ tests)
3. Hardware validated on pioarduino IDF 5.5.x
4. Feature freeze on new capabilities during migration window
5. CI must remain green throughout — each phase is a separate PR

---

## Notes

- `framework = arduino` in `platformio.ini` → change to `framework = espidf` when ready
- `lib_deps` Arduino libraries (WebSockets, PubSubClient, lvgl, LovyanGFX) will need
  IDF component replacements or must be confirmed to compile under `espidf` framework
- ArduinoJson v7 works without Arduino framework — safe to keep
- arduinoFFT native-test dependency is unaffected (native platform)
