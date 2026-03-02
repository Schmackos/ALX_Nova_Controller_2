// test/idf5_pcm1808_test/main.cpp
//
// Minimal PCM1808 → PCM5102A passthrough test.
// Reads ADC samples, writes to DAC, prints peak levels every second.
// No project dependencies: no WiFi, no DSP, no LVGL, no PSRAM, no tasks.
//
// Purpose: isolate whether PCM1808 ADC audio capture works with the IDF5
// I2S driver independent of the full project. If audio passes through here
// but not in the main firmware, the issue is project-level interference.
//
// I2S init order matches official Espressif i2s_es8311 example (IDF v5.5.3):
//   1. TX first  — full config including MCLK=GPIO3
//   2. RX second — full config including MCLK=GPIO3 (re-routes to same signal, no clearing)
//   3. Enable TX → Enable RX (no delay between them)
//
// Build:  pio run -e idf5_pcm1808_test
// Upload: pio run -e idf5_pcm1808_test -t upload
// Monitor: pio device monitor

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <math.h>
#include <cstring>

// ===== Hardware pins (identical to main firmware build flags) =====
static const gpio_num_t PIN_BCK  = GPIO_NUM_16;  // I2S_BCK_PIN
static const gpio_num_t PIN_WS   = GPIO_NUM_18;  // I2S_LRC_PIN
static const gpio_num_t PIN_MCLK = GPIO_NUM_3;   // I2S_MCLK_PIN  (SCKI to PCM1808)
static const gpio_num_t PIN_DIN  = GPIO_NUM_17;  // I2S_DOUT_PIN  (PCM1808 DOUT → ESP32 DIN)
static const gpio_num_t PIN_DOUT = GPIO_NUM_40;  // I2S_TX_DATA_PIN (ESP32 DOUT → PCM5102A DIN)

// ===== I2S parameters =====
static const uint32_t SAMPLE_RATE  = 48000;
static const int DMA_DESC_NUM      = 8;    // DMA descriptor count
static const int DMA_FRAME_NUM     = 256;  // Frames per DMA buffer
static const int BUF_FRAMES        = 256;  // Process frames per loop iteration

// ===== Buffers (internal SRAM — DMA cannot use PSRAM) =====
static int32_t s_rxbuf[BUF_FRAMES * 2];  // Interleaved L/R from ADC

// ===== I2S channel handles =====
static i2s_chan_handle_t s_tx = nullptr;
static i2s_chan_handle_t s_rx = nullptr;
static bool s_ok = false;

// ===== Diagnostics =====
static uint32_t s_totalReads = 0;
static uint32_t s_timeouts   = 0;
static int32_t  s_maxAbsWin  = 0;  // Peak abs value in current ~1s window

// ---------------------------------------------------------------------------

// Convert left-justified 24-bit int32 sample to dBFS.
// PCM1808 format: data in bits [31:8], bits [7:0] = 0x00.
static float sample_to_dbfs(int32_t raw) {
    int32_t s24 = raw >> 8;  // Right-justify to 24-bit signed
    float lin = fabsf((float)s24 / 8388607.0f);
    if (lin < 1.0e-6f) return -120.0f;
    return 20.0f * log10f(lin);
}

// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== PCM1808 -> PCM5102A Passthrough Test ===");
    Serial.printf("Pins: BCK=%d  WS=%d  MCLK=%d  DIN=%d  DOUT=%d\n",
                  PIN_BCK, PIN_WS, PIN_MCLK, PIN_DIN, PIN_DOUT);
    Serial.printf("Rate: %u Hz   MCLK: %lu Hz (256fs)\n",
                  SAMPLE_RATE, (unsigned long)SAMPLE_RATE * 256UL);
    Serial.println("--------------------------------------------");

    // --- Allocate full-duplex channel pair on I2S_NUM_0 ---
    i2s_chan_config_t ch_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ch_cfg.dma_desc_num  = DMA_DESC_NUM;
    ch_cfg.dma_frame_num = DMA_FRAME_NUM;
    ch_cfg.auto_clear    = true;  // TX: fill zeros on underrun (silence during delays)

    esp_err_t err = i2s_new_channel(&ch_cfg, &s_tx, &s_rx);
    if (err != ESP_OK) {
        Serial.printf("[FAIL] i2s_new_channel: %s\n", esp_err_to_name(err));
        return;
    }

    // --- TX config: full clock master (MCLK + BCK + WS outputs, DOUT to DAC) ---
    i2s_std_config_t tx_cfg = {};
    tx_cfg.clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE);
    tx_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                          I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    tx_cfg.gpio_cfg.mclk = PIN_MCLK;       // MCLK output → PCM1808 SCKI (GPIO3)
    tx_cfg.gpio_cfg.bclk = PIN_BCK;        // BCK output (GPIO16)
    tx_cfg.gpio_cfg.ws   = PIN_WS;         // WS/LRCK output (GPIO18)
    tx_cfg.gpio_cfg.dout = PIN_DOUT;       // Data output → PCM5102A DIN (GPIO40)
    tx_cfg.gpio_cfg.din  = I2S_GPIO_UNUSED;
    tx_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    tx_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    tx_cfg.gpio_cfg.invert_flags.ws_inv   = false;

    // --- RX config: same as TX but with DIN set and DOUT unused ---
    // MCLK=GPIO3 in both configs: RX init re-routes GPIO3 to same MCLK signal (no clearing).
    // This matches the official Espressif i2s_es8311 example init pattern.
    i2s_std_config_t rx_cfg = tx_cfg;
    rx_cfg.gpio_cfg.mclk = PIN_MCLK;       // Same as TX — routes GPIO3 to MCLK (no change)
    rx_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    rx_cfg.gpio_cfg.din  = PIN_DIN;         // ADC1 DOUT → ESP32 DIN (GPIO17)

    // TX first (official Espressif order), then RX — both with MCLK=GPIO3.
    err = i2s_channel_init_std_mode(s_tx, &tx_cfg);
    if (err != ESP_OK) {
        Serial.printf("[FAIL] TX i2s_channel_init_std_mode: %s\n", esp_err_to_name(err));
        return;
    }

    err = i2s_channel_init_std_mode(s_rx, &rx_cfg);
    if (err != ESP_OK) {
        Serial.printf("[FAIL] RX i2s_channel_init_std_mode: %s\n", esp_err_to_name(err));
        return;
    }

    // Enable TX then RX — no delay (matches official example).
    i2s_channel_enable(s_tx);
    i2s_channel_enable(s_rx);

    s_ok = true;
    Serial.println("[OK]  I2S enabled (TX first, RX second — official Espressif order)");
    Serial.println("[OK]  Waiting 500ms for PCM1808 PLL to stabilise...");
    delay(500);
    Serial.println("[OK]  Passthrough active — play audio into PCM1808");
    Serial.println("      Format: [time] peak_dBFS  maxAbs  timeouts/total");
    Serial.println("--------------------------------------------");
}

// ---------------------------------------------------------------------------

void loop() {
    if (!s_ok) { delay(1000); return; }

    // Read ADC samples (50ms timeout — longer than one DMA buffer at 48kHz/256 frames = 5.3ms)
    size_t rx_bytes = 0;
    esp_err_t err = i2s_channel_read(s_rx, s_rxbuf, sizeof(s_rxbuf),
                                     &rx_bytes, pdMS_TO_TICKS(50));
    s_totalReads++;

    if (err == ESP_ERR_TIMEOUT || rx_bytes == 0) {
        s_timeouts++;
        memset(s_rxbuf, 0, sizeof(s_rxbuf));
    }

    // Track peak absolute value across the window
    for (int i = 0; i < BUF_FRAMES * 2; i++) {
        int32_t abs_val = (s_rxbuf[i] < 0) ? -s_rxbuf[i] : s_rxbuf[i];
        if (abs_val > s_maxAbsWin) s_maxAbsWin = abs_val;
    }

    // Passthrough: write ADC buffer directly to DAC (20ms timeout)
    size_t tx_bytes = 0;
    i2s_channel_write(s_tx, s_rxbuf, sizeof(s_rxbuf), &tx_bytes, pdMS_TO_TICKS(20));

    // Print diagnostics every ~1 second
    // At 48kHz, 256 frames/buffer → ~5.3ms/buffer → ~188 reads/second → print every 188
    static const uint32_t PRINT_EVERY = 188;
    if (s_totalReads % PRINT_EVERY == 0) {
        float db = sample_to_dbfs(s_maxAbsWin);
        Serial.printf("[%3us] peak=%6.1f dBFS   maxAbs=0x%08X   timeout=%u/%u\n",
                      (unsigned)(millis() / 1000), db,
                      (unsigned)s_maxAbsWin, s_timeouts, s_totalReads);

        // Print raw hex of first 8 samples (4 frames L+R) for format verification
        // Expected: low byte = 0x00 (left-justified 24-bit), values vary with audio
        Serial.printf("       raw[0..7]: %08X %08X %08X %08X  %08X %08X %08X %08X\n",
                      s_rxbuf[0], s_rxbuf[1], s_rxbuf[2], s_rxbuf[3],
                      s_rxbuf[4], s_rxbuf[5], s_rxbuf[6], s_rxbuf[7]);

        // Reset window peak for next interval
        s_maxAbsWin = 0;
    }
}
