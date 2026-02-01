#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <time.h>

/**
 * Compare semantic version strings like "1.0.7" and "1.1.2"
 * Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
 */
int compareVersions(const String &v1, const String &v2);

/**
 * Convert RSSI to signal quality percentage (0-100%)
 */
int rssiToQuality(int rssi);

/**
 * Get human-readable reset reason
 */
String getResetReasonString();

/**
 * Synchronize system time with NTP servers
 */
void syncTimeWithNTP();

#endif // UTILS_H
