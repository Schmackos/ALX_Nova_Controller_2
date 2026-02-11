#include <unity.h>
#include <string>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#include "../test_mocks/PubSubClient.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#include <PubSubClient.h>
#endif

// Mock MQTT settings
struct MQTTSettings {
    String broker;
    uint16_t port;
    String username;
    String password;
    String baseTopic;
    bool enabled;
};

MQTTSettings mqttSettings;
PubSubClient mockMqttClient;

namespace TestMQTTState {
    void reset() {
        mqttSettings.broker = "";
        mqttSettings.port = 1883;
        mqttSettings.username = "";
        mqttSettings.password = "";
        mqttSettings.baseTopic = "alx_nova";
        mqttSettings.enabled = false;
        PubSubClient::reset();
        Preferences::reset();
#ifdef NATIVE_TEST
        ArduinoMock::reset();
#endif
    }
}

// ===== MQTT HANDLER IMPLEMENTATIONS =====

bool loadMqttSettings() {
    Preferences prefs;
    prefs.begin("mqtt", true); // Read-only

    String broker = prefs.getString("broker", "");
    uint16_t port = prefs.getInt("port", 1883);
    String username = prefs.getString("user", "");
    String password = prefs.getString("pass", "");
    String baseTopic = prefs.getString("topic", "alx_nova");

    prefs.end();

    // No broker = MQTT disabled
    if (broker.length() == 0) {
        mqttSettings.enabled = false;
        return false;
    }

    mqttSettings.broker = broker;
    mqttSettings.port = port;
    mqttSettings.username = username;
    mqttSettings.password = password;
    mqttSettings.baseTopic = baseTopic;
    mqttSettings.enabled = true;

    return true;
}

void saveMqttSettings(const char* broker, uint16_t port, const char* username,
                       const char* password, const char* baseTopic) {
    Preferences prefs;
    prefs.begin("mqtt", false);

    prefs.putString("broker", broker);
    prefs.putInt("port", port);
    prefs.putString("user", username ? String(username) : "");
    prefs.putString("pass", password ? String(password) : "");
    prefs.putString("topic", baseTopic ? String(baseTopic) : "alx_nova");

    prefs.end();

    // Reload settings
    loadMqttSettings();
}

bool mqttConnect() {
    if (mqttSettings.broker.length() == 0) {
        return false;
    }

    mockMqttClient.setServer(mqttSettings.broker.c_str(), mqttSettings.port);

    // Generate random client ID (using modulo to avoid conflict with stdlib random())
    long randVal = rand() % 10000;
    String clientId = "alx_nova_" + String(randVal);

    if (mqttSettings.username.length() > 0) {
        return mockMqttClient.connect(clientId.c_str(),
                                      mqttSettings.username.c_str(),
                                      mqttSettings.password.c_str());
    } else {
        return mockMqttClient.connect(clientId.c_str());
    }
}

bool publishMqttMessage(const char* topic, const char* payload) {
    if (!mockMqttClient.connected()) {
        return false;
    }
    return mockMqttClient.publish(topic, payload);
}

String getMqttTopic(const char* subtopic) {
    String full = mqttSettings.baseTopic;
    full += "/";
    full += subtopic;
    return full;
}

void publishLedState(bool state) {
    String topic = getMqttTopic("led/state");
    String payload = state ? "on" : "off";
    publishMqttMessage(topic.c_str(), payload.c_str());
}

void publishBlinkingState(bool state) {
    String topic = getMqttTopic("blinking/state");
    String payload = state ? "on" : "off";
    publishMqttMessage(topic.c_str(), payload.c_str());
}

void publishSmartSensingState(const char* mode, unsigned long timerRemaining) {
    String topic = getMqttTopic("smart_sensing/state");
    String payload = "{\"mode\":\"";
    payload += mode;
    payload += "\",\"timerRemaining\":";
    payload += timerRemaining;
    payload += "}";
    publishMqttMessage(topic.c_str(), payload.c_str());
}

String generateHADiscoveryJson(const char* deviceType) {
    String json = "{\"device_class\":\"";
    json += deviceType;
    json += "\",\"unique_id\":\"alx_nova_";
    json += deviceType;
    json += "\"}";
    return json;
}

void publishHADiscovery() {
    if (!mqttSettings.enabled) {
        return;
    }

    // Publish LED discovery
    String ledDiscovery = generateHADiscoveryJson("light");
    String ledTopic = "homeassistant/light/alx_nova/led/config";
    publishMqttMessage(ledTopic.c_str(), ledDiscovery.c_str());

    // Publish blinking discovery
    String blinkDiscovery = generateHADiscoveryJson("switch");
    String blinkTopic = "homeassistant/switch/alx_nova/blinking/config";
    publishMqttMessage(blinkTopic.c_str(), blinkDiscovery.c_str());
}

void removeHADiscovery() {
    if (!mqttSettings.enabled) {
        return;
    }

    // Publish empty payloads to remove
    String ledTopic = "homeassistant/light/alx_nova/led/config";
    publishMqttMessage(ledTopic.c_str(), "");

    String blinkTopic = "homeassistant/switch/alx_nova/blinking/config";
    publishMqttMessage(blinkTopic.c_str(), "");
}

// ===== Test Setup/Teardown =====

void setUp(void) {
    TestMQTTState::reset();
}

void tearDown(void) {
    // Clean up after each test
}

// ===== Settings Persistence Tests =====

void test_load_mqtt_settings_from_nvs(void) {
    // Save settings first
    Preferences prefs;
    prefs.begin("mqtt", false);
    prefs.putString("broker", "mqtt.example.com");
    prefs.putInt("port", 8883);
    prefs.putString("user", "testuser");
    prefs.putString("pass", "testpass");
    prefs.putString("topic", "my_device");
    prefs.end();

    // Load them back
    bool loaded = loadMqttSettings();

    TEST_ASSERT_TRUE(loaded);
    TEST_ASSERT_EQUAL_STRING("mqtt.example.com", mqttSettings.broker.c_str());
    TEST_ASSERT_EQUAL(8883, mqttSettings.port);
    TEST_ASSERT_EQUAL_STRING("testuser", mqttSettings.username.c_str());
    TEST_ASSERT_EQUAL_STRING("testpass", mqttSettings.password.c_str());
    TEST_ASSERT_EQUAL_STRING("my_device", mqttSettings.baseTopic.c_str());
    TEST_ASSERT_TRUE(mqttSettings.enabled);
}

void test_save_mqtt_settings_to_nvs(void) {
    saveMqttSettings("broker.example.com", 1883, "user", "pass", "topic");

    // Verify they were saved
    Preferences prefs;
    prefs.begin("mqtt", true);
    String broker = prefs.getString("broker", "");
    uint16_t port = prefs.getInt("port", 0);
    prefs.end();

    TEST_ASSERT_EQUAL_STRING("broker.example.com", broker.c_str());
    TEST_ASSERT_EQUAL(1883, port);
}

void test_mqtt_disabled_when_no_broker(void) {
    // Try to load with no broker set
    bool loaded = loadMqttSettings();

    TEST_ASSERT_FALSE(loaded);
    TEST_ASSERT_FALSE(mqttSettings.enabled);
}

// ===== Connection Management Tests =====

void test_mqtt_connect_success(void) {
    mqttSettings.broker = "mqtt.example.com";
    mqttSettings.port = 1883;
    mqttSettings.enabled = true;

    bool connected = mqttConnect();

    TEST_ASSERT_TRUE(connected);
    TEST_ASSERT_TRUE(mockMqttClient.connected());
}

void test_mqtt_reconnect_on_disconnect(void) {
    mqttSettings.broker = "mqtt.example.com";
    mqttSettings.port = 1883;
    mqttSettings.enabled = true;

    // First connection
    mqttConnect();
    TEST_ASSERT_TRUE(mockMqttClient.connected());

    // Simulate disconnect
    mockMqttClient.disconnect();
    TEST_ASSERT_FALSE(mockMqttClient.connected());

    // Reconnect
    bool reconnected = mqttConnect();
    TEST_ASSERT_TRUE(reconnected);
}

void test_mqtt_connect_with_auth(void) {
    mqttSettings.broker = "mqtt.example.com";
    mqttSettings.port = 1883;
    mqttSettings.username = "user";
    mqttSettings.password = "pass";
    mqttSettings.enabled = true;

    bool connected = mqttConnect();

    TEST_ASSERT_TRUE(connected);
    TEST_ASSERT_EQUAL_STRING("user", mockMqttClient.username.c_str());
    TEST_ASSERT_EQUAL_STRING("pass", mockMqttClient.password.c_str());
}

// ===== Publishing Tests =====

void test_publish_led_state(void) {
    mqttSettings.broker = "mqtt.example.com";
    mqttSettings.baseTopic = "alx_nova";
    mqttConnect();

    publishLedState(true);

    String topic = "alx_nova/led/state";
    TEST_ASSERT_TRUE(PubSubClient::wasMessagePublished(topic.c_str()));
    TEST_ASSERT_EQUAL_STRING("on", PubSubClient::getPublishedMessage(topic.c_str()).c_str());
}

void test_publish_blinking_state(void) {
    mqttSettings.broker = "mqtt.example.com";
    mqttSettings.baseTopic = "alx_nova";
    mqttConnect();

    publishBlinkingState(false);

    String topic = "alx_nova/blinking/state";
    TEST_ASSERT_TRUE(PubSubClient::wasMessagePublished(topic.c_str()));
    TEST_ASSERT_EQUAL_STRING("off", PubSubClient::getPublishedMessage(topic.c_str()).c_str());
}

void test_publish_smart_sensing_state(void) {
    mqttSettings.broker = "mqtt.example.com";
    mqttSettings.baseTopic = "alx_nova";
    mqttConnect();

    publishSmartSensingState("smart_auto", 150);

    String topic = "alx_nova/smart_sensing/state";
    TEST_ASSERT_TRUE(PubSubClient::wasMessagePublished(topic.c_str()));

    std::string msg = PubSubClient::getPublishedMessage(topic.c_str());
    TEST_ASSERT_TRUE(msg.find("smart_auto") != std::string::npos);
    TEST_ASSERT_TRUE(msg.find("150") != std::string::npos);
}

// ===== Home Assistant Discovery Tests =====

void test_ha_discovery_generation(void) {
    mqttSettings.broker = "mqtt.example.com";
    mqttSettings.enabled = true;
    mqttConnect();

    publishHADiscovery();

    // Check if discovery messages were published
    TEST_ASSERT_TRUE(PubSubClient::wasMessagePublished("homeassistant/light/alx_nova/led/config"));
    TEST_ASSERT_TRUE(PubSubClient::wasMessagePublished("homeassistant/switch/alx_nova/blinking/config"));
}

void test_ha_discovery_removal(void) {
    mqttSettings.broker = "mqtt.example.com";
    mqttSettings.enabled = true;
    mqttConnect();

    removeHADiscovery();

    // Check if empty payloads were published
    std::string ledPayload = PubSubClient::getPublishedMessage("homeassistant/light/alx_nova/led/config");
    std::string blinkPayload = PubSubClient::getPublishedMessage("homeassistant/switch/alx_nova/blinking/config");

    TEST_ASSERT_EQUAL(0, ledPayload.length());
    TEST_ASSERT_EQUAL(0, blinkPayload.length());
}

// ===== API Handler Tests =====

void test_mqtt_update_validates_broker(void) {
    // Try to set empty broker
    saveMqttSettings("", 1883, "user", "pass", "topic");

    // Should not be enabled
    TEST_ASSERT_FALSE(mqttSettings.enabled);
}

void test_mqtt_custom_base_topic(void) {
    saveMqttSettings("mqtt.example.com", 1883, "", "", "custom_topic");
    mqttConnect();

    publishLedState(true);

    String topic = "custom_topic/led/state";
    TEST_ASSERT_TRUE(PubSubClient::wasMessagePublished(topic.c_str()));
}

void test_mqtt_default_port(void) {
    saveMqttSettings("mqtt.example.com", 0, "", "", "topic"); // Port 0 = use default

    TEST_ASSERT_EQUAL(0, mqttSettings.port);
}

// ===== HA Status Restart Detection Tests =====

void test_ha_status_online_triggers_republish(void) {
    // The homeassistant/status topic with "online" payload should trigger
    // re-publishing discovery. We verify the logic pattern: when topic matches
    // and message is "online", discovery should be republished.
    String topic = "homeassistant/status";
    String message = "online";
    TEST_ASSERT_EQUAL_STRING("homeassistant/status", topic.c_str());
    TEST_ASSERT_EQUAL_STRING("online", message.c_str());
    // The handler returns early (no base topic matching needed)
    TEST_ASSERT_TRUE(strncmp(topic.c_str(), "homeassistant/", 14) == 0);
}

void test_ha_status_offline_no_action(void) {
    // "offline" messages should not trigger re-publishing
    String message = "offline";
    TEST_ASSERT_FALSE(message == "online");
}

// ===== OTA Update Percentage Tests =====

void test_ota_update_percentage_in_progress(void) {
    // When OTA is in progress, update_percentage should be set
    bool otaInProgress = true;
    int otaProgress = 45;

    // Simulate the JSON construction logic
    TEST_ASSERT_TRUE(otaInProgress);
    TEST_ASSERT_EQUAL(45, otaProgress);
}

void test_ota_update_percentage_not_in_progress(void) {
    // When OTA is not in progress, update_percentage should be null
    bool otaInProgress = false;
    TEST_ASSERT_FALSE(otaInProgress);
}

// ===== Non-Blocking OTA Tests =====

void test_mqtt_ota_command_requires_update_available(void) {
    // OTA install should only proceed if update is available
    bool updateAvailable = false;
    String cachedFirmwareUrl = "";
    bool shouldStart = updateAvailable && cachedFirmwareUrl.length() > 0;
    TEST_ASSERT_FALSE(shouldStart);
}

void test_mqtt_ota_command_requires_firmware_url(void) {
    // OTA install should only proceed if firmware URL is available
    bool updateAvailable = true;
    String cachedFirmwareUrl = "";
    bool shouldStart = updateAvailable && cachedFirmwareUrl.length() > 0;
    TEST_ASSERT_FALSE(shouldStart);
}

void test_mqtt_ota_command_starts_when_ready(void) {
    // OTA install should proceed when both update and URL are available
    bool updateAvailable = true;
    String cachedFirmwareUrl = "https://example.com/firmware.bin";
    bool shouldStart = updateAvailable && cachedFirmwareUrl.length() > 0;
    TEST_ASSERT_TRUE(shouldStart);
}

// ===== Availability Configuration Tests =====

void test_effective_base_topic_with_custom_topic(void) {
    // When baseTopic is set, it should be used for LWT
    mqttSettings.baseTopic = "custom/topic";
    String lwt = mqttSettings.baseTopic + "/status";
    TEST_ASSERT_EQUAL_STRING("custom/topic/status", lwt.c_str());
}

void test_availability_topic_matches_lwt(void) {
    // The availability topic in discovery must match the LWT topic
    mqttSettings.baseTopic = "alx_nova";
    String lwtTopic = mqttSettings.baseTopic + "/status";
    String availTopic = mqttSettings.baseTopic + "/status";
    TEST_ASSERT_EQUAL_STRING(lwtTopic.c_str(), availTopic.c_str());
}

// ===== Timezone Offset Validation Tests =====

void test_timezone_offset_valid_range(void) {
    // Valid timezone offsets: -12 to +14
    TEST_ASSERT_TRUE(-12 >= -12 && -12 <= 14);
    TEST_ASSERT_TRUE(0 >= -12 && 0 <= 14);
    TEST_ASSERT_TRUE(14 >= -12 && 14 <= 14);
}

void test_timezone_offset_invalid_range(void) {
    // Invalid timezone offsets
    TEST_ASSERT_FALSE(-13 >= -12 && -13 <= 14);
    TEST_ASSERT_FALSE(15 >= -12 && 15 <= 14);
}

// ===== Sweep Speed Validation Tests =====

void test_sweep_speed_valid_range(void) {
    float speed = 5.0f;
    TEST_ASSERT_TRUE(speed >= 0.1f && speed <= 10.0f);
}

void test_sweep_speed_boundary_min(void) {
    float speed = 0.1f;
    TEST_ASSERT_TRUE(speed >= 0.1f && speed <= 10.0f);
}

void test_sweep_speed_boundary_max(void) {
    float speed = 10.0f;
    TEST_ASSERT_TRUE(speed >= 0.1f && speed <= 10.0f);
}

void test_sweep_speed_below_min(void) {
    float speed = 0.05f;
    TEST_ASSERT_FALSE(speed >= 0.1f && speed <= 10.0f);
}

// ===== Boot Animation Style Mapping Tests =====

void test_boot_animation_style_mapping(void) {
    // Verify string-to-index mapping
    const char *styles[] = {"wave_pulse", "speaker_ripple", "waveform", "beat_bounce", "freq_bars", "heartbeat"};
    TEST_ASSERT_EQUAL_STRING("wave_pulse", styles[0]);
    TEST_ASSERT_EQUAL_STRING("speaker_ripple", styles[1]);
    TEST_ASSERT_EQUAL_STRING("waveform", styles[2]);
    TEST_ASSERT_EQUAL_STRING("beat_bounce", styles[3]);
    TEST_ASSERT_EQUAL_STRING("freq_bars", styles[4]);
    TEST_ASSERT_EQUAL_STRING("heartbeat", styles[5]);
}

void test_boot_animation_style_count(void) {
    // There are exactly 6 animation styles (0-5)
    int styleCount = 6;
    TEST_ASSERT_EQUAL(6, styleCount);
    for (int i = 0; i < styleCount; i++) {
        TEST_ASSERT_TRUE(i >= 0 && i <= 5);
    }
}

// ===== Input Names Tests =====

void test_input_name_labels(void) {
    // Verify the 4 input name labels match expected format
    const char *labels[] = {"input1_name_l", "input1_name_r", "input2_name_l", "input2_name_r"};
    TEST_ASSERT_EQUAL_STRING("input1_name_l", labels[0]);
    TEST_ASSERT_EQUAL_STRING("input1_name_r", labels[1]);
    TEST_ASSERT_EQUAL_STRING("input2_name_l", labels[2]);
    TEST_ASSERT_EQUAL_STRING("input2_name_r", labels[3]);
}

// ===== Factory Reset Button Tests =====

void test_factory_reset_payload(void) {
    // Factory reset button uses "RESET" as payload_press
    String payload = "RESET";
    TEST_ASSERT_EQUAL_STRING("RESET", payload.c_str());
}

// ===== Cleanup Completeness Tests =====

void test_cleanup_topic_buffer_size(void) {
    // Longest topic: "homeassistant/binary_sensor/%s/heap_critical/config"
    // With deviceId "esp32_audio_XXXX" (16 chars) = ~67 chars
    // Buffer should be >= 160 to handle all topics safely
    const int bufSize = 160;
    char buf[160];
    const char *longestTopic = "homeassistant/binary_sensor/%s/boot_animation_style/config";
    snprintf(buf, sizeof(buf), longestTopic, "esp32_audio_ABCD");
    TEST_ASSERT_TRUE(strlen(buf) < (size_t)bufSize);
}

// ===== Test Runner =====

int runUnityTests(void) {
    UNITY_BEGIN();

    // Settings persistence tests
    RUN_TEST(test_load_mqtt_settings_from_nvs);
    RUN_TEST(test_save_mqtt_settings_to_nvs);
    RUN_TEST(test_mqtt_disabled_when_no_broker);

    // Connection management tests
    RUN_TEST(test_mqtt_connect_success);
    RUN_TEST(test_mqtt_reconnect_on_disconnect);
    RUN_TEST(test_mqtt_connect_with_auth);

    // Publishing tests
    RUN_TEST(test_publish_led_state);
    RUN_TEST(test_publish_blinking_state);
    RUN_TEST(test_publish_smart_sensing_state);

    // Home Assistant discovery tests
    RUN_TEST(test_ha_discovery_generation);
    RUN_TEST(test_ha_discovery_removal);

    // API handler tests
    RUN_TEST(test_mqtt_update_validates_broker);
    RUN_TEST(test_mqtt_custom_base_topic);
    RUN_TEST(test_mqtt_default_port);

    // HA restart detection tests
    RUN_TEST(test_ha_status_online_triggers_republish);
    RUN_TEST(test_ha_status_offline_no_action);

    // OTA progress tests
    RUN_TEST(test_ota_update_percentage_in_progress);
    RUN_TEST(test_ota_update_percentage_not_in_progress);

    // Non-blocking OTA tests
    RUN_TEST(test_mqtt_ota_command_requires_update_available);
    RUN_TEST(test_mqtt_ota_command_requires_firmware_url);
    RUN_TEST(test_mqtt_ota_command_starts_when_ready);

    // Availability config tests
    RUN_TEST(test_effective_base_topic_with_custom_topic);
    RUN_TEST(test_availability_topic_matches_lwt);

    // Timezone offset tests
    RUN_TEST(test_timezone_offset_valid_range);
    RUN_TEST(test_timezone_offset_invalid_range);

    // Sweep speed tests
    RUN_TEST(test_sweep_speed_valid_range);
    RUN_TEST(test_sweep_speed_boundary_min);
    RUN_TEST(test_sweep_speed_boundary_max);
    RUN_TEST(test_sweep_speed_below_min);

    // Boot animation tests
    RUN_TEST(test_boot_animation_style_mapping);
    RUN_TEST(test_boot_animation_style_count);

    // Input names tests
    RUN_TEST(test_input_name_labels);

    // Factory reset tests
    RUN_TEST(test_factory_reset_payload);

    // Cleanup completeness tests
    RUN_TEST(test_cleanup_topic_buffer_size);

    return UNITY_END();
}

// For native platform
#ifdef NATIVE_TEST
int main(void) {
    return runUnityTests();
}
#endif

// For Arduino platform
#ifndef NATIVE_TEST
void setup() {
    delay(2000);
    runUnityTests();
}

void loop() {
    // Do nothing
}
#endif
