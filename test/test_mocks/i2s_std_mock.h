#pragma once
#ifdef NATIVE_TEST
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// ===== Minimal IDF5 I2S type stubs for native tests =====
// Mirrors the <driver/i2s_std.h> API used in production firmware.

typedef void*  i2s_chan_handle_t;
typedef int    esp_err_t;

#ifndef ESP_OK
#define ESP_OK          0
#endif
#ifndef ESP_ERR_TIMEOUT
#define ESP_ERR_TIMEOUT (-1)
#endif

#define I2S_GPIO_UNUSED  ((int)-1)

typedef enum { I2S_NUM_0 = 0, I2S_NUM_1 = 1 } i2s_port_t;
typedef enum { I2S_ROLE_MASTER = 0 }            i2s_role_t;
typedef enum {
    I2S_DATA_BIT_WIDTH_16BIT = 16,
    I2S_DATA_BIT_WIDTH_24BIT = 24,
    I2S_DATA_BIT_WIDTH_32BIT = 32
} i2s_data_bit_width_t;
typedef enum {
    I2S_SLOT_MODE_MONO   = 1,
    I2S_SLOT_MODE_STEREO = 2
} i2s_slot_mode_t;

struct i2s_chan_config_t {
    i2s_port_t  id;
    i2s_role_t  role;
    uint32_t    dma_desc_num;
    uint32_t    dma_frame_num;
};

struct i2s_std_clk_config_t {
    uint32_t sample_rate_hz;
};

struct i2s_std_slot_config_t {
    i2s_data_bit_width_t data_bit_width;
    i2s_slot_mode_t      slot_mode;
};

struct i2s_gpio_cfg_t {
    int mclk;
    int bclk;
    int ws;
    int dout;
    int din;
    struct {
        uint8_t mclk_inv;
        uint8_t bclk_inv;
        uint8_t ws_inv;
    } invert_flags;
};

struct i2s_std_config_t {
    i2s_std_clk_config_t  clk_cfg;
    i2s_std_slot_config_t slot_cfg;
    i2s_gpio_cfg_t        gpio_cfg;
};

#define I2S_CHANNEL_DEFAULT_CONFIG(port, role) \
    { (port), (role), 8, 256 }

#define I2S_STD_CLK_DEFAULT_CONFIG(rate) \
    { (rate) }

#define I2S_STD_MSB_SLOT_DEFAULT_CONFIG(bits, mode) \
    { (bits), (mode) }

// ===== Mock state tracking =====
namespace I2sMock {
    static bool    channelEnabled[2]  = {false, false};
    static bool    channelDeleted[2]  = {false, false};
    static uint32_t lastSampleRate[2] = {0, 0};

    inline void reset() {
        memset(channelEnabled,  0, sizeof(channelEnabled));
        memset(channelDeleted,  0, sizeof(channelDeleted));
        memset(lastSampleRate,  0, sizeof(lastSampleRate));
    }
}

// ===== Stub implementations (all return ESP_OK) =====

inline esp_err_t i2s_new_channel(const i2s_chan_config_t* cfg,
                                  i2s_chan_handle_t* tx,
                                  i2s_chan_handle_t* rx) {
    (void)cfg;
    static int id = 0x10;
    if (tx) { *tx = (void*)(uintptr_t)(id++); }
    if (rx) { *rx = (void*)(uintptr_t)(id++); }
    return ESP_OK;
}

inline esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t h,
                                            const i2s_std_config_t* c) {
    (void)h; (void)c;
    return ESP_OK;
}

inline esp_err_t i2s_channel_enable(i2s_chan_handle_t h) {
    (void)h;
    return ESP_OK;
}

inline esp_err_t i2s_channel_disable(i2s_chan_handle_t h) {
    (void)h;
    return ESP_OK;
}

inline esp_err_t i2s_del_channel(i2s_chan_handle_t h) {
    (void)h;
    return ESP_OK;
}

inline esp_err_t i2s_channel_read(i2s_chan_handle_t h, void* buf,
                                   size_t len, size_t* bytes_read,
                                   uint32_t timeout_ms) {
    (void)h; (void)timeout_ms;
    if (buf) memset(buf, 0, len);
    if (bytes_read) *bytes_read = len;
    return ESP_OK;
}

inline esp_err_t i2s_channel_write(i2s_chan_handle_t h, const void* buf,
                                    size_t len, size_t* bytes_written,
                                    uint32_t timeout_ms) {
    (void)h; (void)buf; (void)timeout_ms;
    if (bytes_written) *bytes_written = len;
    return ESP_OK;
}

inline esp_err_t i2s_channel_reconfig_std_clock(i2s_chan_handle_t h,
                                                  const i2s_std_clk_config_t* c) {
    (void)h; (void)c;
    return ESP_OK;
}

#endif // NATIVE_TEST
