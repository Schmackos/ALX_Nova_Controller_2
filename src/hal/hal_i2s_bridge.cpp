#ifdef DAC_ENABLED
#include "hal_i2s_bridge.h"

#ifndef NATIVE_TEST
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include "../config.h"
#include "../debug_serial.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#endif

bool hal_i2s_tx_init(void** txHandle, const HalDeviceConfig* cfg) {
#ifndef NATIVE_TEST
    // Build I2S TX config from HalDeviceConfig with config.h fallbacks
    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chanCfg.dma_desc_num = 8;
    chanCfg.dma_frame_num = 256;
    i2s_chan_handle_t tx = nullptr;
    if (i2s_new_channel(&chanCfg, &tx, nullptr) != ESP_OK) return false;

    uint32_t rate    = (cfg && cfg->sampleRate > 0) ? cfg->sampleRate : 48000;
    uint8_t  bits    = (cfg && cfg->bitDepth > 0)   ? cfg->bitDepth   : 16;
    int      dataPin = (cfg && cfg->pinData > 0)    ? cfg->pinData    : I2S_TX_DATA_PIN;
    int      mclkPin = (cfg && cfg->pinMclk > 0)    ? cfg->pinMclk    : I2S_MCLK_PIN;

    i2s_std_config_t stdCfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t)bits, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (gpio_num_t)mclkPin,
            .bclk = (gpio_num_t)I2S_BCK_PIN,
            .ws   = (gpio_num_t)I2S_LRC_PIN,
            .dout = (gpio_num_t)dataPin,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { 0, 0, 0 }
        }
    };
    if (i2s_channel_init_std_mode(tx, &stdCfg) != ESP_OK) {
        i2s_del_channel(tx);
        return false;
    }
    if (i2s_channel_enable(tx) != ESP_OK) {
        i2s_del_channel(tx);
        return false;
    }
    *txHandle = (void*)tx;
    LOG_I("[HAL I2S Bridge] TX channel init OK: %uHz %ubit pin=%d", rate, bits, dataPin);
    return true;
#else
    *txHandle = (void*)0x1;  // Non-null mock handle
    return true;
#endif
}

bool hal_i2s_rx_init(void** rxHandle, const HalDeviceConfig* cfg) {
#ifndef NATIVE_TEST
    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    chanCfg.dma_desc_num = 8;
    chanCfg.dma_frame_num = 256;
    i2s_chan_handle_t rx = nullptr;
    if (i2s_new_channel(&chanCfg, nullptr, &rx) != ESP_OK) return false;

    uint32_t rate   = (cfg && cfg->sampleRate > 0) ? cfg->sampleRate : 48000;
    uint8_t  bits   = (cfg && cfg->bitDepth > 0)   ? cfg->bitDepth   : 16;
    int      dinPin = (cfg && cfg->pinData > 0)    ? cfg->pinData    : I2S_DOUT_PIN;

    i2s_std_config_t stdCfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t)bits, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_GPIO_UNUSED,
            .ws   = I2S_GPIO_UNUSED,
            .dout = I2S_GPIO_UNUSED,
            .din  = (gpio_num_t)dinPin,
            .invert_flags = { 0, 0, 0 }
        }
    };
    if (i2s_channel_init_std_mode(rx, &stdCfg) != ESP_OK) {
        i2s_del_channel(rx);
        return false;
    }
    if (i2s_channel_enable(rx) != ESP_OK) {
        i2s_del_channel(rx);
        return false;
    }
    *rxHandle = (void*)rx;
    LOG_I("[HAL I2S Bridge] RX channel init OK: %uHz %ubit pin=%d", rate, bits, dinPin);
    return true;
#else
    *rxHandle = (void*)0x2;
    return true;
#endif
}

bool hal_i2s_reconfigure(void* handle, uint32_t rate, uint8_t bits) {
#ifndef NATIVE_TEST
    i2s_chan_handle_t ch = (i2s_chan_handle_t)handle;
    i2s_channel_disable(ch);
    i2s_std_clk_config_t clkCfg = I2S_STD_CLK_DEFAULT_CONFIG(rate);
    i2s_channel_reconfig_std_clock(ch, &clkCfg);
    i2s_channel_enable(ch);
    return true;
#else
    (void)handle;
    (void)rate;
    (void)bits;
    return true;
#endif
}

#endif // DAC_ENABLED
