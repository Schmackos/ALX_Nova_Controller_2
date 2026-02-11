#include "utils.h"
#include "app_state.h"
#include "crash_log.h"
#include "debug_serial.h"
#include <esp_system.h>

int compareVersions(const String &v1, const String &v2) {
  int i = 0, j = 0;
  const int n1 = v1.length();
  const int n2 = v2.length();

  while (i < n1 || j < n2) {
    long num1 = 0;
    long num2 = 0;

    while (i < n1 && isDigit(v1[i])) {
      num1 = num1 * 10 + (v1[i] - '0');
      i++;
    }
    // Skip non-digit separators like '.'
    while (i < n1 && !isDigit(v1[i]))
      i++;

    while (j < n2 && isDigit(v2[j])) {
      num2 = num2 * 10 + (v2[j] - '0');
      j++;
    }
    while (j < n2 && !isDigit(v2[j]))
      j++;

    if (num1 < num2)
      return -1;
    if (num1 > num2)
      return 1;
  }

  return 0;
}

int rssiToQuality(int rssi) {
  if (rssi <= -100)
    return 0;
  if (rssi >= -50)
    return 100;
  return 2 * (rssi + 100); // Linear scale: -100dBm=0%, -50dBm=100%
}

String getResetReasonString() {
  esp_reset_reason_t reason = esp_reset_reason();
  switch (reason) {
  case ESP_RST_POWERON:
    return "power_on";
  case ESP_RST_EXT:
    return "external_reset";
  case ESP_RST_SW:
    return "software_reset";
  case ESP_RST_PANIC:
    return "exception_panic";
  case ESP_RST_INT_WDT:
    return "interrupt_watchdog";
  case ESP_RST_TASK_WDT:
    return "task_watchdog";
  case ESP_RST_WDT:
    return "other_watchdog";
  case ESP_RST_DEEPSLEEP:
    return "deep_sleep_wake";
  case ESP_RST_BROWNOUT:
    return "brownout";
  case ESP_RST_SDIO:
    return "sdio_reset";
  default:
    return "unknown";
  }
}

void syncTimeWithNTP() {
  LOG_I("[NTP] === Synchronizing Time with NTP ===");
  LOG_I("[NTP] Timezone offset: %d seconds (%.1f hours)",
        appState.timezoneOffset, appState.timezoneOffset / 3600.0);
  LOG_I("[NTP] DST offset: %d seconds (%.1f hours)",
        appState.dstOffset, appState.dstOffset / 3600.0);

  configTime(appState.timezoneOffset, appState.dstOffset, "pool.ntp.org", "time.nist.gov");

  LOG_I("[NTP] Waiting for NTP time sync...");
  time_t now = time(nullptr);
  int attempts = 0;

  while (now < 1000000000 && attempts < 20) {
    delay(500);
    now = time(nullptr);
    attempts++;
  }

  if (now < 1000000000) {
    LOG_W("[NTP] Failed to sync time with NTP server");
  } else {
    LOG_I("[NTP] Time synchronized successfully");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStr[32];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
      LOG_I("[NTP] Current local time: %s", timeStr);
    }
    // Backfill crash log timestamp now that NTP is available
    crashlog_update_timestamp();
  }
}
