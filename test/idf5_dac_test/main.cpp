// test/idf5_dac_test/main.cpp
//
// Incremental IDF5 signal-chain test: build up each component in isolation.
// Each stage confirmed working before backporting to main firmware.
//
// TEST_STAGE:
//   0 = bare: raw sine → DAC                              (CONFIRMED CLEAN)
//   1 = + software volume path                            (CONFIRMED CLEAN)
//   2 = + DC-blocking IIR (left-justified, original)      (CONFIRMED CLEAN)
//   4 = + full-duplex I2S: RX active alongside TX, MCLK on GPIO3
//         RX data is read and discarded — tests whether having
//         the PCM1808 RX channel active affects TX output quality.
//
// Build:  pio run -e idf5_dac_test
// Upload: pio run -e idf5_dac_test -t upload

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <math.h>

// ===== Active test stage =====
#define TEST_STAGE 4

// ===== Hardware config (matches main firmware exactly) =====
static const gpio_num_t PIN_BCK  = GPIO_NUM_16;   // I2S_BCK_PIN
static const gpio_num_t PIN_WS   = GPIO_NUM_18;   // I2S_LRC_PIN
static const gpio_num_t PIN_DOUT = GPIO_NUM_40;   // I2S_TX_DATA_PIN (DAC)
static const gpio_num_t PIN_DIN  = GPIO_NUM_17;   // I2S_DOUT_PIN  (ADC1 PCM1808)
static const gpio_num_t PIN_MCLK = GPIO_NUM_3;    // I2S_MCLK_PIN

// ===== Tone parameters =====
static const uint32_t SAMPLE_RATE_HZ = 48000;
static const float    TONE_HZ        = 1000.0f;
static const float    AMPLITUDE      = 0.5f;     // -6.02 dBFS

// ===== Volume gain (same as Stage 1/2) =====
static const float VOLUME_GAIN = 0.8f;

// ===== DC filter coefficient =====
static const float DC_BLOCK_ALPHA = 0.9987f;

// ===== I2S DMA sizing (matches main firmware: I2S_DMA_BUF_COUNT/LEN) =====
static const int DMA_DESC_NUM  = 8;
static const int DMA_FRAME_NUM = 256;

// ===== Buffers =====
static const int BUF_FRAMES = 256;
static int32_t   s_txbuf[BUF_FRAMES * 2];  // TX: interleaved L/R
static int32_t   s_rxbuf[BUF_FRAMES * 2];  // RX: read-and-discard (Stage 4)

// ===== DC filter state =====
static int32_t s_dcPrevIn  = 0;
static float   s_dcPrevOut = 0.0f;

// ===== Channel handles =====
static i2s_chan_handle_t s_tx = nullptr;
static i2s_chan_handle_t s_rx = nullptr;  // Stage 4 only
static bool              s_ok = false;

// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n========================================");
    Serial.println("  IDF5 I2S DAC Chain Test");
#if TEST_STAGE == 4
    Serial.println("  STAGE 4: full-duplex RX+TX, MCLK on GPIO3");
    Serial.println("  RX reads PCM1808 ADC data (discarded)");
    Serial.println("  TX plays 1kHz sine through volume + DC filter");
#endif
    Serial.println("========================================");
    Serial.printf("  BCK=%d  WS=%d  DOUT=%d  DIN=%d  MCLK=%d\n",
                  PIN_BCK, PIN_WS, PIN_DOUT, PIN_DIN, PIN_MCLK);
    Serial.printf("  %u Hz  %.0f Hz tone  %.2f dBFS\n",
                  SAMPLE_RATE_HZ, TONE_HZ, 20.0f * log10f(AMPLITUDE));
    Serial.println("----------------------------------------");

    // --- Channel allocation ---
    i2s_chan_config_t ch_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ch_cfg.dma_desc_num  = DMA_DESC_NUM;
    ch_cfg.dma_frame_num = DMA_FRAME_NUM;
    ch_cfg.auto_clear    = true;  // TX: send zeros on underrun (matches main firmware)

#if TEST_STAGE >= 4
    // Full-duplex: allocate TX and RX together (IDF5 requires paired allocation)
    esp_err_t err = i2s_new_channel(&ch_cfg, &s_tx, &s_rx);
#else
    // TX-only
    esp_err_t err = i2s_new_channel(&ch_cfg, &s_tx, nullptr);
#endif
    if (err != ESP_OK) {
        Serial.printf("[FAIL] i2s_new_channel: %s\n", esp_err_to_name(err));
        return;
    }

    // --- I2S std config (matches main firmware i2s_audio_enable_tx exactly) ---
    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE_HZ);
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                           I2S_DATA_BIT_WIDTH_32BIT,
                           I2S_SLOT_MODE_STEREO);
#if TEST_STAGE >= 4
    std_cfg.gpio_cfg.mclk = PIN_MCLK;  // MCLK output on GPIO 3
    std_cfg.gpio_cfg.din  = PIN_DIN;   // PCM1808 ADC1 data on GPIO 17
#else
    std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.din  = I2S_GPIO_UNUSED;
#endif
    std_cfg.gpio_cfg.bclk = PIN_BCK;
    std_cfg.gpio_cfg.ws   = PIN_WS;
    std_cfg.gpio_cfg.dout = PIN_DOUT;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv   = false;

#if TEST_STAGE >= 4
    // Full-duplex: IDF5 requires BOTH handles initialized with the same std_cfg
    err = i2s_channel_init_std_mode(s_rx, &std_cfg);
    if (err != ESP_OK) {
        Serial.printf("[FAIL] RX i2s_channel_init_std_mode: %s\n", esp_err_to_name(err));
        return;
    }
#endif
    err = i2s_channel_init_std_mode(s_tx, &std_cfg);
    if (err != ESP_OK) {
        Serial.printf("[FAIL] TX i2s_channel_init_std_mode: %s\n", esp_err_to_name(err));
        return;
    }

#if TEST_STAGE >= 4
    i2s_channel_enable(s_rx);
#endif
    err = i2s_channel_enable(s_tx);
    if (err != ESP_OK) {
        Serial.printf("[FAIL] i2s_channel_enable TX: %s\n", esp_err_to_name(err));
        return;
    }

    s_ok = true;
    Serial.println("[OK]  I2S channels enabled");
}

// ---------------------------------------------------------------------------

void loop() {
    if (!s_ok) { delay(1000); return; }

    static float    phase     = 0.0f;
    static uint32_t buf_count = 0;

    const float phase_inc = 2.0f * (float)M_PI * TONE_HZ / (float)SAMPLE_RATE_HZ;

#if TEST_STAGE >= 4
    // Read RX (PCM1808 ADC data) — discard it.
    // Short timeout: we don't care if read fails, TX must keep running.
    size_t rx_bytes = 0;
    i2s_channel_read(s_rx, s_rxbuf, sizeof(s_rxbuf), &rx_bytes, pdMS_TO_TICKS(10));
    // rx_bytes ignored — we discard ADC data
#endif

    // --- Generate raw left-justified 24-bit sine (Stage 0 base) ---
    for (int f = 0; f < BUF_FRAMES; f++) {
        float sample = sinf(phase) * AMPLITUDE;
        phase += phase_inc;
        s_txbuf[f * 2]     = (int32_t)(sample * 8388607.0f) << 8;
        s_txbuf[f * 2 + 1] = s_txbuf[f * 2];
    }
    phase = fmodf(phase, 2.0f * (float)M_PI);

    // --- Stage 1+: software volume path ---
    for (int i = 0; i < BUF_FRAMES * 2; i++) {
        float f = (float)s_txbuf[i] / 2147483647.0f;
        f *= VOLUME_GAIN;
        s_txbuf[i] = (int32_t)(f * 2147483647.0f);
    }

    // --- Stage 2+: DC-blocking IIR (left-justified, original) ---
    for (int f = 0; f < BUF_FRAMES; f++) {
        int32_t rawL = s_txbuf[f * 2];
        float outL = (float)(rawL - s_dcPrevIn) + DC_BLOCK_ALPHA * s_dcPrevOut;
        s_dcPrevIn  = rawL;
        s_dcPrevOut = outL;
        int32_t filtered = (int32_t)outL;
        s_txbuf[f * 2]     = filtered;
        s_txbuf[f * 2 + 1] = filtered;
    }

    // --- Write to DAC (20ms timeout — matches dac_output_write in main firmware) ---
    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(s_tx, s_txbuf, sizeof(s_txbuf),
                                      &bytes_written, pdMS_TO_TICKS(20));

    buf_count++;
    if (buf_count % 500 == 0) {
        Serial.printf("  [%us] buf=%u  tx=%u/%u bytes  err=%s\n",
                      (unsigned)(millis() / 1000), buf_count,
                      (unsigned)bytes_written, (unsigned)sizeof(s_txbuf),
                      esp_err_to_name(err));
    }
}
