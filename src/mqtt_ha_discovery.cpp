// mqtt_ha_discovery.cpp
// Home Assistant MQTT auto-discovery configuration publishing and removal.
// Extracted from mqtt_handler.cpp as part of a 3-file split.
//
// Functions:
//   addHADeviceInfo()   — static helper: injects device info + availability into
//                         any JsonDocument before publishing a discovery payload
//   publishHADiscovery() — publishes all HA entity configs (~100 entities)
//   removeHADiscovery()  — wipes all HA entity configs with empty-payload retain

#include "mqtt_handler.h"
#include "app_state.h"
#include "globals.h"
#include "config.h"
#include "debug_serial.h"
#include "hal/hal_device_manager.h"
#include <Arduino.h>
#include <ArduinoJson.h>

// ===== Home Assistant Auto-Discovery =====

// ArduinoJson v7: JsonDocument heap-allocates tree nodes via malloc().
// Only ~24 bytes on stack per instance. Static reuse here avoids
// repeated malloc/free for 23+ HA entities on mqtt_task (4KB stack).
static JsonDocument haDoc;

// Helper function to create device info JSON object
static void addHADeviceInfo(JsonDocument &doc) {
  String deviceId = getMqttDeviceId();

  // Extract short ID for display name
  uint64_t chipId = ESP.getEfuseMac();
  uint16_t shortId = (uint16_t)(chipId & 0xFFFF);
  char idBuf[5];
  snprintf(idBuf, sizeof(idBuf), "%04X", shortId);

  JsonObject device = doc["device"].to<JsonObject>();
  JsonArray identifiers = device["identifiers"].to<JsonArray>();
  identifiers.add(deviceId);
  device["name"] = String(MANUFACTURER_MODEL) + " " + idBuf;
  device["model"] = MANUFACTURER_MODEL;
  device["manufacturer"] = MANUFACTURER_NAME;
  device["serial_number"] = appState.general.deviceSerialNumber;
  device["sw_version"] = firmwareVer;
  device["configuration_url"] = "http://" + WiFi.localIP().toString();

  // Availability — tells HA which topic indicates online/offline
  String availBase = getEffectiveMqttBaseTopic();
  JsonArray avail = doc["availability"].to<JsonArray>();
  JsonObject a = avail.add<JsonObject>();
  a["topic"] = availBase + "/status";
  a["payload_available"] = "online";
  a["payload_not_available"] = "offline";
}

// Publish Home Assistant auto-discovery configuration
void publishHADiscovery() {
  if (appState.debug.heapCritical) {
    LOG_W("[MQTT] Skipping HA discovery — heap critical");
    return;
  }
  if (!mqttClient.connected() || !appState.mqtt.haDiscovery)
    return;

  LOG_I("[MQTT] Publishing Home Assistant discovery configs...");

  String deviceId = getMqttDeviceId();
  String base = getEffectiveMqttBaseTopic();

  // ===== Amplifier Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Amplifier";
    haDoc["unique_id"] = deviceId + "_amplifier";
    haDoc["state_topic"] = base + "/smartsensing/amplifier";
    haDoc["command_topic"] = base + "/smartsensing/amplifier/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["icon"] = "mdi:amplifier";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/amplifier/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== AP Mode Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Access Point";
    haDoc["unique_id"] = deviceId + "_ap";
    haDoc["state_topic"] = base + "/ap/enabled";
    haDoc["command_topic"] = base + "/ap/enabled/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["icon"] = "mdi:access-point";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/ap/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Smart Sensing Mode Select =====
  {
    haDoc.clear();
    haDoc["name"] = "Smart Sensing Mode";
    haDoc["unique_id"] = deviceId + "_mode";
    haDoc["state_topic"] = base + "/smartsensing/mode";
    haDoc["command_topic"] = base + "/smartsensing/mode/set";
    JsonArray options = haDoc["options"].to<JsonArray>();
    options.add("always_on");
    options.add("always_off");
    options.add("smart_auto");
    haDoc["icon"] = "mdi:auto-fix";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/select/" + deviceId + "/mode/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Timer Duration Number =====
  {
    haDoc.clear();
    haDoc["name"] = "Timer Duration";
    haDoc["unique_id"] = deviceId + "_timer_duration";
    haDoc["state_topic"] = base + "/smartsensing/timer_duration";
    haDoc["command_topic"] = base + "/smartsensing/timer_duration/set";
    haDoc["min"] = 1;
    haDoc["max"] = 60;
    haDoc["step"] = 1;
    haDoc["unit_of_measurement"] = "min";
    haDoc["icon"] = "mdi:timer-outline";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/timer_duration/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Audio Threshold Number =====
  {
    haDoc.clear();
    haDoc["name"] = "Audio Threshold";
    haDoc["unique_id"] = deviceId + "_audio_threshold";
    haDoc["state_topic"] = base + "/smartsensing/audio_threshold";
    haDoc["command_topic"] = base + "/smartsensing/audio_threshold/set";
    haDoc["min"] = -96;
    haDoc["max"] = 0;
    haDoc["step"] = 1;
    haDoc["unit_of_measurement"] = "dBFS";
    haDoc["icon"] = "mdi:volume-vibrate";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/audio_threshold/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Audio Level Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "Audio Level";
    haDoc["unique_id"] = deviceId + "_audio_level";
    haDoc["state_topic"] = base + "/smartsensing/audio_level";
    haDoc["unit_of_measurement"] = "dBFS";
    haDoc["state_class"] = "measurement";
    haDoc["suggested_display_precision"] = 1;
    haDoc["icon"] = "mdi:volume-vibrate";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/audio_level/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Timer Remaining Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "Timer Remaining";
    haDoc["unique_id"] = deviceId + "_timer_remaining";
    haDoc["state_topic"] = base + "/smartsensing/timer_remaining";
    haDoc["unit_of_measurement"] = "s";
    haDoc["icon"] = "mdi:timer-sand";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/sensor/" + deviceId + "/timer_remaining/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== WiFi RSSI Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "WiFi Signal";
    haDoc["unique_id"] = deviceId + "_rssi";
    haDoc["state_topic"] = base + "/wifi/rssi";
    haDoc["unit_of_measurement"] = "dBm";
    haDoc["device_class"] = "signal_strength";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:wifi";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/rssi/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== WiFi Connected Binary Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "WiFi Connected";
    haDoc["unique_id"] = deviceId + "_wifi_connected";
    haDoc["state_topic"] = base + "/wifi/connected";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["device_class"] = "connectivity";
    haDoc["entity_category"] = "diagnostic";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/binary_sensor/" + deviceId + "/wifi_connected/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Detected Binary Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "Signal Detected";
    haDoc["unique_id"] = deviceId + "_signal_detected";
    haDoc["state_topic"] = base + "/smartsensing/signal_detected";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/binary_sensor/" + deviceId + "/signal_detected/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }


  // ===== Update Available Binary Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "Update Available";
    haDoc["unique_id"] = deviceId + "_update_available";
    haDoc["state_topic"] = base + "/system/update_available";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["device_class"] = "update";
    haDoc["entity_category"] = "diagnostic";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/binary_sensor/" + deviceId + "/update_available/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Firmware Version Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "Firmware Version";
    haDoc["unique_id"] = deviceId + "_firmware";
    haDoc["state_topic"] = base + "/system/firmware";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:tag";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/firmware/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Latest Firmware Version Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "Latest Firmware Version";
    haDoc["unique_id"] = deviceId + "_latest_firmware";
    haDoc["state_topic"] = base + "/system/latest_version";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:tag-arrow-up";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/sensor/" + deviceId + "/latest_firmware/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Reboot Button =====
  {
    haDoc.clear();
    haDoc["name"] = "Reboot";
    haDoc["unique_id"] = deviceId + "_reboot";
    haDoc["command_topic"] = base + "/system/reboot";
    haDoc["payload_press"] = "REBOOT";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:restart";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/button/" + deviceId + "/reboot/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Check Update Button =====
  {
    haDoc.clear();
    haDoc["name"] = "Check for Updates";
    haDoc["unique_id"] = deviceId + "_check_update";
    haDoc["command_topic"] = base + "/system/check_update";
    haDoc["payload_press"] = "CHECK";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:update";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/button/" + deviceId + "/check_update/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Factory Reset Button =====
  {
    haDoc.clear();
    haDoc["name"] = "Factory Reset";
    haDoc["unique_id"] = deviceId + "_factory_reset";
    haDoc["command_topic"] = base + "/system/factory_reset";
    haDoc["payload_press"] = "RESET";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:factory";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/button/" + deviceId + "/factory_reset/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Auto-Update Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Auto Update";
    haDoc["unique_id"] = deviceId + "_auto_update";
    haDoc["state_topic"] = base + "/settings/auto_update";
    haDoc["command_topic"] = base + "/settings/auto_update/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:update";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/auto_update/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== OTA Channel Select =====
  {
    haDoc.clear();
    haDoc["name"] = "Update Channel";
    haDoc["unique_id"] = deviceId + "_ota_channel";
    haDoc["state_topic"] = base + "/settings/ota_channel";
    haDoc["command_topic"] = base + "/settings/ota_channel/set";
    JsonArray options = haDoc["options"].to<JsonArray>();
    options.add("stable");
    options.add("beta");
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:tag-multiple";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/select/" + deviceId + "/ota_channel/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Firmware Update Entity =====
  // This provides the native HA Update entity with install capability
  {
    haDoc.clear();
    haDoc["name"] = "Firmware";
    haDoc["unique_id"] = deviceId + "_firmware_update";
    haDoc["device_class"] = "firmware";
    haDoc["state_topic"] = base + "/system/update/state";
    haDoc["command_topic"] = base + "/system/update/command";
    haDoc["payload_install"] = "install";
    haDoc["entity_picture"] =
        "https://brands.home-assistant.io/_/esphome/icon.png";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/update/" + deviceId + "/firmware/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== IP Address Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "IP Address";
    haDoc["unique_id"] = deviceId + "_ip";
    haDoc["state_topic"] = base + "/wifi/ip";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:ip-network";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/ip/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }


  // ===== Hardware Diagnostics =====

  // ===== CPU Temperature Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "CPU Temperature";
    haDoc["unique_id"] = deviceId + "_cpu_temp";
    haDoc["state_topic"] = base + "/hardware/temperature";
    haDoc["unit_of_measurement"] = "°C";
    haDoc["device_class"] = "temperature";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:thermometer";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/cpu_temp/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== CPU Usage Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "CPU Usage";
    haDoc["unique_id"] = deviceId + "_cpu_usage";
    haDoc["state_topic"] = base + "/hardware/cpu_usage";
    haDoc["unit_of_measurement"] = "%";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:cpu-64-bit";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/cpu_usage/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Free Heap Memory Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "Free Heap Memory";
    haDoc["unique_id"] = deviceId + "_heap_free";
    haDoc["state_topic"] = base + "/hardware/heap_free";
    haDoc["unit_of_measurement"] = "B";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:memory";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/heap_free/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Uptime Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "Uptime";
    haDoc["unique_id"] = deviceId + "_uptime";
    haDoc["state_topic"] = base + "/system/uptime";
    haDoc["unit_of_measurement"] = "s";
    haDoc["device_class"] = "duration";
    haDoc["state_class"] = "total_increasing";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:clock-outline";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/uptime/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== LittleFS Used Storage Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "LittleFS Used";
    haDoc["unique_id"] = deviceId + "_LittleFS_used";
    haDoc["state_topic"] = base + "/hardware/LittleFS_used";
    haDoc["unit_of_measurement"] = "B";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:harddisk";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/LittleFS_used/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== WiFi Channel Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "WiFi Channel";
    haDoc["unique_id"] = deviceId + "_wifi_channel";
    haDoc["state_topic"] = base + "/wifi/channel";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:wifi";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/wifi_channel/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Dark Mode Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Dark Mode";
    haDoc["unique_id"] = deviceId + "_dark_mode";
    haDoc["state_topic"] = base + "/settings/dark_mode";
    haDoc["command_topic"] = base + "/settings/dark_mode/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:weather-night";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/dark_mode/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Certificate Validation Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Certificate Validation";
    haDoc["unique_id"] = deviceId + "_cert_validation";
    haDoc["state_topic"] = base + "/settings/cert_validation";
    haDoc["command_topic"] = base + "/settings/cert_validation/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:certificate";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/switch/" + deviceId + "/cert_validation/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Display Backlight Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Display Backlight";
    haDoc["unique_id"] = deviceId + "_backlight";
    haDoc["state_topic"] = base + "/display/backlight";
    haDoc["command_topic"] = base + "/display/backlight/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["icon"] = "mdi:brightness-6";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/backlight/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Screen Timeout Number =====
  {
    haDoc.clear();
    haDoc["name"] = "Screen Timeout";
    haDoc["unique_id"] = deviceId + "_screen_timeout";
    haDoc["state_topic"] = base + "/settings/screen_timeout";
    haDoc["command_topic"] = base + "/settings/screen_timeout/set";
    haDoc["min"] = 0;
    haDoc["max"] = 600;
    haDoc["step"] = 30;
    haDoc["unit_of_measurement"] = "s";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:timer-off-outline";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/screen_timeout/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Dim Enabled Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Dim";
    haDoc["unique_id"] = deviceId + "_dim_enabled";
    haDoc["state_topic"] = base + "/display/dim_enabled";
    haDoc["command_topic"] = base + "/display/dim_enabled/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:brightness-auto";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/switch/" + deviceId + "/dim_enabled/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Dim Timeout Number =====
  {
    haDoc.clear();
    haDoc["name"] = "Dim Timeout";
    haDoc["unique_id"] = deviceId + "_dim_timeout";
    haDoc["state_topic"] = base + "/settings/dim_timeout";
    haDoc["command_topic"] = base + "/settings/dim_timeout/set";
    haDoc["min"] = 0;
    haDoc["max"] = 60;
    haDoc["step"] = 5;
    haDoc["unit_of_measurement"] = "s";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:brightness-auto";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/dim_timeout/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Display Brightness Number =====
  {
    haDoc.clear();
    haDoc["name"] = "Display Brightness";
    haDoc["unique_id"] = deviceId + "_brightness";
    haDoc["state_topic"] = base + "/display/brightness";
    haDoc["command_topic"] = base + "/display/brightness/set";
    haDoc["min"] = 10;
    haDoc["max"] = 100;
    haDoc["step"] = 25;
    haDoc["unit_of_measurement"] = "%";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:brightness-percent";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/brightness/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Dim Brightness Select =====
  {
    haDoc.clear();
    haDoc["name"] = "Dim Brightness";
    haDoc["unique_id"] = deviceId + "_dim_brightness";
    haDoc["state_topic"] = base + "/display/dim_brightness";
    haDoc["command_topic"] = base + "/display/dim_brightness/set";
    JsonArray options = haDoc["options"].to<JsonArray>();
    options.add("10");
    options.add("25");
    options.add("50");
    options.add("75");
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:brightness-4";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/select/" + deviceId + "/dim_brightness/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Buzzer Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Buzzer";
    haDoc["unique_id"] = deviceId + "_buzzer";
    haDoc["state_topic"] = base + "/settings/buzzer";
    haDoc["command_topic"] = base + "/settings/buzzer/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:volume-high";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/buzzer/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Buzzer Volume Number =====
  {
    haDoc.clear();
    haDoc["name"] = "Buzzer Volume";
    haDoc["unique_id"] = deviceId + "_buzzer_volume";
    haDoc["state_topic"] = base + "/settings/buzzer_volume";
    haDoc["command_topic"] = base + "/settings/buzzer_volume/set";
    haDoc["min"] = 0;
    haDoc["max"] = 2;
    haDoc["step"] = 1;
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:volume-medium";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/buzzer_volume/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }


  // ===== Audio Update Rate Select =====
  {
    haDoc.clear();
    haDoc["name"] = "Audio Update Rate";
    haDoc["unique_id"] = deviceId + "_audio_update_rate";
    haDoc["state_topic"] = base + "/settings/audio_update_rate";
    haDoc["command_topic"] = base + "/settings/audio_update_rate/set";
    JsonArray options = haDoc["options"].to<JsonArray>();
    options.add("20");
    options.add("33");
    options.add("50");
    options.add("100");
    haDoc["unit_of_measurement"] = "ms";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:update";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic =
        "homeassistant/select/" + deviceId + "/audio_update_rate/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Signal Generator";
    haDoc["unique_id"] = deviceId + "_siggen_enabled";
    haDoc["state_topic"] = base + "/signalgenerator/enabled";
    haDoc["command_topic"] = base + "/signalgenerator/enabled/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/siggen_enabled/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Waveform Select =====
  {
    haDoc.clear();
    haDoc["name"] = "Signal Waveform";
    haDoc["unique_id"] = deviceId + "_siggen_waveform";
    haDoc["state_topic"] = base + "/signalgenerator/waveform";
    haDoc["command_topic"] = base + "/signalgenerator/waveform/set";
    JsonArray options = haDoc["options"].to<JsonArray>();
    options.add("sine");
    options.add("square");
    options.add("white_noise");
    options.add("sweep");
    haDoc["icon"] = "mdi:waveform";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/select/" + deviceId + "/siggen_waveform/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Frequency Number =====
  {
    haDoc.clear();
    haDoc["name"] = "Signal Frequency";
    haDoc["unique_id"] = deviceId + "_siggen_frequency";
    haDoc["state_topic"] = base + "/signalgenerator/frequency";
    haDoc["command_topic"] = base + "/signalgenerator/frequency/set";
    haDoc["min"] = 1;
    haDoc["max"] = 22000;
    haDoc["unit_of_measurement"] = "Hz";
    haDoc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/number/" + deviceId + "/siggen_frequency/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Amplitude Number =====
  {
    haDoc.clear();
    haDoc["name"] = "Signal Amplitude";
    haDoc["unique_id"] = deviceId + "_siggen_amplitude";
    haDoc["state_topic"] = base + "/signalgenerator/amplitude";
    haDoc["command_topic"] = base + "/signalgenerator/amplitude/set";
    haDoc["min"] = -96;
    haDoc["max"] = 0;
    haDoc["step"] = 1;
    haDoc["unit_of_measurement"] = "dBFS";
    haDoc["icon"] = "mdi:volume-high";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/number/" + deviceId + "/siggen_amplitude/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Channel Select =====
  {
    haDoc.clear();
    haDoc["name"] = "Signal Channel";
    haDoc["unique_id"] = deviceId + "_siggen_channel";
    haDoc["state_topic"] = base + "/signalgenerator/channel";
    haDoc["command_topic"] = base + "/signalgenerator/channel/set";
    JsonArray options = haDoc["options"].to<JsonArray>();
    options.add("ch1");
    options.add("ch2");
    options.add("both");
    haDoc["icon"] = "mdi:speaker-multiple";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/select/" + deviceId + "/siggen_channel/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Output Mode Select =====
  {
    haDoc.clear();
    haDoc["name"] = "Signal Output Mode";
    haDoc["unique_id"] = deviceId + "_siggen_output_mode";
    haDoc["state_topic"] = base + "/signalgenerator/output_mode";
    haDoc["command_topic"] = base + "/signalgenerator/output_mode/set";
    JsonArray options = haDoc["options"].to<JsonArray>();
    options.add("software");
    options.add("pwm");
    haDoc["icon"] = "mdi:export";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/select/" + deviceId + "/siggen_output_mode/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Target ADC Select =====
  {
    haDoc.clear();
    haDoc["name"] = "Signal Target ADC";
    haDoc["unique_id"] = deviceId + "_siggen_target_adc";
    haDoc["state_topic"] = base + "/signalgenerator/target_adc";
    haDoc["command_topic"] = base + "/signalgenerator/target_adc/set";
    JsonArray options = haDoc["options"].to<JsonArray>();
    options.add("adc1");
    options.add("adc2");
    options.add("both");
    haDoc["icon"] = "mdi:audio-input-stereo-minijack";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/select/" + deviceId + "/siggen_target_adc/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }


  // ===== Per-ADC Audio Diagnostic Entities (only active inputs) =====
  // Topic labels are generated dynamically to support up to AUDIO_PIPELINE_MAX_INPUTS lanes.
  {
    int adcCount = appState.audio.activeInputCount;
    if (adcCount <= 0) adcCount = appState.audio.numAdcsDetected;
    if (adcCount > AUDIO_PIPELINE_MAX_INPUTS) adcCount = AUDIO_PIPELINE_MAX_INPUTS;
    for (int a = 0; a < adcCount; a++) {
      char labelBuf[8];
      char nameBuf[16];
      snprintf(labelBuf, sizeof(labelBuf), "adc%d", a + 1);
      snprintf(nameBuf, sizeof(nameBuf), "ADC %d", a + 1);
      String prefix = base + "/audio/" + labelBuf;
      String idSuffix = String("_") + labelBuf;

      // Per-ADC Level Sensor
      {
        haDoc.clear();
        haDoc["name"] = String(nameBuf) + " Audio Level";
        haDoc["unique_id"] = deviceId + idSuffix + "_level";
        haDoc["state_topic"] = prefix + "/level";
        haDoc["unit_of_measurement"] = "dBFS";
        haDoc["state_class"] = "measurement";
        haDoc["icon"] = "mdi:volume-high";
        addHADeviceInfo(haDoc);
        String payload;
        serializeJson(haDoc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + labelBuf + "_level/config").c_str(), payload.c_str(), true);
      }

      // Per-ADC Status Sensor
      {
        haDoc.clear();
        haDoc["name"] = String(nameBuf) + " ADC Status";
        haDoc["unique_id"] = deviceId + idSuffix + "_adc_status";
        haDoc["state_topic"] = prefix + "/adc_status";
        haDoc["entity_category"] = "diagnostic";
        haDoc["icon"] = "mdi:audio-input-stereo-minijack";
        addHADeviceInfo(haDoc);
        String payload;
        serializeJson(haDoc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + labelBuf + "_adc_status/config").c_str(), payload.c_str(), true);
      }

      // Per-ADC Noise Floor Sensor
      {
        haDoc.clear();
        haDoc["name"] = String(nameBuf) + " Noise Floor";
        haDoc["unique_id"] = deviceId + idSuffix + "_noise_floor";
        haDoc["state_topic"] = prefix + "/noise_floor";
        haDoc["unit_of_measurement"] = "dBFS";
        haDoc["state_class"] = "measurement";
        haDoc["entity_category"] = "diagnostic";
        haDoc["icon"] = "mdi:volume-low";
        addHADeviceInfo(haDoc);
        String payload;
        serializeJson(haDoc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + labelBuf + "_noise_floor/config").c_str(), payload.c_str(), true);
      }

      // Per-ADC Vrms Sensor
      {
        haDoc.clear();
        haDoc["name"] = String(nameBuf) + " Vrms";
        haDoc["unique_id"] = deviceId + idSuffix + "_vrms";
        haDoc["state_topic"] = prefix + "/vrms";
        haDoc["unit_of_measurement"] = "V";
        haDoc["device_class"] = "voltage";
        haDoc["state_class"] = "measurement";
        haDoc["entity_category"] = "diagnostic";
        haDoc["suggested_display_precision"] = 3;
        haDoc["icon"] = "mdi:sine-wave";
        addHADeviceInfo(haDoc);
        String payload;
        serializeJson(haDoc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + labelBuf + "_vrms/config").c_str(), payload.c_str(), true);
      }

      // SNR/SFDR discovery removed — debug-only data, accessible via REST/WS/GUI.
      // Cleanup of orphaned entities is handled in removeHADiscovery().
    }
  }

  // ===== Legacy Combined Audio ADC Status Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "ADC Status";
    haDoc["unique_id"] = deviceId + "_adc_status";
    haDoc["state_topic"] = base + "/audio/adc_status";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:audio-input-stereo-minijack";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/adc_status/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Legacy Combined Audio Noise Floor Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "Audio Noise Floor";
    haDoc["unique_id"] = deviceId + "_noise_floor";
    haDoc["state_topic"] = base + "/audio/noise_floor";
    haDoc["unit_of_measurement"] = "dBFS";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:volume-low";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/noise_floor/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Legacy Combined Input Voltage (Vrms) Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "Input Voltage (Vrms)";
    haDoc["unique_id"] = deviceId + "_input_vrms";
    haDoc["state_topic"] = base + "/audio/input_vrms";
    haDoc["unit_of_measurement"] = "V";
    haDoc["device_class"] = "voltage";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["suggested_display_precision"] = 3;
    haDoc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/input_vrms/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== ADC Reference Voltage Number =====
  {
    haDoc.clear();
    haDoc["name"] = "ADC Reference Voltage";
    haDoc["unique_id"] = deviceId + "_adc_vref";
    haDoc["state_topic"] = base + "/settings/adc_vref";
    haDoc["command_topic"] = base + "/settings/adc_vref/set";
    haDoc["min"] = 1.0;
    haDoc["max"] = 5.0;
    haDoc["step"] = 0.1;
    haDoc["unit_of_measurement"] = "V";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:flash-triangle-outline";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/number/" + deviceId + "/adc_vref/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Per-ADC Enable Switches =====
  {
    const char *adcNames[] = {"ADC Input 1", "ADC Input 2"};
    const char *adcIds[] = {"input1_enabled", "input2_enabled"};
    const char *adcTopics[] = {"/audio/input1/enabled", "/audio/input2/enabled"};
    for (int a = 0; a < 2; a++) {
      haDoc.clear();
      haDoc["name"] = adcNames[a];
      haDoc["unique_id"] = deviceId + "_" + adcIds[a];
      haDoc["state_topic"] = base + adcTopics[a];
      haDoc["command_topic"] = base + String(adcTopics[a]) + "/set";
      haDoc["payload_on"] = "ON";
      haDoc["payload_off"] = "OFF";
      haDoc["entity_category"] = "config";
      haDoc["icon"] = "mdi:audio-input-stereo-minijack";
      addHADeviceInfo(haDoc);
      String payload;
      serializeJson(haDoc, payload);
      String topic = "homeassistant/switch/" + deviceId + "/" + adcIds[a] + "/config";
      mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }
  }

  // ===== VU Meter Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "VU Meter";
    haDoc["unique_id"] = deviceId + "_vu_meter";
    haDoc["state_topic"] = base + "/audio/vu_meter";
    haDoc["command_topic"] = base + "/audio/vu_meter/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:chart-bar";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/vu_meter/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Audio Waveform Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Audio Waveform";
    haDoc["unique_id"] = deviceId + "_waveform";
    haDoc["state_topic"] = base + "/audio/waveform";
    haDoc["command_topic"] = base + "/audio/waveform/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:waveform";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/waveform/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Frequency Spectrum Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Frequency Spectrum";
    haDoc["unique_id"] = deviceId + "_spectrum";
    haDoc["state_topic"] = base + "/audio/spectrum";
    haDoc["command_topic"] = base + "/audio/spectrum/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:equalizer";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/spectrum/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== FFT Window Type Select =====
  {
    haDoc.clear();
    haDoc["name"] = "FFT Window";
    haDoc["unique_id"] = deviceId + "_fft_window";
    haDoc["state_topic"] = base + "/audio/fft_window";
    haDoc["command_topic"] = base + "/audio/fft_window/set";
    JsonArray options = haDoc["options"].to<JsonArray>();
    options.add("hann");
    options.add("blackman");
    options.add("blackman_harris");
    options.add("blackman_nuttall");
    options.add("nuttall");
    options.add("flat_top");
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:window-shutter-settings";
    addHADeviceInfo(haDoc);

    String payload;
    serializeJson(haDoc, payload);
    String topic = "homeassistant/select/" + deviceId + "/fft_window/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }


  // ===== Debug Mode Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Debug Mode";
    haDoc["unique_id"] = deviceId + "_debug_mode";
    haDoc["state_topic"] = base + "/debug/mode";
    haDoc["command_topic"] = base + "/debug/mode/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:bug";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/debug_mode/config").c_str(), payload.c_str(), true);
  }

  // ===== Debug Serial Level Number =====
  {
    haDoc.clear();
    haDoc["name"] = "Debug Serial Level";
    haDoc["unique_id"] = deviceId + "_debug_serial_level";
    haDoc["state_topic"] = base + "/debug/serial_level";
    haDoc["command_topic"] = base + "/debug/serial_level/set";
    haDoc["min"] = 0;
    haDoc["max"] = 3;
    haDoc["step"] = 1;
    haDoc["mode"] = "slider";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:console";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/number/" + deviceId + "/debug_serial_level/config").c_str(), payload.c_str(), true);
  }

  // ===== Debug HW Stats Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Debug HW Stats";
    haDoc["unique_id"] = deviceId + "_debug_hw_stats";
    haDoc["state_topic"] = base + "/debug/hw_stats";
    haDoc["command_topic"] = base + "/debug/hw_stats/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:chart-line";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/debug_hw_stats/config").c_str(), payload.c_str(), true);
  }

  // ===== Debug I2S Metrics Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Debug I2S Metrics";
    haDoc["unique_id"] = deviceId + "_debug_i2s_metrics";
    haDoc["state_topic"] = base + "/debug/i2s_metrics";
    haDoc["command_topic"] = base + "/debug/i2s_metrics/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:timer-outline";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/debug_i2s_metrics/config").c_str(), payload.c_str(), true);
  }

  // ===== Debug Task Monitor Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Debug Task Monitor";
    haDoc["unique_id"] = deviceId + "_debug_task_monitor";
    haDoc["state_topic"] = base + "/debug/task_monitor";
    haDoc["command_topic"] = base + "/debug/task_monitor/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:format-list-bulleted";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/debug_task_monitor/config").c_str(), payload.c_str(), true);
  }

  // ===== Task Monitor Diagnostic Sensors =====
  {
    haDoc.clear();
    haDoc["name"] = "Task Count";
    haDoc["unique_id"] = deviceId + "_task_count";
    haDoc["state_topic"] = base + "/hardware/task_count";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:format-list-numbered";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/task_count/config").c_str(), payload.c_str(), true);
  }
  {
    haDoc.clear();
    haDoc["name"] = "Loop Time";
    haDoc["unique_id"] = deviceId + "_loop_time";
    haDoc["state_topic"] = base + "/hardware/loop_time_us";
    haDoc["unit_of_measurement"] = "us";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:timer-outline";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/loop_time/config").c_str(), payload.c_str(), true);
  }
  {
    haDoc.clear();
    haDoc["name"] = "Loop Time Max";
    haDoc["unique_id"] = deviceId + "_loop_time_max";
    haDoc["state_topic"] = base + "/hardware/loop_time_max_us";
    haDoc["unit_of_measurement"] = "us";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:timer-alert-outline";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/loop_time_max/config").c_str(), payload.c_str(), true);
  }
  {
    haDoc.clear();
    haDoc["name"] = "Min Stack Free";
    haDoc["unique_id"] = deviceId + "_min_stack_free";
    haDoc["state_topic"] = base + "/hardware/min_stack_free";
    haDoc["unit_of_measurement"] = "B";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:memory";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/min_stack_free/config").c_str(), payload.c_str(), true);
  }

  // ===== Crash Diagnostics — Reset Reason (diagnostic sensor) =====
  {
    haDoc.clear();
    haDoc["name"] = "Reset Reason";
    haDoc["unique_id"] = deviceId + "_reset_reason";
    haDoc["state_topic"] = base + "/diagnostics/reset_reason";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:restart-alert";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/reset_reason/config").c_str(), payload.c_str(), true);
  }

  // ===== Crash Diagnostics — Was Crash (binary sensor) =====
  {
    haDoc.clear();
    haDoc["name"] = "Last Boot Was Crash";
    haDoc["unique_id"] = deviceId + "_was_crash";
    haDoc["state_topic"] = base + "/diagnostics/was_crash";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["device_class"] = "problem";
    haDoc["entity_category"] = "diagnostic";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/was_crash/config").c_str(), payload.c_str(), true);
  }

  // ===== Heap Health — Heap Critical (binary sensor) =====
  {
    haDoc.clear();
    haDoc["name"] = "Heap Critical";
    haDoc["unique_id"] = deviceId + "_heap_critical";
    haDoc["state_topic"] = base + "/diagnostics/heap_critical";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["device_class"] = "problem";
    haDoc["entity_category"] = "diagnostic";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/heap_critical/config").c_str(), payload.c_str(), true);
  }

  // ===== Audio DMA Alloc Failed (binary sensor) =====
  {
    haDoc.clear();
    haDoc["name"] = "Audio DMA Alloc Failed";
    haDoc["unique_id"] = deviceId + "_dma_alloc_failed";
    haDoc["state_topic"] = base + "/diagnostics/dma_alloc_failed";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["device_class"] = "problem";
    haDoc["entity_category"] = "diagnostic";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/dma_alloc_failed/config").c_str(), payload.c_str(), true);
  }

  // ===== Heap Health — Max Alloc Block (diagnostic sensor) =====
  {
    haDoc.clear();
    haDoc["name"] = "Heap Max Block";
    haDoc["unique_id"] = deviceId + "_heap_max_block";
    haDoc["state_topic"] = base + "/diagnostics/heap_max_block";
    haDoc["unit_of_measurement"] = "B";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:memory";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/heap_max_block/config").c_str(), payload.c_str(), true);
  }

  // ===== Timezone Offset Number =====
  {
    haDoc.clear();
    haDoc["name"] = "Timezone Offset";
    haDoc["unique_id"] = deviceId + "_timezone_offset";
    haDoc["state_topic"] = base + "/settings/timezone_offset";
    haDoc["command_topic"] = base + "/settings/timezone_offset/set";
    haDoc["min"] = -12;
    haDoc["max"] = 14;
    haDoc["step"] = 1;
    haDoc["unit_of_measurement"] = "h";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:map-clock-outline";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/number/" + deviceId + "/timezone_offset/config").c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Sweep Speed Number =====
  {
    haDoc.clear();
    haDoc["name"] = "Signal Sweep Speed";
    haDoc["unique_id"] = deviceId + "_siggen_sweep_speed";
    haDoc["state_topic"] = base + "/signalgenerator/sweep_speed";
    haDoc["command_topic"] = base + "/signalgenerator/sweep_speed/set";
    haDoc["min"] = 0.1;
    haDoc["max"] = 10.0;
    haDoc["step"] = 0.1;
    haDoc["unit_of_measurement"] = "Hz/s";
    haDoc["icon"] = "mdi:speedometer";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/number/" + deviceId + "/siggen_sweep_speed/config").c_str(), payload.c_str(), true);
  }

  // ===== Input Names (4 read-only sensors) =====
  {
    const char *inputLabels[] = {
        "input1_name_l", "input1_name_r", "input2_name_l", "input2_name_r",
        "input3_name_l", "input3_name_r", "input4_name_l", "input4_name_r",
        "input5_name_l", "input5_name_r", "input6_name_l", "input6_name_r",
        "input7_name_l", "input7_name_r", "input8_name_l", "input8_name_r"};
    const char *inputDisplayNames[] = {
        "Input 1 Left Name", "Input 1 Right Name", "Input 2 Left Name", "Input 2 Right Name",
        "Input 3 Left Name", "Input 3 Right Name", "Input 4 Left Name", "Input 4 Right Name",
        "Input 5 Left Name", "Input 5 Right Name", "Input 6 Left Name", "Input 6 Right Name",
        "Input 7 Left Name", "Input 7 Right Name", "Input 8 Left Name", "Input 8 Right Name"};
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {
      haDoc.clear();
      haDoc["name"] = inputDisplayNames[i];
      haDoc["unique_id"] = deviceId + "_" + inputLabels[i];
      haDoc["state_topic"] = base + "/audio/" + inputLabels[i];
      haDoc["entity_category"] = "diagnostic";
      haDoc["icon"] = "mdi:label-outline";
      addHADeviceInfo(haDoc);
      String payload;
      serializeJson(haDoc, payload);
      mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + inputLabels[i] + "/config").c_str(), payload.c_str(), true);
    }
  }


#ifdef DSP_ENABLED
  // ===== DSP Enabled Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "DSP";
    haDoc["unique_id"] = deviceId + "_dsp_enabled";
    haDoc["state_topic"] = base + "/dsp/enabled";
    haDoc["command_topic"] = base + "/dsp/enabled/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["icon"] = "mdi:equalizer";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/dsp_enabled/config").c_str(), payload.c_str(), true);
  }

  // ===== DSP Bypass Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "DSP Bypass";
    haDoc["unique_id"] = deviceId + "_dsp_bypass";
    haDoc["state_topic"] = base + "/dsp/bypass";
    haDoc["command_topic"] = base + "/dsp/bypass/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["icon"] = "mdi:debug-step-over";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/dsp_bypass/config").c_str(), payload.c_str(), true);
  }

  // ===== DSP CPU Load Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "DSP CPU Load";
    haDoc["unique_id"] = deviceId + "_dsp_cpu_load";
    haDoc["state_topic"] = base + "/dsp/cpu_load";
    haDoc["unit_of_measurement"] = "%";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:cpu-64-bit";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/dsp_cpu_load/config").c_str(), payload.c_str(), true);
  }

  // ===== DSP Preset Select =====
  {
    haDoc.clear();
    haDoc["name"] = "DSP Preset";
    haDoc["unique_id"] = deviceId + "_dsp_preset";
    haDoc["state_topic"] = base + "/dsp/preset";
    haDoc["command_topic"] = base + "/dsp/preset/set";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:playlist-music";
    JsonArray opts = haDoc["options"].to<JsonArray>();
    opts.add("Custom");
    extern bool dsp_preset_exists(int);
    for (int i = 0; i < DSP_PRESET_MAX_SLOTS; i++) {
      if (appState.dsp.presetNames[i][0] && dsp_preset_exists(i)) {
        opts.add(appState.dsp.presetNames[i]);
      }
    }
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/select/" + deviceId + "/dsp_preset/config").c_str(), payload.c_str(), true);
  }

  // ===== Per-Channel DSP Entities =====
  {
    const char *chNames[] = {"L1", "R1", "L2", "R2"};
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
      String chPrefix = base + "/dsp/channel_" + String(ch);
      String idSuffix = String("_dsp_ch") + String(ch);

      // Per-channel bypass switch
      {
        haDoc.clear();
        haDoc["name"] = String("DSP ") + chNames[ch] + " Bypass";
        haDoc["unique_id"] = deviceId + idSuffix + "_bypass";
        haDoc["state_topic"] = chPrefix + "/bypass";
        haDoc["command_topic"] = chPrefix + "/bypass/set";
        haDoc["payload_on"] = "ON";
        haDoc["payload_off"] = "OFF";
        haDoc["entity_category"] = "config";
        haDoc["icon"] = "mdi:debug-step-over";
        addHADeviceInfo(haDoc);
        String payload;
        serializeJson(haDoc, payload);
        mqttClient.publish(("homeassistant/switch/" + deviceId + "/dsp_ch" + String(ch) + "_bypass/config").c_str(), payload.c_str(), true);
      }

      // Per-channel stage count sensor
      {
        haDoc.clear();
        haDoc["name"] = String("DSP ") + chNames[ch] + " Stages";
        haDoc["unique_id"] = deviceId + idSuffix + "_stages";
        haDoc["state_topic"] = chPrefix + "/stage_count";
        haDoc["state_class"] = "measurement";
        haDoc["entity_category"] = "diagnostic";
        haDoc["icon"] = "mdi:filter";
        addHADeviceInfo(haDoc);
        String payload;
        serializeJson(haDoc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/dsp_ch" + String(ch) + "_stages/config").c_str(), payload.c_str(), true);
      }

      // Per-channel limiter gain reduction sensor
      {
        haDoc.clear();
        haDoc["name"] = String("DSP ") + chNames[ch] + " Limiter GR";
        haDoc["unique_id"] = deviceId + idSuffix + "_limiter_gr";
        haDoc["state_topic"] = chPrefix + "/limiter_gr";
        haDoc["unit_of_measurement"] = "dB";
        haDoc["state_class"] = "measurement";
        haDoc["entity_category"] = "diagnostic";
        haDoc["icon"] = "mdi:arrow-collapse-down";
        addHADeviceInfo(haDoc);
        String payload;
        serializeJson(haDoc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/dsp_ch" + String(ch) + "_limiter_gr/config").c_str(), payload.c_str(), true);
      }
    }
  }

  // ===== PEQ Bypass Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "PEQ Bypass";
    haDoc["unique_id"] = deviceId + "_peq_bypass";
    haDoc["state_topic"] = base + "/dsp/peq/bypass";
    haDoc["command_topic"] = base + "/dsp/peq/bypass/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:equalizer";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/peq_bypass/config").c_str(), payload.c_str(), true);
  }

  // PEQ band switches removed — controlled via DSP API / WebSocket only.
  // Cleanup of orphaned PEQ band entities is handled in removeHADiscovery().
#endif

#ifdef GUI_ENABLED
  // ===== Boot Animation Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "Boot Animation";
    haDoc["unique_id"] = deviceId + "_boot_animation";
    haDoc["state_topic"] = base + "/settings/boot_animation";
    haDoc["command_topic"] = base + "/settings/boot_animation/set";
    haDoc["payload_on"] = "ON";
    haDoc["payload_off"] = "OFF";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:animation-play";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/boot_animation/config").c_str(), payload.c_str(), true);
  }

  // ===== Boot Animation Style Select =====
  {
    haDoc.clear();
    haDoc["name"] = "Boot Animation Style";
    haDoc["unique_id"] = deviceId + "_boot_animation_style";
    haDoc["state_topic"] = base + "/settings/boot_animation_style";
    haDoc["command_topic"] = base + "/settings/boot_animation_style/set";
    JsonArray options = haDoc["options"].to<JsonArray>();
    options.add("wave_pulse");
    options.add("speaker_ripple");
    options.add("waveform");
    options.add("beat_bounce");
    options.add("freq_bars");
    options.add("heartbeat");
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:animation";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/select/" + deviceId + "/boot_animation_style/config").c_str(), payload.c_str(), true);
  }
#endif

#ifdef USB_AUDIO_ENABLED
  // ===== USB Audio Connected Binary Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "USB Connected";
    haDoc["unique_id"] = deviceId + "_usb_audio_connected";
    haDoc["state_topic"] = base + "/" + MQTT_TOPIC_USB_CONNECTED;
    haDoc["payload_on"] = "true";
    haDoc["payload_off"] = "false";
    haDoc["device_class"] = "connectivity";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:usb";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/usb_audio_connected/config").c_str(), payload.c_str(), true);
  }

  // ===== USB Audio Streaming Binary Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "USB Streaming";
    haDoc["unique_id"] = deviceId + "_usb_audio_streaming";
    haDoc["state_topic"] = base + "/" + MQTT_TOPIC_USB_STREAMING;
    haDoc["payload_on"] = "true";
    haDoc["payload_off"] = "false";
    haDoc["device_class"] = "running";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:music";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/usb_audio_streaming/config").c_str(), payload.c_str(), true);
  }

  // ===== USB Audio Enabled Switch =====
  {
    haDoc.clear();
    haDoc["name"] = "USB Audio";
    haDoc["unique_id"] = deviceId + "_usb_audio_enabled";
    haDoc["state_topic"] = base + "/" + MQTT_TOPIC_USB_ENABLED;
    haDoc["command_topic"] = base + "/" + MQTT_TOPIC_USB_ENABLE_SET;
    haDoc["payload_on"] = "true";
    haDoc["payload_off"] = "false";
    haDoc["entity_category"] = "config";
    haDoc["icon"] = "mdi:usb-port";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/usb_audio_enabled/config").c_str(), payload.c_str(), true);
  }

  // ===== USB Audio Sample Rate Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "USB Sample Rate";
    haDoc["unique_id"] = deviceId + "_usb_audio_sample_rate";
    haDoc["state_topic"] = base + "/" + MQTT_TOPIC_USB_RATE;
    haDoc["unit_of_measurement"] = "Hz";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/usb_audio_sample_rate/config").c_str(), payload.c_str(), true);
  }

  // ===== USB Audio Volume Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "USB Volume";
    haDoc["unique_id"] = deviceId + "_usb_audio_volume";
    haDoc["state_topic"] = base + "/" + MQTT_TOPIC_USB_VOLUME;
    haDoc["unit_of_measurement"] = "dB";
    haDoc["state_class"] = "measurement";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:volume-high";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/usb_audio_volume/config").c_str(), payload.c_str(), true);
  }

  // ===== USB Audio Buffer Overruns Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "USB Buffer Overruns";
    haDoc["unique_id"] = deviceId + "_usb_audio_overruns";
    haDoc["state_topic"] = base + "/" + MQTT_TOPIC_USB_OVERRUNS;
    haDoc["state_class"] = "total_increasing";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:alert-circle-outline";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/usb_audio_overruns/config").c_str(), payload.c_str(), true);
  }

  // ===== USB Audio Buffer Underruns Sensor =====
  {
    haDoc.clear();
    haDoc["name"] = "USB Buffer Underruns";
    haDoc["unique_id"] = deviceId + "_usb_audio_underruns";
    haDoc["state_topic"] = base + "/" + MQTT_TOPIC_USB_UNDERRUNS;
    haDoc["state_class"] = "total_increasing";
    haDoc["entity_category"] = "diagnostic";
    haDoc["icon"] = "mdi:alert-outline";
    addHADeviceInfo(haDoc);
    String payload;
    serializeJson(haDoc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/usb_audio_underruns/config").c_str(), payload.c_str(), true);
  }
#endif

  // ===== Generic HAL device entities (auto-discovery for new mezzanine devices) =====
  // Publishes a basic availability binary_sensor for HAL devices that do not have
  // dedicated HA entities above, so new expansion modules are visible in HA without
  // per-device hand-coding in this file.
  {
    struct HalDiscoveryCtx {
      const String* base;
      const String* deviceId;
      PubSubClient* client;
    };
    HalDiscoveryCtx ctx { &base, &deviceId, &mqttClient };

    HalDeviceManager::instance().forEach([](HalDevice* dev, void* raw) {
      if (!dev || dev->_state == HAL_STATE_REMOVED) return;

      // Skip devices that already have dedicated HA entities above, and
      // internal/utility devices that should not appear as HA entities.
      const char* compat = dev->getDescriptor().compatible;
      if (strcmp(compat, "ti,pcm5102a") == 0 ||
          strcmp(compat, "everest-semi,es8311") == 0 ||
          strcmp(compat, "generic,relay-amp") == 0 ||
          strcmp(compat, "generic,piezo-buzzer") == 0 ||
          strcmp(compat, "alx,signal-gen") == 0 ||
          strcmp(compat, "alx,usb-audio") == 0 ||
          strcmp(compat, "generic,status-led") == 0 ||
          strcmp(compat, "alps,ec11") == 0 ||
          strcmp(compat, "generic,tact-switch") == 0 ||
          strcmp(compat, "sitronix,st7735s") == 0) return;

      auto* c = static_cast<HalDiscoveryCtx*>(raw);
      const String& base      = *c->base;
      const String& deviceId  = *c->deviceId;
      PubSubClient& client    = *c->client;

      // Publish binary_sensor for device availability
      {
        haDoc.clear();
        String uid = deviceId + "_hal_" + String(dev->getSlot());
        haDoc["unique_id"]    = uid + "_available";
        haDoc["name"]         = String(dev->getDescriptor().name) + " Available";
        haDoc["state_topic"]  = base + "/hal/" + String(dev->getSlot()) + "/available";
        haDoc["payload_on"]   = "true";
        haDoc["payload_off"]  = "false";
        haDoc["device_class"] = "connectivity";
        haDoc["entity_category"] = "diagnostic";
        addHADeviceInfo(haDoc);

        String payload;
        serializeJson(haDoc, payload);
        String topic = "homeassistant/binary_sensor/" + uid + "_available/config";
        client.publish(topic.c_str(), payload.c_str(), true);
      }
    }, static_cast<void*>(&ctx));
  }

  LOG_I("[MQTT] Home Assistant discovery configs published");
}

// Remove Home Assistant auto-discovery configuration
void removeHADiscovery() {
  if (!mqttClient.connected())
    return;

  LOG_I("[MQTT] Removing Home Assistant discovery configs...");

  String deviceId = getMqttDeviceId();

  // List of all discovery topics to remove
  const char *topics[] = {
      "homeassistant/switch/%s/amplifier/config",
      "homeassistant/switch/%s/ap/config",
      "homeassistant/switch/%s/auto_update/config",
      "homeassistant/select/%s/ota_channel/config",
      "homeassistant/switch/%s/dark_mode/config",
      "homeassistant/switch/%s/cert_validation/config",
      "homeassistant/select/%s/mode/config",
      "homeassistant/number/%s/timer_duration/config",
      "homeassistant/number/%s/audio_threshold/config",
      "homeassistant/sensor/%s/audio_level/config",
      "homeassistant/sensor/%s/timer_remaining/config",
      "homeassistant/sensor/%s/rssi/config",
      "homeassistant/sensor/%s/firmware/config",
      "homeassistant/sensor/%s/latest_firmware/config",
      "homeassistant/sensor/%s/ip/config",
      "homeassistant/sensor/%s/cpu_temp/config",
      "homeassistant/sensor/%s/cpu_usage/config",
      "homeassistant/sensor/%s/heap_free/config",
      "homeassistant/sensor/%s/uptime/config",
      "homeassistant/sensor/%s/LittleFS_used/config",
      "homeassistant/sensor/%s/wifi_channel/config",
      "homeassistant/binary_sensor/%s/wifi_connected/config",
      "homeassistant/binary_sensor/%s/signal_detected/config",
      "homeassistant/binary_sensor/%s/update_available/config",
      "homeassistant/button/%s/reboot/config",
      "homeassistant/button/%s/check_update/config",
      "homeassistant/update/%s/firmware/config",
      "homeassistant/switch/%s/backlight/config",
      "homeassistant/number/%s/screen_timeout/config",
      "homeassistant/switch/%s/dim_enabled/config",
      "homeassistant/number/%s/dim_timeout/config",
      "homeassistant/number/%s/brightness/config",
      "homeassistant/select/%s/dim_brightness/config",
      "homeassistant/switch/%s/buzzer/config",
      "homeassistant/number/%s/buzzer_volume/config",
      "homeassistant/switch/%s/siggen_enabled/config",
      "homeassistant/select/%s/siggen_waveform/config",
      "homeassistant/number/%s/siggen_frequency/config",
      "homeassistant/number/%s/siggen_amplitude/config",
      "homeassistant/select/%s/siggen_channel/config",
      "homeassistant/select/%s/siggen_output_mode/config",
      "homeassistant/select/%s/audio_update_rate/config",
      "homeassistant/sensor/%s/adc_status/config",
      "homeassistant/sensor/%s/noise_floor/config",
      "homeassistant/sensor/%s/input_vrms/config",
      "homeassistant/number/%s/adc_vref/config",
      "homeassistant/switch/%s/input1_enabled/config",
      "homeassistant/switch/%s/input2_enabled/config",
      "homeassistant/switch/%s/vu_meter/config",
      "homeassistant/switch/%s/waveform/config",
      "homeassistant/switch/%s/spectrum/config",
      "homeassistant/sensor/%s/task_count/config",
      "homeassistant/sensor/%s/loop_time/config",
      "homeassistant/sensor/%s/loop_time_max/config",
      "homeassistant/sensor/%s/min_stack_free/config",
      "homeassistant/switch/%s/debug_mode/config",
      "homeassistant/number/%s/debug_serial_level/config",
      "homeassistant/switch/%s/debug_hw_stats/config",
      "homeassistant/switch/%s/debug_i2s_metrics/config",
      "homeassistant/switch/%s/debug_task_monitor/config",
      // Per-ADC entities
      "homeassistant/sensor/%s/adc1_level/config",
      "homeassistant/sensor/%s/adc1_adc_status/config",
      "homeassistant/sensor/%s/adc1_noise_floor/config",
      "homeassistant/sensor/%s/adc1_vrms/config",
      "homeassistant/sensor/%s/adc2_level/config",
      "homeassistant/sensor/%s/adc2_adc_status/config",
      "homeassistant/sensor/%s/adc2_noise_floor/config",
      "homeassistant/sensor/%s/adc2_vrms/config",
      "homeassistant/sensor/%s/adc1_snr/config",
      "homeassistant/sensor/%s/adc1_sfdr/config",
      "homeassistant/sensor/%s/adc2_snr/config",
      "homeassistant/sensor/%s/adc2_sfdr/config",
      "homeassistant/select/%s/fft_window/config",
      // Signal generator target ADC
      "homeassistant/select/%s/siggen_target_adc/config",
      // Crash diagnostics
      "homeassistant/sensor/%s/reset_reason/config",
      "homeassistant/binary_sensor/%s/was_crash/config",
      "homeassistant/binary_sensor/%s/heap_critical/config",
      "homeassistant/binary_sensor/%s/dma_alloc_failed/config",
      "homeassistant/sensor/%s/heap_max_block/config",
      // Factory reset button
      "homeassistant/button/%s/factory_reset/config",
      // Timezone offset
      "homeassistant/number/%s/timezone_offset/config",
      // Sweep speed
      "homeassistant/number/%s/siggen_sweep_speed/config",
      // Input names
      "homeassistant/sensor/%s/input1_name_l/config",
      "homeassistant/sensor/%s/input1_name_r/config",
      "homeassistant/sensor/%s/input2_name_l/config",
      "homeassistant/sensor/%s/input2_name_r/config",
      // Boot animation
      "homeassistant/switch/%s/boot_animation/config",
      "homeassistant/select/%s/boot_animation_style/config",
      // DSP entities
      "homeassistant/switch/%s/dsp_enabled/config",
      "homeassistant/switch/%s/dsp_bypass/config",
      "homeassistant/sensor/%s/dsp_cpu_load/config",
      "homeassistant/switch/%s/dsp_ch0_bypass/config",
      "homeassistant/switch/%s/dsp_ch1_bypass/config",
      "homeassistant/switch/%s/dsp_ch2_bypass/config",
      "homeassistant/switch/%s/dsp_ch3_bypass/config",
      "homeassistant/sensor/%s/dsp_ch0_stages/config",
      "homeassistant/sensor/%s/dsp_ch1_stages/config",
      "homeassistant/sensor/%s/dsp_ch2_stages/config",
      "homeassistant/sensor/%s/dsp_ch3_stages/config",
      "homeassistant/sensor/%s/dsp_ch0_limiter_gr/config",
      "homeassistant/sensor/%s/dsp_ch1_limiter_gr/config",
      "homeassistant/sensor/%s/dsp_ch2_limiter_gr/config",
      "homeassistant/sensor/%s/dsp_ch3_limiter_gr/config",
      // PEQ bypass
      "homeassistant/switch/%s/peq_bypass/config",
      // USB audio entities
      "homeassistant/binary_sensor/%s/usb_audio_connected/config",
      "homeassistant/binary_sensor/%s/usb_audio_streaming/config",
      "homeassistant/switch/%s/usb_audio_enabled/config",
      "homeassistant/sensor/%s/usb_audio_sample_rate/config",
      "homeassistant/sensor/%s/usb_audio_volume/config",
      "homeassistant/sensor/%s/usb_audio_overruns/config",
      "homeassistant/sensor/%s/usb_audio_underruns/config"};

  char topicBuf[160];
  for (const char *topicTemplate : topics) {
    snprintf(topicBuf, sizeof(topicBuf), topicTemplate, deviceId.c_str());
    mqttClient.publish(topicBuf, "", true); // Empty payload removes the config
  }

  // Remove orphaned PEQ band switch entities (20 = 2 channels x 10 bands)
  for (int ch = 0; ch < 2; ch++) {
    for (int b = 0; b < DSP_PEQ_BANDS; b++) {
      String topic = "homeassistant/switch/" + deviceId + "/peq_ch" + String(ch) + "_band" + String(b + 1) + "/config";
      mqttClient.publish(topic.c_str(), "", true);
    }
  }

  // Remove dynamic HAL device entities published by publishHADiscovery().
  // These use per-slot topics and are not in the static array above.
  {
    struct RemoveCtx {
      const String* deviceId;
      PubSubClient* client;
    };
    RemoveCtx rctx { &deviceId, &mqttClient };

    HalDeviceManager::instance().forEach([](HalDevice* dev, void* raw) {
      if (!dev) return;

      // Mirror the skip list from publishHADiscovery() so we only remove
      // topics that were actually published.
      const char* compat = dev->getDescriptor().compatible;
      if (strcmp(compat, "ti,pcm5102a") == 0 ||
          strcmp(compat, "everest-semi,es8311") == 0 ||
          strcmp(compat, "generic,relay-amp") == 0 ||
          strcmp(compat, "generic,piezo-buzzer") == 0 ||
          strcmp(compat, "alx,signal-gen") == 0 ||
          strcmp(compat, "alx,usb-audio") == 0 ||
          strcmp(compat, "generic,status-led") == 0 ||
          strcmp(compat, "alps,ec11") == 0 ||
          strcmp(compat, "generic,tact-switch") == 0 ||
          strcmp(compat, "sitronix,st7735s") == 0) return;

      auto* c = static_cast<RemoveCtx*>(raw);
      String uid = *c->deviceId + "_hal_" + String(dev->getSlot());
      String topic = "homeassistant/binary_sensor/" + uid + "_available/config";
      c->client->publish(topic.c_str(), "", true); // Empty payload removes the config
    }, static_cast<void*>(&rctx));
  }

  LOG_I("[MQTT] Home Assistant discovery configs removed");
}
