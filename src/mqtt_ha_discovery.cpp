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
  if (!mqttClient.connected() || !appState.mqtt.haDiscovery)
    return;

  LOG_I("[MQTT] Publishing Home Assistant discovery configs...");

  String deviceId = getMqttDeviceId();
  String base = getEffectiveMqttBaseTopic();

  // ===== Amplifier Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Amplifier";
    doc["unique_id"] = deviceId + "_amplifier";
    doc["state_topic"] = base + "/smartsensing/amplifier";
    doc["command_topic"] = base + "/smartsensing/amplifier/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:amplifier";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/amplifier/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== AP Mode Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Access Point";
    doc["unique_id"] = deviceId + "_ap";
    doc["state_topic"] = base + "/ap/enabled";
    doc["command_topic"] = base + "/ap/enabled/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:access-point";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/ap/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Smart Sensing Mode Select =====
  {
    JsonDocument doc;
    doc["name"] = "Smart Sensing Mode";
    doc["unique_id"] = deviceId + "_mode";
    doc["state_topic"] = base + "/smartsensing/mode";
    doc["command_topic"] = base + "/smartsensing/mode/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("always_on");
    options.add("always_off");
    options.add("smart_auto");
    doc["icon"] = "mdi:auto-fix";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/select/" + deviceId + "/mode/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Timer Duration Number =====
  {
    JsonDocument doc;
    doc["name"] = "Timer Duration";
    doc["unique_id"] = deviceId + "_timer_duration";
    doc["state_topic"] = base + "/smartsensing/timer_duration";
    doc["command_topic"] = base + "/smartsensing/timer_duration/set";
    doc["min"] = 1;
    doc["max"] = 60;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "min";
    doc["icon"] = "mdi:timer-outline";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/timer_duration/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Audio Threshold Number =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Threshold";
    doc["unique_id"] = deviceId + "_audio_threshold";
    doc["state_topic"] = base + "/smartsensing/audio_threshold";
    doc["command_topic"] = base + "/smartsensing/audio_threshold/set";
    doc["min"] = -96;
    doc["max"] = 0;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "dBFS";
    doc["icon"] = "mdi:volume-vibrate";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/audio_threshold/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Audio Level Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Level";
    doc["unique_id"] = deviceId + "_audio_level";
    doc["state_topic"] = base + "/smartsensing/audio_level";
    doc["unit_of_measurement"] = "dBFS";
    doc["state_class"] = "measurement";
    doc["suggested_display_precision"] = 1;
    doc["icon"] = "mdi:volume-vibrate";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/audio_level/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Timer Remaining Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Timer Remaining";
    doc["unique_id"] = deviceId + "_timer_remaining";
    doc["state_topic"] = base + "/smartsensing/timer_remaining";
    doc["unit_of_measurement"] = "s";
    doc["icon"] = "mdi:timer-sand";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/sensor/" + deviceId + "/timer_remaining/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== WiFi RSSI Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "WiFi Signal";
    doc["unique_id"] = deviceId + "_rssi";
    doc["state_topic"] = base + "/wifi/rssi";
    doc["unit_of_measurement"] = "dBm";
    doc["device_class"] = "signal_strength";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:wifi";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/rssi/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== WiFi Connected Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "WiFi Connected";
    doc["unique_id"] = deviceId + "_wifi_connected";
    doc["state_topic"] = base + "/wifi/connected";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "connectivity";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/binary_sensor/" + deviceId + "/wifi_connected/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Detected Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Detected";
    doc["unique_id"] = deviceId + "_signal_detected";
    doc["state_topic"] = base + "/smartsensing/signal_detected";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/binary_sensor/" + deviceId + "/signal_detected/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }


  // ===== Update Available Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Update Available";
    doc["unique_id"] = deviceId + "_update_available";
    doc["state_topic"] = base + "/system/update_available";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "update";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/binary_sensor/" + deviceId + "/update_available/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Firmware Version Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Firmware Version";
    doc["unique_id"] = deviceId + "_firmware";
    doc["state_topic"] = base + "/system/firmware";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:tag";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/firmware/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Latest Firmware Version Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Latest Firmware Version";
    doc["unique_id"] = deviceId + "_latest_firmware";
    doc["state_topic"] = base + "/system/latest_version";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:tag-arrow-up";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/sensor/" + deviceId + "/latest_firmware/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Reboot Button =====
  {
    JsonDocument doc;
    doc["name"] = "Reboot";
    doc["unique_id"] = deviceId + "_reboot";
    doc["command_topic"] = base + "/system/reboot";
    doc["payload_press"] = "REBOOT";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:restart";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/button/" + deviceId + "/reboot/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Check Update Button =====
  {
    JsonDocument doc;
    doc["name"] = "Check for Updates";
    doc["unique_id"] = deviceId + "_check_update";
    doc["command_topic"] = base + "/system/check_update";
    doc["payload_press"] = "CHECK";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:update";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/button/" + deviceId + "/check_update/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Factory Reset Button =====
  {
    JsonDocument doc;
    doc["name"] = "Factory Reset";
    doc["unique_id"] = deviceId + "_factory_reset";
    doc["command_topic"] = base + "/system/factory_reset";
    doc["payload_press"] = "RESET";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:factory";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/button/" + deviceId + "/factory_reset/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Auto-Update Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Auto Update";
    doc["unique_id"] = deviceId + "_auto_update";
    doc["state_topic"] = base + "/settings/auto_update";
    doc["command_topic"] = base + "/settings/auto_update/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:update";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/auto_update/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== OTA Channel Select =====
  {
    JsonDocument doc;
    doc["name"] = "Update Channel";
    doc["unique_id"] = deviceId + "_ota_channel";
    doc["state_topic"] = base + "/settings/ota_channel";
    doc["command_topic"] = base + "/settings/ota_channel/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("stable");
    options.add("beta");
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:tag-multiple";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/select/" + deviceId + "/ota_channel/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Firmware Update Entity =====
  // This provides the native HA Update entity with install capability
  {
    JsonDocument doc;
    doc["name"] = "Firmware";
    doc["unique_id"] = deviceId + "_firmware_update";
    doc["device_class"] = "firmware";
    doc["state_topic"] = base + "/system/update/state";
    doc["command_topic"] = base + "/system/update/command";
    doc["payload_install"] = "install";
    doc["entity_picture"] =
        "https://brands.home-assistant.io/_/esphome/icon.png";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/update/" + deviceId + "/firmware/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== IP Address Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "IP Address";
    doc["unique_id"] = deviceId + "_ip";
    doc["state_topic"] = base + "/wifi/ip";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:ip-network";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/ip/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }


  // ===== Hardware Diagnostics =====

  // ===== CPU Temperature Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "CPU Temperature";
    doc["unique_id"] = deviceId + "_cpu_temp";
    doc["state_topic"] = base + "/hardware/temperature";
    doc["unit_of_measurement"] = "°C";
    doc["device_class"] = "temperature";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:thermometer";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/cpu_temp/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== CPU Usage Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "CPU Usage";
    doc["unique_id"] = deviceId + "_cpu_usage";
    doc["state_topic"] = base + "/hardware/cpu_usage";
    doc["unit_of_measurement"] = "%";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:cpu-64-bit";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/cpu_usage/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Free Heap Memory Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Free Heap Memory";
    doc["unique_id"] = deviceId + "_heap_free";
    doc["state_topic"] = base + "/hardware/heap_free";
    doc["unit_of_measurement"] = "B";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:memory";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/heap_free/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Uptime Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Uptime";
    doc["unique_id"] = deviceId + "_uptime";
    doc["state_topic"] = base + "/system/uptime";
    doc["unit_of_measurement"] = "s";
    doc["device_class"] = "duration";
    doc["state_class"] = "total_increasing";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:clock-outline";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/uptime/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== LittleFS Used Storage Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "LittleFS Used";
    doc["unique_id"] = deviceId + "_LittleFS_used";
    doc["state_topic"] = base + "/hardware/LittleFS_used";
    doc["unit_of_measurement"] = "B";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:harddisk";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/LittleFS_used/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== WiFi Channel Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "WiFi Channel";
    doc["unique_id"] = deviceId + "_wifi_channel";
    doc["state_topic"] = base + "/wifi/channel";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:wifi";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/wifi_channel/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Dark Mode Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Dark Mode";
    doc["unique_id"] = deviceId + "_dark_mode";
    doc["state_topic"] = base + "/settings/dark_mode";
    doc["command_topic"] = base + "/settings/dark_mode/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:weather-night";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/dark_mode/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Certificate Validation Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Certificate Validation";
    doc["unique_id"] = deviceId + "_cert_validation";
    doc["state_topic"] = base + "/settings/cert_validation";
    doc["command_topic"] = base + "/settings/cert_validation/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:certificate";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/switch/" + deviceId + "/cert_validation/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Display Backlight Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Display Backlight";
    doc["unique_id"] = deviceId + "_backlight";
    doc["state_topic"] = base + "/display/backlight";
    doc["command_topic"] = base + "/display/backlight/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:brightness-6";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/backlight/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Screen Timeout Number =====
  {
    JsonDocument doc;
    doc["name"] = "Screen Timeout";
    doc["unique_id"] = deviceId + "_screen_timeout";
    doc["state_topic"] = base + "/settings/screen_timeout";
    doc["command_topic"] = base + "/settings/screen_timeout/set";
    doc["min"] = 0;
    doc["max"] = 600;
    doc["step"] = 30;
    doc["unit_of_measurement"] = "s";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:timer-off-outline";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/screen_timeout/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Dim Enabled Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Dim";
    doc["unique_id"] = deviceId + "_dim_enabled";
    doc["state_topic"] = base + "/display/dim_enabled";
    doc["command_topic"] = base + "/display/dim_enabled/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:brightness-auto";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/switch/" + deviceId + "/dim_enabled/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Dim Timeout Number =====
  {
    JsonDocument doc;
    doc["name"] = "Dim Timeout";
    doc["unique_id"] = deviceId + "_dim_timeout";
    doc["state_topic"] = base + "/settings/dim_timeout";
    doc["command_topic"] = base + "/settings/dim_timeout/set";
    doc["min"] = 0;
    doc["max"] = 60;
    doc["step"] = 5;
    doc["unit_of_measurement"] = "s";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:brightness-auto";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/dim_timeout/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Display Brightness Number =====
  {
    JsonDocument doc;
    doc["name"] = "Display Brightness";
    doc["unique_id"] = deviceId + "_brightness";
    doc["state_topic"] = base + "/display/brightness";
    doc["command_topic"] = base + "/display/brightness/set";
    doc["min"] = 10;
    doc["max"] = 100;
    doc["step"] = 25;
    doc["unit_of_measurement"] = "%";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:brightness-percent";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/brightness/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Dim Brightness Select =====
  {
    JsonDocument doc;
    doc["name"] = "Dim Brightness";
    doc["unique_id"] = deviceId + "_dim_brightness";
    doc["state_topic"] = base + "/display/dim_brightness";
    doc["command_topic"] = base + "/display/dim_brightness/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("10");
    options.add("25");
    options.add("50");
    options.add("75");
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:brightness-4";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/select/" + deviceId + "/dim_brightness/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Buzzer Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Buzzer";
    doc["unique_id"] = deviceId + "_buzzer";
    doc["state_topic"] = base + "/settings/buzzer";
    doc["command_topic"] = base + "/settings/buzzer/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:volume-high";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/buzzer/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Buzzer Volume Number =====
  {
    JsonDocument doc;
    doc["name"] = "Buzzer Volume";
    doc["unique_id"] = deviceId + "_buzzer_volume";
    doc["state_topic"] = base + "/settings/buzzer_volume";
    doc["command_topic"] = base + "/settings/buzzer_volume/set";
    doc["min"] = 0;
    doc["max"] = 2;
    doc["step"] = 1;
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:volume-medium";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/buzzer_volume/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }


  // ===== Audio Update Rate Select =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Update Rate";
    doc["unique_id"] = deviceId + "_audio_update_rate";
    doc["state_topic"] = base + "/settings/audio_update_rate";
    doc["command_topic"] = base + "/settings/audio_update_rate/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("20");
    options.add("33");
    options.add("50");
    options.add("100");
    doc["unit_of_measurement"] = "ms";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:update";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/select/" + deviceId + "/audio_update_rate/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Generator";
    doc["unique_id"] = deviceId + "_siggen_enabled";
    doc["state_topic"] = base + "/signalgenerator/enabled";
    doc["command_topic"] = base + "/signalgenerator/enabled/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/siggen_enabled/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Waveform Select =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Waveform";
    doc["unique_id"] = deviceId + "_siggen_waveform";
    doc["state_topic"] = base + "/signalgenerator/waveform";
    doc["command_topic"] = base + "/signalgenerator/waveform/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("sine");
    options.add("square");
    options.add("white_noise");
    options.add("sweep");
    doc["icon"] = "mdi:waveform";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/select/" + deviceId + "/siggen_waveform/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Frequency Number =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Frequency";
    doc["unique_id"] = deviceId + "_siggen_frequency";
    doc["state_topic"] = base + "/signalgenerator/frequency";
    doc["command_topic"] = base + "/signalgenerator/frequency/set";
    doc["min"] = 1;
    doc["max"] = 22000;
    doc["unit_of_measurement"] = "Hz";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/number/" + deviceId + "/siggen_frequency/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Amplitude Number =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Amplitude";
    doc["unique_id"] = deviceId + "_siggen_amplitude";
    doc["state_topic"] = base + "/signalgenerator/amplitude";
    doc["command_topic"] = base + "/signalgenerator/amplitude/set";
    doc["min"] = -96;
    doc["max"] = 0;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "dBFS";
    doc["icon"] = "mdi:volume-high";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/number/" + deviceId + "/siggen_amplitude/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Channel Select =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Channel";
    doc["unique_id"] = deviceId + "_siggen_channel";
    doc["state_topic"] = base + "/signalgenerator/channel";
    doc["command_topic"] = base + "/signalgenerator/channel/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("ch1");
    options.add("ch2");
    options.add("both");
    doc["icon"] = "mdi:speaker-multiple";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/select/" + deviceId + "/siggen_channel/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Output Mode Select =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Output Mode";
    doc["unique_id"] = deviceId + "_siggen_output_mode";
    doc["state_topic"] = base + "/signalgenerator/output_mode";
    doc["command_topic"] = base + "/signalgenerator/output_mode/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("software");
    options.add("pwm");
    doc["icon"] = "mdi:export";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/select/" + deviceId + "/siggen_output_mode/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Target ADC Select =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Target ADC";
    doc["unique_id"] = deviceId + "_siggen_target_adc";
    doc["state_topic"] = base + "/signalgenerator/target_adc";
    doc["command_topic"] = base + "/signalgenerator/target_adc/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("adc1");
    options.add("adc2");
    options.add("both");
    doc["icon"] = "mdi:audio-input-stereo-minijack";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
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
        JsonDocument doc;
        doc["name"] = String(nameBuf) + " Audio Level";
        doc["unique_id"] = deviceId + idSuffix + "_level";
        doc["state_topic"] = prefix + "/level";
        doc["unit_of_measurement"] = "dBFS";
        doc["state_class"] = "measurement";
        doc["icon"] = "mdi:volume-high";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + labelBuf + "_level/config").c_str(), payload.c_str(), true);
      }

      // Per-ADC Status Sensor
      {
        JsonDocument doc;
        doc["name"] = String(nameBuf) + " ADC Status";
        doc["unique_id"] = deviceId + idSuffix + "_adc_status";
        doc["state_topic"] = prefix + "/adc_status";
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:audio-input-stereo-minijack";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + labelBuf + "_adc_status/config").c_str(), payload.c_str(), true);
      }

      // Per-ADC Noise Floor Sensor
      {
        JsonDocument doc;
        doc["name"] = String(nameBuf) + " Noise Floor";
        doc["unique_id"] = deviceId + idSuffix + "_noise_floor";
        doc["state_topic"] = prefix + "/noise_floor";
        doc["unit_of_measurement"] = "dBFS";
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:volume-low";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + labelBuf + "_noise_floor/config").c_str(), payload.c_str(), true);
      }

      // Per-ADC Vrms Sensor
      {
        JsonDocument doc;
        doc["name"] = String(nameBuf) + " Vrms";
        doc["unique_id"] = deviceId + idSuffix + "_vrms";
        doc["state_topic"] = prefix + "/vrms";
        doc["unit_of_measurement"] = "V";
        doc["device_class"] = "voltage";
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["suggested_display_precision"] = 3;
        doc["icon"] = "mdi:sine-wave";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + labelBuf + "_vrms/config").c_str(), payload.c_str(), true);
      }

      // SNR/SFDR discovery removed — debug-only data, accessible via REST/WS/GUI.
      // Cleanup of orphaned entities is handled in removeHADiscovery().
    }
  }

  // ===== Legacy Combined Audio ADC Status Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "ADC Status";
    doc["unique_id"] = deviceId + "_adc_status";
    doc["state_topic"] = base + "/audio/adc_status";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:audio-input-stereo-minijack";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/adc_status/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Legacy Combined Audio Noise Floor Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Noise Floor";
    doc["unique_id"] = deviceId + "_noise_floor";
    doc["state_topic"] = base + "/audio/noise_floor";
    doc["unit_of_measurement"] = "dBFS";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:volume-low";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/noise_floor/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Legacy Combined Input Voltage (Vrms) Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Input Voltage (Vrms)";
    doc["unique_id"] = deviceId + "_input_vrms";
    doc["state_topic"] = base + "/audio/input_vrms";
    doc["unit_of_measurement"] = "V";
    doc["device_class"] = "voltage";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["suggested_display_precision"] = 3;
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/input_vrms/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== ADC Reference Voltage Number =====
  {
    JsonDocument doc;
    doc["name"] = "ADC Reference Voltage";
    doc["unique_id"] = deviceId + "_adc_vref";
    doc["state_topic"] = base + "/settings/adc_vref";
    doc["command_topic"] = base + "/settings/adc_vref/set";
    doc["min"] = 1.0;
    doc["max"] = 5.0;
    doc["step"] = 0.1;
    doc["unit_of_measurement"] = "V";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:flash-triangle-outline";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/number/" + deviceId + "/adc_vref/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Per-ADC Enable Switches =====
  {
    const char *adcNames[] = {"ADC Input 1", "ADC Input 2"};
    const char *adcIds[] = {"input1_enabled", "input2_enabled"};
    const char *adcTopics[] = {"/audio/input1/enabled", "/audio/input2/enabled"};
    for (int a = 0; a < 2; a++) {
      JsonDocument doc;
      doc["name"] = adcNames[a];
      doc["unique_id"] = deviceId + "_" + adcIds[a];
      doc["state_topic"] = base + adcTopics[a];
      doc["command_topic"] = base + String(adcTopics[a]) + "/set";
      doc["payload_on"] = "ON";
      doc["payload_off"] = "OFF";
      doc["entity_category"] = "config";
      doc["icon"] = "mdi:audio-input-stereo-minijack";
      addHADeviceInfo(doc);
      String payload;
      serializeJson(doc, payload);
      String topic = "homeassistant/switch/" + deviceId + "/" + adcIds[a] + "/config";
      mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }
  }

  // ===== VU Meter Switch =====
  {
    JsonDocument doc;
    doc["name"] = "VU Meter";
    doc["unique_id"] = deviceId + "_vu_meter";
    doc["state_topic"] = base + "/audio/vu_meter";
    doc["command_topic"] = base + "/audio/vu_meter/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:chart-bar";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/vu_meter/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Audio Waveform Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Waveform";
    doc["unique_id"] = deviceId + "_waveform";
    doc["state_topic"] = base + "/audio/waveform";
    doc["command_topic"] = base + "/audio/waveform/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:waveform";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/waveform/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Frequency Spectrum Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Frequency Spectrum";
    doc["unique_id"] = deviceId + "_spectrum";
    doc["state_topic"] = base + "/audio/spectrum";
    doc["command_topic"] = base + "/audio/spectrum/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:equalizer";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/spectrum/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== FFT Window Type Select =====
  {
    JsonDocument doc;
    doc["name"] = "FFT Window";
    doc["unique_id"] = deviceId + "_fft_window";
    doc["state_topic"] = base + "/audio/fft_window";
    doc["command_topic"] = base + "/audio/fft_window/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("hann");
    options.add("blackman");
    options.add("blackman_harris");
    options.add("blackman_nuttall");
    options.add("nuttall");
    options.add("flat_top");
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:window-shutter-settings";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/select/" + deviceId + "/fft_window/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }


  // ===== Debug Mode Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Debug Mode";
    doc["unique_id"] = deviceId + "_debug_mode";
    doc["state_topic"] = base + "/debug/mode";
    doc["command_topic"] = base + "/debug/mode/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:bug";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/debug_mode/config").c_str(), payload.c_str(), true);
  }

  // ===== Debug Serial Level Number =====
  {
    JsonDocument doc;
    doc["name"] = "Debug Serial Level";
    doc["unique_id"] = deviceId + "_debug_serial_level";
    doc["state_topic"] = base + "/debug/serial_level";
    doc["command_topic"] = base + "/debug/serial_level/set";
    doc["min"] = 0;
    doc["max"] = 3;
    doc["step"] = 1;
    doc["mode"] = "slider";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:console";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/number/" + deviceId + "/debug_serial_level/config").c_str(), payload.c_str(), true);
  }

  // ===== Debug HW Stats Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Debug HW Stats";
    doc["unique_id"] = deviceId + "_debug_hw_stats";
    doc["state_topic"] = base + "/debug/hw_stats";
    doc["command_topic"] = base + "/debug/hw_stats/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:chart-line";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/debug_hw_stats/config").c_str(), payload.c_str(), true);
  }

  // ===== Debug I2S Metrics Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Debug I2S Metrics";
    doc["unique_id"] = deviceId + "_debug_i2s_metrics";
    doc["state_topic"] = base + "/debug/i2s_metrics";
    doc["command_topic"] = base + "/debug/i2s_metrics/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:timer-outline";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/debug_i2s_metrics/config").c_str(), payload.c_str(), true);
  }

  // ===== Debug Task Monitor Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Debug Task Monitor";
    doc["unique_id"] = deviceId + "_debug_task_monitor";
    doc["state_topic"] = base + "/debug/task_monitor";
    doc["command_topic"] = base + "/debug/task_monitor/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:format-list-bulleted";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/debug_task_monitor/config").c_str(), payload.c_str(), true);
  }

  // ===== Task Monitor Diagnostic Sensors =====
  {
    JsonDocument doc;
    doc["name"] = "Task Count";
    doc["unique_id"] = deviceId + "_task_count";
    doc["state_topic"] = base + "/hardware/task_count";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:format-list-numbered";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/task_count/config").c_str(), payload.c_str(), true);
  }
  {
    JsonDocument doc;
    doc["name"] = "Loop Time";
    doc["unique_id"] = deviceId + "_loop_time";
    doc["state_topic"] = base + "/hardware/loop_time_us";
    doc["unit_of_measurement"] = "us";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:timer-outline";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/loop_time/config").c_str(), payload.c_str(), true);
  }
  {
    JsonDocument doc;
    doc["name"] = "Loop Time Max";
    doc["unique_id"] = deviceId + "_loop_time_max";
    doc["state_topic"] = base + "/hardware/loop_time_max_us";
    doc["unit_of_measurement"] = "us";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:timer-alert-outline";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/loop_time_max/config").c_str(), payload.c_str(), true);
  }
  {
    JsonDocument doc;
    doc["name"] = "Min Stack Free";
    doc["unique_id"] = deviceId + "_min_stack_free";
    doc["state_topic"] = base + "/hardware/min_stack_free";
    doc["unit_of_measurement"] = "B";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:memory";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/min_stack_free/config").c_str(), payload.c_str(), true);
  }

  // ===== Crash Diagnostics — Reset Reason (diagnostic sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Reset Reason";
    doc["unique_id"] = deviceId + "_reset_reason";
    doc["state_topic"] = base + "/diagnostics/reset_reason";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:restart-alert";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/reset_reason/config").c_str(), payload.c_str(), true);
  }

  // ===== Crash Diagnostics — Was Crash (binary sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Last Boot Was Crash";
    doc["unique_id"] = deviceId + "_was_crash";
    doc["state_topic"] = base + "/diagnostics/was_crash";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "problem";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/was_crash/config").c_str(), payload.c_str(), true);
  }

  // ===== Heap Health — Heap Critical (binary sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Heap Critical";
    doc["unique_id"] = deviceId + "_heap_critical";
    doc["state_topic"] = base + "/diagnostics/heap_critical";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "problem";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/heap_critical/config").c_str(), payload.c_str(), true);
  }

  // ===== Audio DMA Alloc Failed (binary sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Audio DMA Alloc Failed";
    doc["unique_id"] = deviceId + "_dma_alloc_failed";
    doc["state_topic"] = base + "/diagnostics/dma_alloc_failed";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "problem";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/dma_alloc_failed/config").c_str(), payload.c_str(), true);
  }

  // ===== Heap Health — Max Alloc Block (diagnostic sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Heap Max Block";
    doc["unique_id"] = deviceId + "_heap_max_block";
    doc["state_topic"] = base + "/diagnostics/heap_max_block";
    doc["unit_of_measurement"] = "B";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:memory";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/heap_max_block/config").c_str(), payload.c_str(), true);
  }

  // ===== Timezone Offset Number =====
  {
    JsonDocument doc;
    doc["name"] = "Timezone Offset";
    doc["unique_id"] = deviceId + "_timezone_offset";
    doc["state_topic"] = base + "/settings/timezone_offset";
    doc["command_topic"] = base + "/settings/timezone_offset/set";
    doc["min"] = -12;
    doc["max"] = 14;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "h";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:map-clock-outline";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/number/" + deviceId + "/timezone_offset/config").c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Sweep Speed Number =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Sweep Speed";
    doc["unique_id"] = deviceId + "_siggen_sweep_speed";
    doc["state_topic"] = base + "/signalgenerator/sweep_speed";
    doc["command_topic"] = base + "/signalgenerator/sweep_speed/set";
    doc["min"] = 0.1;
    doc["max"] = 10.0;
    doc["step"] = 0.1;
    doc["unit_of_measurement"] = "Hz/s";
    doc["icon"] = "mdi:speedometer";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
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
      JsonDocument doc;
      doc["name"] = inputDisplayNames[i];
      doc["unique_id"] = deviceId + "_" + inputLabels[i];
      doc["state_topic"] = base + "/audio/" + inputLabels[i];
      doc["entity_category"] = "diagnostic";
      doc["icon"] = "mdi:label-outline";
      addHADeviceInfo(doc);
      String payload;
      serializeJson(doc, payload);
      mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + inputLabels[i] + "/config").c_str(), payload.c_str(), true);
    }
  }


#ifdef DSP_ENABLED
  // ===== DSP Enabled Switch =====
  {
    JsonDocument doc;
    doc["name"] = "DSP";
    doc["unique_id"] = deviceId + "_dsp_enabled";
    doc["state_topic"] = base + "/dsp/enabled";
    doc["command_topic"] = base + "/dsp/enabled/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:equalizer";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/dsp_enabled/config").c_str(), payload.c_str(), true);
  }

  // ===== DSP Bypass Switch =====
  {
    JsonDocument doc;
    doc["name"] = "DSP Bypass";
    doc["unique_id"] = deviceId + "_dsp_bypass";
    doc["state_topic"] = base + "/dsp/bypass";
    doc["command_topic"] = base + "/dsp/bypass/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:debug-step-over";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/dsp_bypass/config").c_str(), payload.c_str(), true);
  }

  // ===== DSP CPU Load Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "DSP CPU Load";
    doc["unique_id"] = deviceId + "_dsp_cpu_load";
    doc["state_topic"] = base + "/dsp/cpu_load";
    doc["unit_of_measurement"] = "%";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:cpu-64-bit";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/dsp_cpu_load/config").c_str(), payload.c_str(), true);
  }

  // ===== DSP Preset Select =====
  {
    JsonDocument doc;
    doc["name"] = "DSP Preset";
    doc["unique_id"] = deviceId + "_dsp_preset";
    doc["state_topic"] = base + "/dsp/preset";
    doc["command_topic"] = base + "/dsp/preset/set";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:playlist-music";
    JsonArray opts = doc["options"].to<JsonArray>();
    opts.add("Custom");
    extern bool dsp_preset_exists(int);
    for (int i = 0; i < DSP_PRESET_MAX_SLOTS; i++) {
      if (appState.dsp.presetNames[i][0] && dsp_preset_exists(i)) {
        opts.add(appState.dsp.presetNames[i]);
      }
    }
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
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
        JsonDocument doc;
        doc["name"] = String("DSP ") + chNames[ch] + " Bypass";
        doc["unique_id"] = deviceId + idSuffix + "_bypass";
        doc["state_topic"] = chPrefix + "/bypass";
        doc["command_topic"] = chPrefix + "/bypass/set";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["entity_category"] = "config";
        doc["icon"] = "mdi:debug-step-over";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/switch/" + deviceId + "/dsp_ch" + String(ch) + "_bypass/config").c_str(), payload.c_str(), true);
      }

      // Per-channel stage count sensor
      {
        JsonDocument doc;
        doc["name"] = String("DSP ") + chNames[ch] + " Stages";
        doc["unique_id"] = deviceId + idSuffix + "_stages";
        doc["state_topic"] = chPrefix + "/stage_count";
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:filter";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/dsp_ch" + String(ch) + "_stages/config").c_str(), payload.c_str(), true);
      }

      // Per-channel limiter gain reduction sensor
      {
        JsonDocument doc;
        doc["name"] = String("DSP ") + chNames[ch] + " Limiter GR";
        doc["unique_id"] = deviceId + idSuffix + "_limiter_gr";
        doc["state_topic"] = chPrefix + "/limiter_gr";
        doc["unit_of_measurement"] = "dB";
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:arrow-collapse-down";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/dsp_ch" + String(ch) + "_limiter_gr/config").c_str(), payload.c_str(), true);
      }
    }
  }

  // ===== PEQ Bypass Switch =====
  {
    JsonDocument doc;
    doc["name"] = "PEQ Bypass";
    doc["unique_id"] = deviceId + "_peq_bypass";
    doc["state_topic"] = base + "/dsp/peq/bypass";
    doc["command_topic"] = base + "/dsp/peq/bypass/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:equalizer";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/peq_bypass/config").c_str(), payload.c_str(), true);
  }

  // PEQ band switches removed — controlled via DSP API / WebSocket only.
  // Cleanup of orphaned PEQ band entities is handled in removeHADiscovery().
#endif

#ifdef GUI_ENABLED
  // ===== Boot Animation Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Boot Animation";
    doc["unique_id"] = deviceId + "_boot_animation";
    doc["state_topic"] = base + "/settings/boot_animation";
    doc["command_topic"] = base + "/settings/boot_animation/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:animation-play";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/boot_animation/config").c_str(), payload.c_str(), true);
  }

  // ===== Boot Animation Style Select =====
  {
    JsonDocument doc;
    doc["name"] = "Boot Animation Style";
    doc["unique_id"] = deviceId + "_boot_animation_style";
    doc["state_topic"] = base + "/settings/boot_animation_style";
    doc["command_topic"] = base + "/settings/boot_animation_style/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("wave_pulse");
    options.add("speaker_ripple");
    options.add("waveform");
    options.add("beat_bounce");
    options.add("freq_bars");
    options.add("heartbeat");
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:animation";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/select/" + deviceId + "/boot_animation_style/config").c_str(), payload.c_str(), true);
  }
#endif

#ifdef USB_AUDIO_ENABLED
  // ===== USB Audio Connected Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "USB Connected";
    doc["unique_id"] = deviceId + "_usb_audio_connected";
    doc["state_topic"] = base + "/" + MQTT_TOPIC_USB_CONNECTED;
    doc["payload_on"] = "true";
    doc["payload_off"] = "false";
    doc["device_class"] = "connectivity";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:usb";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/usb_audio_connected/config").c_str(), payload.c_str(), true);
  }

  // ===== USB Audio Streaming Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "USB Streaming";
    doc["unique_id"] = deviceId + "_usb_audio_streaming";
    doc["state_topic"] = base + "/" + MQTT_TOPIC_USB_STREAMING;
    doc["payload_on"] = "true";
    doc["payload_off"] = "false";
    doc["device_class"] = "running";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:music";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/usb_audio_streaming/config").c_str(), payload.c_str(), true);
  }

  // ===== USB Audio Enabled Switch =====
  {
    JsonDocument doc;
    doc["name"] = "USB Audio";
    doc["unique_id"] = deviceId + "_usb_audio_enabled";
    doc["state_topic"] = base + "/" + MQTT_TOPIC_USB_ENABLED;
    doc["command_topic"] = base + "/" + MQTT_TOPIC_USB_ENABLE_SET;
    doc["payload_on"] = "true";
    doc["payload_off"] = "false";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:usb-port";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/usb_audio_enabled/config").c_str(), payload.c_str(), true);
  }

  // ===== USB Audio Sample Rate Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "USB Sample Rate";
    doc["unique_id"] = deviceId + "_usb_audio_sample_rate";
    doc["state_topic"] = base + "/" + MQTT_TOPIC_USB_RATE;
    doc["unit_of_measurement"] = "Hz";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/usb_audio_sample_rate/config").c_str(), payload.c_str(), true);
  }

  // ===== USB Audio Volume Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "USB Volume";
    doc["unique_id"] = deviceId + "_usb_audio_volume";
    doc["state_topic"] = base + "/" + MQTT_TOPIC_USB_VOLUME;
    doc["unit_of_measurement"] = "dB";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:volume-high";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/usb_audio_volume/config").c_str(), payload.c_str(), true);
  }

  // ===== USB Audio Buffer Overruns Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "USB Buffer Overruns";
    doc["unique_id"] = deviceId + "_usb_audio_overruns";
    doc["state_topic"] = base + "/" + MQTT_TOPIC_USB_OVERRUNS;
    doc["state_class"] = "total_increasing";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:alert-circle-outline";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/usb_audio_overruns/config").c_str(), payload.c_str(), true);
  }

  // ===== USB Audio Buffer Underruns Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "USB Buffer Underruns";
    doc["unique_id"] = deviceId + "_usb_audio_underruns";
    doc["state_topic"] = base + "/" + MQTT_TOPIC_USB_UNDERRUNS;
    doc["state_class"] = "total_increasing";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:alert-outline";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
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
        JsonDocument doc;
        String uid = deviceId + "_hal_" + String(dev->getSlot());
        doc["unique_id"]    = uid + "_available";
        doc["name"]         = String(dev->getDescriptor().name) + " Available";
        doc["state_topic"]  = base + "/hal/" + String(dev->getSlot()) + "/available";
        doc["payload_on"]   = "true";
        doc["payload_off"]  = "false";
        doc["device_class"] = "connectivity";
        doc["entity_category"] = "diagnostic";
        addHADeviceInfo(doc);

        String payload;
        serializeJson(doc, payload);
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
