/*
 * ESP32-C6 Hosted Firmware Update
 *
 * Downloads the esp-hosted C6 co-processor firmware from Espressif and
 * flashes it to the ESP32-C6 WiFi module via SDIO using the hosted OTA API.
 *
 * Uses Ethernet for the download (internet access) and WiFi mode init to
 * prime the hosted stack before calling hostedBeginUpdate().
 *
 * Build: pio run -e p4_hosted_update --target upload
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include "esp32-hal-hosted.h"

// Firmware URL printed in boot log: "Update URL: .../esp32c6-v2.11.6.bin"
#define FIRMWARE_URL "https://espressif.github.io/arduino-esp32/hosted/esp32c6-v2.11.6.bin"

#define CHUNK_SIZE      512   // Small chunks — old C6 firmware OTA RPC times out on large writes
#define WRITE_DELAY_MS    5   // Pause between writes so C6 flash has time to complete
#define ETH_TIMEOUT     20000 // ms to wait for Ethernet IP
#define WIFI_INIT_DELAY  4000 // ms to allow esp-hosted to initialise

static volatile bool s_ethGotIP = false;

static void onNetEvent(arduino_event_id_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            Serial.println("[Update] ETH started");
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            Serial.printf("[Update] ETH link up (%dMbps)\n", ETH.linkSpeed());
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            Serial.printf("[Update] ETH IP: %s\n", ETH.localIP().toString().c_str());
            s_ethGotIP = true;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Serial.println("[Update] ETH link down");
            break;
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\n[Update] ===== ESP32-C6 Hosted Firmware Update =====");

    // --- Step 1: Ethernet → internet access --------------------------------
    Network.onEvent(onNetEvent);
    if (!ETH.begin()) {
        Serial.println("[Update] ERROR: ETH.begin() failed — aborting");
        return;
    }
    Serial.printf("[Update] Waiting for Ethernet IP (up to %ds)...\n", ETH_TIMEOUT / 1000);
    uint32_t t = millis();
    while (!s_ethGotIP && millis() - t < ETH_TIMEOUT) {
        delay(200);
        Serial.print(".");
    }
    Serial.println();
    if (!s_ethGotIP) {
        Serial.println("[Update] ERROR: No Ethernet IP — plug in RJ45 cable and retry");
        return;
    }

    // --- Step 2: Init WiFi hosted stack (sets hosted_initialized = true) ---
    Serial.println("[Update] Initialising WiFi hosted stack...");
    WiFi.mode(WIFI_STA);
    delay(WIFI_INIT_DELAY);

    // --- Step 3: Log current versions -------------------------------------
    uint32_t hMaj, hMin, hPatch, sMaj, sMin, sPatch;
    hostedGetHostVersion(&hMaj, &hMin, &hPatch);
    hostedGetSlaveVersion(&sMaj, &sMin, &sPatch);
    Serial.printf("[Update] Host  version: %lu.%lu.%lu\n", hMaj, hMin, hPatch);
    Serial.printf("[Update] Slave version: %lu.%lu.%lu\n", sMaj, sMin, sPatch);

    if (!hostedHasUpdate()) {
        Serial.println("[Update] hostedHasUpdate() = false (versions match or query failed)");
        Serial.println("[Update] Proceeding anyway with forced update...");
    }

    // --- Step 4: Download firmware over Ethernet ---------------------------
    Serial.printf("[Update] Downloading: %s\n", FIRMWARE_URL);

    NetworkClientSecure client;
    client.setInsecure();   // Skip TLS cert check — this is a one-time admin action
    client.setTimeout(30);

    HTTPClient http;
    http.begin(client, FIRMWARE_URL);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(30000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[Update] ERROR: HTTP %d — download failed\n", code);
        http.end();
        return;
    }

    int total = http.getSize();   // -1 = chunked/unknown
    Serial.printf("[Update] Firmware size: %d bytes\n", total);

    // --- Step 5: Begin hosted OTA ------------------------------------------
    if (!hostedBeginUpdate()) {
        Serial.println("[Update] ERROR: hostedBeginUpdate() failed");
        Serial.println("[Update]   (hosted stack not initialised — WiFi init may have failed)");
        http.end();
        return;
    }
    Serial.println("[Update] hostedBeginUpdate OK");

    // --- Step 6: Stream download → C6 -------------------------------------
    Stream *stream = http.getStreamPtr();
    uint8_t *buf = (uint8_t *)malloc(CHUNK_SIZE);
    if (!buf) {
        Serial.println("[Update] ERROR: malloc failed");
        hostedEndUpdate();
        http.end();
        return;
    }

    int written   = 0;
    int remaining = total;
    bool ok       = true;

    while (http.connected() && (remaining > 0 || total == -1)) {
        int avail = stream->available();
        if (avail > 0) {
            int n = stream->readBytes(buf, min(avail, CHUNK_SIZE));
            if (n > 0) {
                delay(WRITE_DELAY_MS);
                if (!hostedWriteUpdate(buf, n)) {
                    Serial.printf("[Update] ERROR: hostedWriteUpdate failed at offset %d\n", written);
                    ok = false;
                    break;
                }
                written    += n;
                if (total > 0) remaining -= n;

                // Progress bar every 32 KB
                if (written % (32 * 1024) < n) {
                    int pct = (total > 0) ? (written * 100 / total) : -1;
                    if (pct >= 0)
                        Serial.printf("[Update] %3d%%  %d / %d bytes\n", pct, written, total);
                    else
                        Serial.printf("[Update] %d bytes written\n", written);
                }
            }
        } else {
            delay(1);
        }
    }

    free(buf);
    http.end();

    if (!ok) {
        hostedEndUpdate();
        Serial.println("[Update] FAILED — C6 firmware not changed");
        return;
    }
    Serial.printf("[Update] Download complete: %d bytes written\n", written);

    // --- Step 7: Finalise and activate ------------------------------------
    if (!hostedEndUpdate()) {
        Serial.println("[Update] ERROR: hostedEndUpdate() failed");
        return;
    }
    Serial.println("[Update] hostedEndUpdate OK");

    Serial.println("[Update] Activating new C6 firmware (C6 will reboot)...");
    hostedActivateUpdate();

    Serial.println("[Update] ===== UPDATE COMPLETE =====");
    Serial.println("[Update] Rebooting ESP32-P4 in 3 s...");
    delay(3000);
    ESP.restart();
}

void loop() {}
