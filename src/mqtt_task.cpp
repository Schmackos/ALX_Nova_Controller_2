#include "mqtt_task.h"
#include "mqtt_handler.h"
#include "app_state.h"
#include "globals.h"
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <WiFi.h>

// Forward declarations for Wave 2A additions (not yet in mqtt_handler.h)
void mqttPublishPendingState();
void mqttPublishHeartbeat();

static void mqtt_task_fn(void*) {
    esp_task_wdt_add(NULL);
    while (true) {
        esp_task_wdt_reset();

        if (!appState.mqtt.enabled || appState.mqtt.broker.length() == 0
                || WiFi.status() != WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (appState._mqttReconfigPending) {
            appState._mqttReconfigPending = false;
            mqttClient.disconnect();
            setupMqtt();
        }

        if (!mqttClient.connected()) {
            appState.mqtt.connected = false;
            mqttReconnect();
        }

        mqttClient.loop();

        mqttPublishPendingState();
        mqttPublishHeartbeat();

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void mqtt_task_init() {
    xTaskCreatePinnedToCore(mqtt_task_fn, "mqtt_task",
        TASK_STACK_SIZE_MQTT, NULL, TASK_PRIORITY_MQTT, NULL, 0);
}
