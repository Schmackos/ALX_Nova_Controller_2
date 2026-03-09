#include "utils.h"
#include "app_state.h"
#include "crash_log.h"
#include "debug_serial.h"
#include <esp_system.h>

int compareVersions(const String &v1, const String &v2) {
  // Split on "-beta." to separate base version from pre-release ordinal
  String base1 = v1, base2 = v2;
  int beta1 = 0, beta2 = 0;  // 0 = stable (wins over any beta)

  int idx1 = v1.indexOf("-beta.");
  if (idx1 >= 0) {
    base1 = v1.substring(0, idx1);
    beta1 = v1.substring(idx1 + 6).toInt();
    if (beta1 < 1) beta1 = 1;  // malformed beta still counts as pre-release
  }

  int idx2 = v2.indexOf("-beta.");
  if (idx2 >= 0) {
    base2 = v2.substring(0, idx2);
    beta2 = v2.substring(idx2 + 6).toInt();
    if (beta2 < 1) beta2 = 1;
  }

  // Compare base versions (MAJOR.MINOR.PATCH) numerically
  int i = 0, j = 0;
  const int n1 = base1.length();
  const int n2 = base2.length();

  while (i < n1 || j < n2) {
    long num1 = 0;
    long num2 = 0;

    while (i < n1 && isDigit(base1[i])) {
      num1 = num1 * 10 + (base1[i] - '0');
      i++;
    }
    while (i < n1 && !isDigit(base1[i]))
      i++;

    while (j < n2 && isDigit(base2[j])) {
      num2 = num2 * 10 + (base2[j] - '0');
      j++;
    }
    while (j < n2 && !isDigit(base2[j]))
      j++;

    if (num1 < num2)
      return -1;
    if (num1 > num2)
      return 1;
  }

  // Base versions equal — stable (beta=0) beats any beta
  if (beta1 == 0 && beta2 > 0) return 1;   // v1 is stable, v2 is beta
  if (beta1 > 0 && beta2 == 0) return -1;  // v1 is beta, v2 is stable
  // Both betas or both stable: compare ordinals
  if (beta1 < beta2) return -1;
  if (beta1 > beta2) return 1;
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
        appState.general.timezoneOffset, appState.general.timezoneOffset / 3600.0);
  LOG_I("[NTP] DST offset: %d seconds (%.1f hours)",
        appState.general.dstOffset, appState.general.dstOffset / 3600.0);

  configTime(appState.general.timezoneOffset, appState.general.dstOffset, "pool.ntp.org", "time.nist.gov");

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
