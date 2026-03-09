#ifndef STATE_OTA_STATE_H
#define STATE_OTA_STATE_H

#include "config.h"

// Maximum number of cached OTA release entries
static const int OTA_MAX_RELEASES = 5;

// Information about a single firmware release
struct ReleaseInfo {
  String version;
  String firmwareUrl;
  String checksum;
  bool isPrerelease = false;
  String publishedAt;  // "YYYY-MM-DD"
};

// All OTA-related state fields
struct OtaState {
  unsigned long lastCheck = 0;
  bool inProgress = false;
  int progress = 0;
  String status = "idle";
  String statusMessage = "idle";
  int progressBytes = 0;
  int totalBytes = 0;
  bool autoUpdateEnabled = false;
  String cachedFirmwareUrl;
  String cachedChecksum;
  bool updateAvailable = false;
  String cachedLatestVersion;
  unsigned long updateDiscoveredTime = 0;

  // Release channel: 0 = stable (latest non-prerelease), 1 = beta (includes prereleases)
  uint8_t channel = 0;

  // Release list cache (on-demand, populated by /api/releases)
  ReleaseInfo cachedReleaseList[OTA_MAX_RELEASES];
  int cachedReleaseListCount = 0;

  // Just-updated state
  bool justUpdated = false;
  String previousFirmwareVersion;
};

#endif // STATE_OTA_STATE_H
