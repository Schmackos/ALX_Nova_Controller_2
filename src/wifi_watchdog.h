#ifndef WIFI_WATCHDOG_H
#define WIFI_WATCHDOG_H

// ===== WiFi RX Watchdog =====
// When ESP32 internal SRAM heap drops below ~40KB, WiFi RX buffer allocations fail.
// Incoming packets (ping, HTTP, WebSocket) are silently dropped even though outgoing
// MQTT publishes still work. The fix: if the heap has been critical for >2 minutes,
// force a WiFi disconnect so wifi_manager can reconnect and flush stale RX buffers.
//
// This is a pure function — no hardware dependencies — so tests can include this
// header directly without any Arduino/ESP32 mock overhead.

// Returns true when the WiFi stack should be force-disconnected and reconnected.
//
// Parameters:
//   heapCritical        - true when ESP.getMaxAllocHeap() < HEAP_CRITICAL_THRESHOLD_BYTES
//   wifiConnected       - true when WiFi.status() == WL_CONNECTED
//   otaInProgress       - true during an active OTA download (must not disrupt WiFi)
//   criticalDurationMs  - how long the heap has been continuously critical (millis delta)
//
// Rules:
//   - Heap not critical          -> false (no action needed)
//   - WiFi not connected         -> false (nothing to reconnect)
//   - OTA in progress            -> false (never disconnect during firmware download)
//   - Critical < 120 000 ms      -> false (give the system time to self-recover)
//   - All other conditions       -> true  (reconnect to flush stale RX buffers)
inline bool wifi_watchdog_should_reconnect(bool heapCritical,
                                           bool wifiConnected,
                                           bool otaInProgress,
                                           unsigned long criticalDurationMs) {
    if (!heapCritical)    return false;
    if (!wifiConnected)   return false;
    if (otaInProgress)    return false;
    return criticalDurationMs >= 120000UL;  // 2 minutes
}

#endif // WIFI_WATCHDOG_H
