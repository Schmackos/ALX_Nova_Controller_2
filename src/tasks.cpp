#include "tasks.h"
#include "app_state.h"
#include "debug_serial.h"
#include "mqtt_handler.h"
#include "ota_updater.h"
#include "smart_sensing.h"
#include "websocket_handler.h"
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <esp_task_wdt.h>


// ===== Task Handles =====
TaskHandle_t sensingTaskHandle = NULL;
TaskHandle_t webServerTaskHandle = NULL;
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t otaTaskHandle = NULL;

// ===== Event Queue =====
QueueHandle_t eventQueue = NULL;
static const int EVENT_QUEUE_SIZE = 20;

// ===== State Mutex =====
SemaphoreHandle_t stateMutex = NULL;

// External references
extern WebServer server;
extern WebSocketsServer webSocket;

// ===== Initialization =====
void initTasks() {
  LOG_I("Initializing FreeRTOS tasks...");

  // Create event queue
  eventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(TaskEvent));
  if (eventQueue == NULL) {
    LOG_E("Failed to create event queue!");
    return;
  }

  // Create state mutex
  stateMutex = xSemaphoreCreateMutex();
  if (stateMutex == NULL) {
    LOG_E("Failed to create state mutex!");
    return;
  }

  // Create tasks pinned to specific cores
  // Core 0: Sensing (high priority, time-critical)
  // Core 1: Web/MQTT/OTA (I/O bound)

  xTaskCreatePinnedToCore(smartSensingTask, "Sensing", TASK_STACK_SIZE_SENSING,
                          NULL, TASK_PRIORITY_SENSING, &sensingTaskHandle,
                          0 // Core 0
  );

  xTaskCreatePinnedToCore(webServerTask, "WebServer", TASK_STACK_SIZE_WEB, NULL,
                          TASK_PRIORITY_WEB, &webServerTaskHandle,
                          1 // Core 1
  );

  xTaskCreatePinnedToCore(mqttTask, "MQTT", TASK_STACK_SIZE_MQTT, NULL,
                          TASK_PRIORITY_MQTT, &mqttTaskHandle,
                          1 // Core 1
  );

  xTaskCreatePinnedToCore(otaCheckTask, "OTA", TASK_STACK_SIZE_OTA, NULL,
                          TASK_PRIORITY_OTA, &otaTaskHandle,
                          1 // Core 1
  );

  LOG_I("FreeRTOS tasks initialized successfully");
}

// ===== Smart Sensing Task (Core 0) =====
void smartSensingTask(void *parameter) {
  LOG_I("Smart Sensing task started on Core %d", xPortGetCoreID());

  // Add task to watchdog
  esp_task_wdt_add(NULL);

  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t taskPeriod = pdMS_TO_TICKS(100); // 100ms update rate

  while (true) {
    // Reset watchdog
    esp_task_wdt_reset();

    // Update smart sensing logic (thread-safe)
    {
      StateLock lock;
      updateSmartSensingLogic();
    }

    // Wait for next cycle
    vTaskDelayUntil(&lastWakeTime, taskPeriod);
  }
}

// ===== Web Server Task (Core 1) =====
void webServerTask(void *parameter) {
  LOG_I("Web Server task started on Core %d", xPortGetCoreID());

  // Add task to watchdog
  esp_task_wdt_add(NULL);

  while (true) {
    // Reset watchdog
    esp_task_wdt_reset();

    // Handle web server and WebSocket
    server.handleClient();
    webSocket.loop();

    // Broadcast states periodically
    static unsigned long lastBroadcast = 0;
    if (millis() - lastBroadcast >= 1000) {
      lastBroadcast = millis();
      StateLock lock;
      sendSmartSensingState();
    }

    // Hardware stats broadcast
    static unsigned long lastHwStats = 0;
    if (millis() - lastHwStats >= appState.hardwareStatsInterval) {
      lastHwStats = millis();
      sendHardwareStats();
    }

    // Short delay to yield to other tasks
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ===== MQTT Task (Core 1) =====
void mqttTask(void *parameter) {
  LOG_I("MQTT task started on Core %d", xPortGetCoreID());

  // Add task to watchdog
  esp_task_wdt_add(NULL);

  while (true) {
    // Reset watchdog
    esp_task_wdt_reset();

    // Handle MQTT loop
    {
      StateLock lock;
      mqttLoop();
    }

    // Short delay
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ===== OTA Check Task (Core 1) =====
void otaCheckTask(void *parameter) {
  LOG_I("OTA Check task started on Core %d", xPortGetCoreID());

  // Add task to watchdog
  esp_task_wdt_add(NULL);

  while (true) {
    // Reset watchdog
    esp_task_wdt_reset();

    // Check for updates periodically
    bool shouldCheck = false;
    {
      StateLock lock;
      if (!appState.isAPMode && WiFi.status() == WL_CONNECTED &&
          !appState.otaInProgress) {
        if (millis() - appState.lastOTACheck >= OTA_CHECK_INTERVAL ||
            appState.lastOTACheck == 0) {
          appState.lastOTACheck = millis();
          shouldCheck = true;
        }
      }
    }

    if (shouldCheck) {
      checkForFirmwareUpdate();
    }

    // Long delay - OTA checks are infrequent
    vTaskDelay(pdMS_TO_TICKS(10000)); // Check every 10 seconds
  }
}

// ===== Event Helpers =====
void sendEvent(EventType type, uint32_t data) {
  TaskEvent event = {type, data};
  xQueueSend(eventQueue, &event, 0);
}

bool receiveEvent(TaskEvent &event, TickType_t timeout) {
  return xQueueReceive(eventQueue, &event, timeout) == pdTRUE;
}

// ===== State Access Helpers =====
void lockState() {
  if (stateMutex != NULL) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
  }
}

void unlockState() {
  if (stateMutex != NULL) {
    xSemaphoreGive(stateMutex);
  }
}
