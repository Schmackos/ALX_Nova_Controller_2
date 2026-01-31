#ifndef TASKS_H
#define TASKS_H

#include "config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>


// ===== Task Handles =====
extern TaskHandle_t sensingTaskHandle;
extern TaskHandle_t webServerTaskHandle;
extern TaskHandle_t mqttTaskHandle;
extern TaskHandle_t otaTaskHandle;

// ===== Event Queue Types =====
enum EventType {
  EVENT_SIGNAL_DETECTED,
  EVENT_SIGNAL_LOST,
  EVENT_TIMER_EXPIRED,
  EVENT_MODE_CHANGE,
  EVENT_AMPLIFIER_ON,
  EVENT_AMPLIFIER_OFF,
  EVENT_WIFI_CONNECTED,
  EVENT_WIFI_DISCONNECTED,
  EVENT_MQTT_CONNECTED,
  EVENT_MQTT_DISCONNECTED,
  EVENT_OTA_AVAILABLE,
  EVENT_OTA_START,
  EVENT_OTA_COMPLETE,
  EVENT_OTA_FAILED,
  EVENT_BUTTON_PRESS
};

struct TaskEvent {
  EventType type;
  uint32_t data;
};

// ===== Event Queue =====
extern QueueHandle_t eventQueue;

// ===== Mutex for Shared State =====
extern SemaphoreHandle_t stateMutex;

// ===== Task Functions =====
void initTasks();
void smartSensingTask(void *parameter);
void webServerTask(void *parameter);
void mqttTask(void *parameter);
void otaCheckTask(void *parameter);

// ===== Event Helpers =====
void sendEvent(EventType type, uint32_t data = 0);
bool receiveEvent(TaskEvent &event, TickType_t timeout = 0);

// ===== State Access Helpers (thread-safe) =====
void lockState();
void unlockState();

// RAII-style state lock
class StateLock {
public:
  StateLock() { lockState(); }
  ~StateLock() { unlockState(); }
};

#endif // TASKS_H
