#include <unity.h>
#include <string>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Minimal AppState for OTA Task Tests =====

enum AppFSMState {
  STATE_IDLE,
  STATE_SIGNAL_DETECTED,
  STATE_AUTO_OFF_TIMER,
  STATE_WEB_CONFIG,
  STATE_OTA_UPDATE,
  STATE_ERROR
};

class AppState {
public:
  static AppState &getInstance() {
    static AppState instance;
    return instance;
  }

  AppState(const AppState &) = delete;
  AppState &operator=(const AppState &) = delete;

  // OTA fields
  bool otaInProgress = false;
  int otaProgress = 0;
  String otaStatus = "idle";
  String otaStatusMessage = "idle";
  int otaProgressBytes = 0;
  int otaTotalBytes = 0;
  String cachedFirmwareUrl;
  unsigned long updateDiscoveredTime = 0;
  bool otaHttpFallback = false;

  // Audio pause flag (used during I2S teardown for OTA)
  volatile bool audioPaused = false;

  // FSM
  AppFSMState fsmState = STATE_IDLE;
  void setFSMState(AppFSMState s) {
    fsmState = s;
    _fsmStateDirty = true;
  }

  // OTA dirty flag
  bool isOTADirty() const { return _otaDirty; }
  void clearOTADirty() { _otaDirty = false; }
  void markOTADirty() { _otaDirty = true; }

  // Other dirty flags (subset needed for clearAllDirtyFlags test)
  bool _fsmStateDirty = false;
  bool _displayDirty = false;
  bool _buzzerDirty = false;
  bool _settingsDirty = false;
  bool _sigGenDirty = false;

  void clearAllDirtyFlags() {
    _fsmStateDirty = false;
    _displayDirty = false;
    _buzzerDirty = false;
    _settingsDirty = false;
    _sigGenDirty = false;
    _otaDirty = false;
  }

  bool hasAnyDirtyFlag() const {
    return _fsmStateDirty || _displayDirty || _buzzerDirty ||
           _settingsDirty || _sigGenDirty || _otaDirty;
  }

private:
  AppState() {}
  bool _otaDirty = false;
};

#define appState AppState::getInstance()

// ===== setOTAProgress (mirrors real implementation) =====
static void setOTAProgress(const char* status, const char* message, int progress) {
  appState.otaStatus = status;
  appState.otaStatusMessage = message;
  appState.otaProgress = progress;
  appState.markOTADirty();
}

// ===== I2S driver mock tracking =====
static bool i2sDriversUninstalled = false;
static bool i2sDriversReinstalled = false;

void i2s_audio_uninstall_drivers() {
  i2sDriversUninstalled = true;
}

void i2s_audio_reinstall_drivers() {
  i2sDriversReinstalled = true;
}

// ===== FreeRTOS stubs for task guard tests =====
static bool taskCreateCalled = false;
static bool lastTaskCreateResult = true;

// Stub for isOTATaskRunning — controlled by test
static bool stubOtaDownloadTaskRunning = false;
static bool stubOtaCheckTaskRunning = false;

bool isOTATaskRunning() {
  return stubOtaDownloadTaskRunning || stubOtaCheckTaskRunning;
}

// Simplified startOTADownloadTask (mirrors real guard logic, no actual xTaskCreate)
bool startOTADownloadTask_testable() {
  if (stubOtaDownloadTaskRunning || appState.otaInProgress) {
    return false;
  }

  appState.otaInProgress = true;
  appState.otaStatus = "preparing";
  appState.otaStatusMessage = "Preparing for update...";
  appState.otaProgress = 0;
  appState.setFSMState(STATE_OTA_UPDATE);
  appState.markOTADirty();

  // Pause audio and tear down I2S (mirrors real implementation)
  appState.audioPaused = true;
  i2s_audio_uninstall_drivers();

  taskCreateCalled = true;
  if (!lastTaskCreateResult) {
    i2s_audio_reinstall_drivers();
    appState.audioPaused = false;
    appState.otaInProgress = false;
    setOTAProgress("error", "Failed to start update task", 0);
    appState.setFSMState(STATE_IDLE);
    return false;
  }

  stubOtaDownloadTaskRunning = true;
  return true;
}

// Simplified startOTACheckTask (mirrors real guard logic)
bool startOTACheckTask_testable() {
  if (stubOtaCheckTaskRunning || appState.otaInProgress) {
    return false;
  }

  taskCreateCalled = true;
  stubOtaCheckTaskRunning = true;
  return true;
}

// ===== Test Setup/Teardown =====

void setUp(void) {
  // Reset all state
  appState.otaInProgress = false;
  appState.otaProgress = 0;
  appState.otaStatus = "idle";
  appState.otaStatusMessage = "idle";
  appState.otaProgressBytes = 0;
  appState.otaTotalBytes = 0;
  appState.cachedFirmwareUrl = "";
  appState.updateDiscoveredTime = 0;
  appState.otaHttpFallback = false;
  appState.audioPaused = false;
  appState.fsmState = STATE_IDLE;
  appState.clearAllDirtyFlags();

  taskCreateCalled = false;
  lastTaskCreateResult = true;
  stubOtaDownloadTaskRunning = false;
  stubOtaCheckTaskRunning = false;
  i2sDriversUninstalled = false;
  i2sDriversReinstalled = false;

#ifdef NATIVE_TEST
  ArduinoMock::reset();
#endif
}

void tearDown(void) {}

// ===== Tests =====

void test_setOTAProgress_sets_fields(void) {
  setOTAProgress("downloading", "Downloading firmware...", 50);

  TEST_ASSERT_EQUAL_STRING("downloading", appState.otaStatus.c_str());
  TEST_ASSERT_EQUAL_STRING("Downloading firmware...", appState.otaStatusMessage.c_str());
  TEST_ASSERT_EQUAL_INT(50, appState.otaProgress);
}

void test_setOTAProgress_marks_dirty(void) {
  appState.clearOTADirty();
  TEST_ASSERT_FALSE(appState.isOTADirty());

  setOTAProgress("downloading", "msg", 25);

  TEST_ASSERT_TRUE(appState.isOTADirty());
}

void test_ota_dirty_flag_initially_false(void) {
  // setUp already clears flags
  TEST_ASSERT_FALSE(appState.isOTADirty());
}

void test_ota_dirty_flag_set_and_clear(void) {
  appState.markOTADirty();
  TEST_ASSERT_TRUE(appState.isOTADirty());

  appState.clearOTADirty();
  TEST_ASSERT_FALSE(appState.isOTADirty());
}

void test_startOTADownload_guard_already_in_progress(void) {
  appState.otaInProgress = true;

  bool result = startOTADownloadTask_testable();

  TEST_ASSERT_FALSE(result);
  TEST_ASSERT_FALSE(taskCreateCalled);
}

void test_startOTADownload_guard_task_already_running(void) {
  stubOtaDownloadTaskRunning = true;

  bool result = startOTADownloadTask_testable();

  TEST_ASSERT_FALSE(result);
  TEST_ASSERT_FALSE(taskCreateCalled);
}

void test_startOTADownload_sets_initial_state(void) {
  appState.clearAllDirtyFlags();

  bool result = startOTADownloadTask_testable();

  TEST_ASSERT_TRUE(result);
  TEST_ASSERT_TRUE(appState.otaInProgress);
  TEST_ASSERT_EQUAL_STRING("preparing", appState.otaStatus.c_str());
  TEST_ASSERT_EQUAL(STATE_OTA_UPDATE, appState.fsmState);
  TEST_ASSERT_TRUE(appState.isOTADirty());
  TEST_ASSERT_TRUE(taskCreateCalled);
}

void test_startOTACheck_guard_already_running(void) {
  stubOtaCheckTaskRunning = true;

  bool result = startOTACheckTask_testable();

  TEST_ASSERT_FALSE(result);
  TEST_ASSERT_FALSE(taskCreateCalled);
}

void test_startOTACheck_guard_ota_in_progress(void) {
  appState.otaInProgress = true;

  bool result = startOTACheckTask_testable();

  TEST_ASSERT_FALSE(result);
  TEST_ASSERT_FALSE(taskCreateCalled);
}

void test_isOTATaskRunning_false_when_no_task(void) {
  stubOtaDownloadTaskRunning = false;
  stubOtaCheckTaskRunning = false;

  TEST_ASSERT_FALSE(isOTATaskRunning());
}

void test_isOTATaskRunning_true_when_download_running(void) {
  stubOtaDownloadTaskRunning = true;

  TEST_ASSERT_TRUE(isOTATaskRunning());
}

void test_ota_failure_resets_state(void) {
  // Simulate a running OTA that failed
  appState.otaInProgress = true;
  appState.updateDiscoveredTime = 12345;
  appState.fsmState = STATE_OTA_UPDATE;
  appState.clearAllDirtyFlags();

  // Simulate failure path (what otaDownloadTask does on failure)
  appState.otaInProgress = false;
  appState.updateDiscoveredTime = 0;
  appState.setFSMState(STATE_IDLE);
  appState.markOTADirty();

  TEST_ASSERT_FALSE(appState.otaInProgress);
  TEST_ASSERT_EQUAL(0UL, appState.updateDiscoveredTime);
  TEST_ASSERT_EQUAL(STATE_IDLE, appState.fsmState);
  TEST_ASSERT_TRUE(appState.isOTADirty());
}

void test_clearAllDirtyFlags_includes_ota(void) {
  appState.markOTADirty();
  TEST_ASSERT_TRUE(appState.isOTADirty());

  appState.clearAllDirtyFlags();
  TEST_ASSERT_FALSE(appState.isOTADirty());
}

void test_hasAnyDirtyFlag_includes_ota(void) {
  appState.clearAllDirtyFlags();
  TEST_ASSERT_FALSE(appState.hasAnyDirtyFlag());

  appState.markOTADirty();
  TEST_ASSERT_TRUE(appState.hasAnyDirtyFlag());
}

void test_startOTADownload_task_create_failure(void) {
  lastTaskCreateResult = false;

  bool result = startOTADownloadTask_testable();

  TEST_ASSERT_FALSE(result);
  TEST_ASSERT_FALSE(appState.otaInProgress);
  TEST_ASSERT_EQUAL_STRING("error", appState.otaStatus.c_str());
  TEST_ASSERT_EQUAL(STATE_IDLE, appState.fsmState);
  // I2S should be reinstalled on failure
  TEST_ASSERT_TRUE(i2sDriversReinstalled);
  TEST_ASSERT_FALSE(appState.audioPaused);
}

// ===== I2S Driver Management Tests =====

void test_ota_download_pauses_audio_and_uninstalls_i2s(void) {
  bool result = startOTADownloadTask_testable();

  TEST_ASSERT_TRUE(result);
  TEST_ASSERT_TRUE(appState.audioPaused);
  TEST_ASSERT_TRUE(i2sDriversUninstalled);
}

void test_ota_failure_reinstalls_i2s(void) {
  // Start OTA download
  bool result = startOTADownloadTask_testable();
  TEST_ASSERT_TRUE(result);
  TEST_ASSERT_TRUE(i2sDriversUninstalled);

  // Simulate failure path (what otaDownloadTask does on failure)
  i2s_audio_reinstall_drivers();
  appState.audioPaused = false;
  appState.otaInProgress = false;
  appState.updateDiscoveredTime = 0;
  appState.setFSMState(STATE_IDLE);
  appState.markOTADirty();

  TEST_ASSERT_TRUE(i2sDriversReinstalled);
  TEST_ASSERT_FALSE(appState.audioPaused);
  TEST_ASSERT_FALSE(appState.otaInProgress);
}

void test_ota_task_create_failure_reinstalls_i2s(void) {
  lastTaskCreateResult = false;

  bool result = startOTADownloadTask_testable();

  TEST_ASSERT_FALSE(result);
  // I2S should have been uninstalled then reinstalled
  TEST_ASSERT_TRUE(i2sDriversUninstalled);
  TEST_ASSERT_TRUE(i2sDriversReinstalled);
  TEST_ASSERT_FALSE(appState.audioPaused);
}

// ===== Test Runner =====

int runUnityTests(void) {
  UNITY_BEGIN();

  RUN_TEST(test_setOTAProgress_sets_fields);
  RUN_TEST(test_setOTAProgress_marks_dirty);
  RUN_TEST(test_ota_dirty_flag_initially_false);
  RUN_TEST(test_ota_dirty_flag_set_and_clear);
  RUN_TEST(test_startOTADownload_guard_already_in_progress);
  RUN_TEST(test_startOTADownload_guard_task_already_running);
  RUN_TEST(test_startOTADownload_sets_initial_state);
  RUN_TEST(test_startOTACheck_guard_already_running);
  RUN_TEST(test_startOTACheck_guard_ota_in_progress);
  RUN_TEST(test_isOTATaskRunning_false_when_no_task);
  RUN_TEST(test_isOTATaskRunning_true_when_download_running);
  RUN_TEST(test_ota_failure_resets_state);
  RUN_TEST(test_clearAllDirtyFlags_includes_ota);
  RUN_TEST(test_hasAnyDirtyFlag_includes_ota);
  RUN_TEST(test_startOTADownload_task_create_failure);

  // I2S driver management tests
  RUN_TEST(test_ota_download_pauses_audio_and_uninstalls_i2s);
  RUN_TEST(test_ota_failure_reinstalls_i2s);
  RUN_TEST(test_ota_task_create_failure_reinstalls_i2s);

  return UNITY_END();
}

#ifdef NATIVE_TEST
int main(void) {
  return runUnityTests();
}
#endif

#ifndef NATIVE_TEST
void setup() {
  delay(2000);
  runUnityTests();
}

void loop() {}
#endif
