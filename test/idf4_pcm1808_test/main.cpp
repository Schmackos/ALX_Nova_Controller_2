// test/idf4_pcm1808_test/main.cpp
//
// PCM1808 → PCM5102A passthrough using the IDF4 legacy I2S API (<driver/i2s.h>).
// Configuration EXACTLY matches firmware v1.8.5 (last confirmed-working release).
//
// v1.8.5 key fields reproduced here:
//   use_apll  = true            (ESP32-S3 has no APLL → driver falls back to PLL_F160M,
//                                but fixed_mclk influences divider selection)
//   fixed_mclk = 48000 * 256   (explicitly request 12.288 MHz MCLK)
//   Single i2s_set_pin() call with mck_io_num set — no TX/RX init ordering issue.
//
// Diagnostic purpose:
//   If THIS sketch captures music → the IDF5 driver has an issue causing the PCM1808
//   analog input to be unreachable.
//   If THIS sketch also fails → confirm fixed_mclk is not the difference and look
//   for hardware wiring problems independent of IDF version.
//
// Build:  pio run -e idf4_pcm1808_test
// Upload: pio run -e idf4_pcm1808_test -t upload
// Monitor: pio device monitor

#include <Arduino.h>
#include <driver/i2s.h>   // IDF4 legacy API (deprecated in IDF5 but still present)
#include <math.h>
#include <cstring>

// ===== Hardware pins (identical to main firmware build flags) =====
#define PIN_BCK   16   // I2S_BCK_PIN
#define PIN_WS    18   // I2S_LRC_PIN
#define PIN_MCLK   3   // I2S_MCLK_PIN (SCKI to PCM1808)
#define PIN_DIN   17   // I2S_DOUT_PIN (PCM1808 DOUT → ESP32 DIN)
#define PIN_DOUT  40   // I2S_TX_DATA_PIN (ESP32 DOUT → PCM5102A DIN)

// ===== I2S parameters =====
#define SAMPLE_RATE    48000
#define DMA_BUF_COUNT  8
#define DMA_BUF_LEN    256
#define BUF_FRAMES     256

// ===== Buffers (internal SRAM) =====
static int32_t s_buf[BUF_FRAMES * 2];

// ===== Diagnostics =====
static bool     s_ok       = false;
static uint32_t s_totalReads = 0;
static uint32_t s_timeouts   = 0;
static int32_t  s_maxAbsWin  = 0;

// ---------------------------------------------------------------------------

static float sample_to_dbfs(int32_t raw) {
    int32_t s24 = raw >> 8;
    float lin = fabsf((float)s24 / 8388607.0f);
    if (lin < 1.0e-6f) return -120.0f;
    return 20.0f * log10f(lin);
}

// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== PCM1808 Passthrough Test (IDF4 v1.8.5 config) ===");
    Serial.printf("Pins: BCK=%d  WS=%d  MCLK=%d  DIN=%d  DOUT=%d\n",
                  PIN_BCK, PIN_WS, PIN_MCLK, PIN_DIN, PIN_DOUT);
    Serial.printf("Rate: %u Hz   MCLK: %u Hz (256fs)\n",
                  SAMPLE_RATE, SAMPLE_RATE * 256u);

    // --- IDF4 legacy I2S config — exact v1.8.5 reproduction ---
    // Full-duplex master: TX (DAC) + RX (ADC) on I2S_NUM_0.
    // use_apll=true + fixed_mclk: v1.8.5 requested APLL for cleaner MCLK.
    // ESP32-S3 has no APLL so the driver falls back to PLL_F160M, but fixed_mclk
    // (when non-zero) still guides the PLL divider calculation for the requested frequency.
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = 0,
        .dma_buf_count        = DMA_BUF_COUNT,
        .dma_buf_len          = DMA_BUF_LEN,
        .use_apll             = true,              // v1.8.5: true (APLL requested)
        .tx_desc_auto_clear   = true,
        .fixed_mclk           = SAMPLE_RATE * 256, // v1.8.5: 48000*256 = 12.288 MHz
        .mclk_multiple        = I2S_MCLK_MULTIPLE_256,
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[FAIL] i2s_driver_install: %s\n", esp_err_to_name(err));
        return;
    }

    // --- Single pin config call — MCLK, BCK, WS, TX data out, RX data in ---
    // All pins set atomically. No TX/RX init ordering issue possible.
    i2s_pin_config_t pins = {
        .mck_io_num   = PIN_MCLK,  // MCLK output → PCM1808 SCKI (GPIO3)
        .bck_io_num   = PIN_BCK,   // BCK output (GPIO16)
        .ws_io_num    = PIN_WS,    // WS/LRCK output (GPIO18)
        .data_out_num = PIN_DOUT,  // DAC data out (GPIO40)
        .data_in_num  = PIN_DIN,   // ADC1 data in (GPIO17)
    };

    err = i2s_set_pin(I2S_NUM_0, &pins);
    if (err != ESP_OK) {
        Serial.printf("[FAIL] i2s_set_pin: %s\n", esp_err_to_name(err));
        return;
    }

    s_ok = true;
    Serial.println("[OK]  I2S ready — waiting 1s for PCM1808 PLL...");
    delay(1000);  // PCM1808 PLL acquisition: 2048 LRCK cycles = ~43 ms at 48 kHz
    Serial.println("[OK]  Passthrough active — play audio into PCM1808");
    Serial.println("      Format: [time] peak_dBFS  maxAbs  timeouts/total");
    Serial.println("---------------------------------------------------");
}

// ---------------------------------------------------------------------------

void loop() {
    if (!s_ok) { delay(1000); return; }

    // Read ADC (PCM1808) — 50ms timeout
    size_t bytes_read = 0;
    esp_err_t err = i2s_read(I2S_NUM_0, s_buf, sizeof(s_buf),
                             &bytes_read, pdMS_TO_TICKS(50));
    s_totalReads++;

    if (err == ESP_ERR_TIMEOUT || bytes_read == 0) {
        s_timeouts++;
        memset(s_buf, 0, sizeof(s_buf));
    }

    // Track peak
    for (int i = 0; i < BUF_FRAMES * 2; i++) {
        int32_t abs_val = (s_buf[i] < 0) ? -s_buf[i] : s_buf[i];
        if (abs_val > s_maxAbsWin) s_maxAbsWin = abs_val;
    }

    // Passthrough: write ADC buffer to DAC
    size_t bytes_written = 0;
    i2s_write(I2S_NUM_0, s_buf, bytes_read ? bytes_read : sizeof(s_buf),
              &bytes_written, pdMS_TO_TICKS(20));

    // Print every ~1 second
    static const uint32_t PRINT_EVERY = 188;
    if (s_totalReads % PRINT_EVERY == 0) {
        float db = sample_to_dbfs(s_maxAbsWin);
        Serial.printf("[%3us] peak=%6.1f dBFS   maxAbs=0x%08X   timeout=%u/%u\n",
                      (unsigned)(millis() / 1000), db,
                      (unsigned)s_maxAbsWin, s_timeouts, s_totalReads);
        Serial.printf("       raw[0..7]: %08X %08X %08X %08X  %08X %08X %08X %08X\n",
                      s_buf[0], s_buf[1], s_buf[2], s_buf[3],
                      s_buf[4], s_buf[5], s_buf[6], s_buf[7]);
        s_maxAbsWin = 0;
    }
}
