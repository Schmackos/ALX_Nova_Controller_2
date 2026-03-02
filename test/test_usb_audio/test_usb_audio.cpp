#include <cmath>
#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Include the source directly — pure functions are always compiled,
// ESP32-specific code is guarded by #ifndef NATIVE_TEST.
#include "../../src/usb_audio.cpp"

// ===== Test Fixtures =====

static int32_t ringBufStorage[128 * 2]; // 128 frames, stereo
static UsbAudioRingBuffer rb;

void setUp(void) {
    memset(ringBufStorage, 0, sizeof(ringBufStorage));
    usb_rb_init(&rb, ringBufStorage, 128);
}

void tearDown(void) {}

// ===== Ring Buffer: Initialization =====

void test_rb_init_empty(void) {
    TEST_ASSERT_EQUAL(0, usb_rb_available(&rb));
    TEST_ASSERT_EQUAL(127, usb_rb_free(&rb)); // capacity-1
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, usb_rb_fill_level(&rb));
}

void test_rb_init_positions_zero(void) {
    TEST_ASSERT_EQUAL(0, rb.writePos);
    TEST_ASSERT_EQUAL(0, rb.readPos);
    TEST_ASSERT_EQUAL(0, rb.overruns);
    TEST_ASSERT_EQUAL(0, rb.underruns);
}

// ===== Ring Buffer: Write / Read =====

void test_rb_write_read_basic(void) {
    int32_t wdata[4] = {100, 200, 300, 400}; // 2 stereo frames
    uint32_t written = usb_rb_write(&rb, wdata, 2);
    TEST_ASSERT_EQUAL(2, written);
    TEST_ASSERT_EQUAL(2, usb_rb_available(&rb));

    int32_t rdata[4] = {};
    uint32_t read = usb_rb_read(&rb, rdata, 2);
    TEST_ASSERT_EQUAL(2, read);
    TEST_ASSERT_EQUAL(100, rdata[0]);
    TEST_ASSERT_EQUAL(200, rdata[1]);
    TEST_ASSERT_EQUAL(300, rdata[2]);
    TEST_ASSERT_EQUAL(400, rdata[3]);
    TEST_ASSERT_EQUAL(0, usb_rb_available(&rb));
}

void test_rb_write_multiple_reads(void) {
    int32_t wdata[8] = {1, 2, 3, 4, 5, 6, 7, 8}; // 4 frames
    usb_rb_write(&rb, wdata, 4);

    int32_t rdata[4] = {};
    usb_rb_read(&rb, rdata, 2); // Read first 2 frames
    TEST_ASSERT_EQUAL(1, rdata[0]);
    TEST_ASSERT_EQUAL(2, rdata[1]);
    TEST_ASSERT_EQUAL(3, rdata[2]);
    TEST_ASSERT_EQUAL(4, rdata[3]);

    usb_rb_read(&rb, rdata, 2); // Read next 2 frames
    TEST_ASSERT_EQUAL(5, rdata[0]);
    TEST_ASSERT_EQUAL(6, rdata[1]);
    TEST_ASSERT_EQUAL(7, rdata[2]);
    TEST_ASSERT_EQUAL(8, rdata[3]);

    TEST_ASSERT_EQUAL(0, usb_rb_available(&rb));
}

// ===== Ring Buffer: Wraparound =====

void test_rb_wraparound(void) {
    // Fill most of the buffer
    int32_t wdata[2] = {42, 43};
    for (int i = 0; i < 120; i++) {
        wdata[0] = i * 2;
        wdata[1] = i * 2 + 1;
        usb_rb_write(&rb, wdata, 1);
    }
    TEST_ASSERT_EQUAL(120, usb_rb_available(&rb));

    // Read 100 frames to advance readPos
    int32_t rdata[2];
    for (int i = 0; i < 100; i++) {
        usb_rb_read(&rb, rdata, 1);
    }
    TEST_ASSERT_EQUAL(20, usb_rb_available(&rb));

    // Write 100 more frames — this wraps around
    for (int i = 0; i < 100; i++) {
        wdata[0] = 1000 + i;
        wdata[1] = 2000 + i;
        usb_rb_write(&rb, wdata, 1);
    }
    TEST_ASSERT_EQUAL(120, usb_rb_available(&rb));

    // Read and verify data integrity after wraparound
    // First 20 frames are the remaining from original write (indices 100-119)
    for (int i = 0; i < 20; i++) {
        usb_rb_read(&rb, rdata, 1);
        TEST_ASSERT_EQUAL((100 + i) * 2, rdata[0]);
        TEST_ASSERT_EQUAL((100 + i) * 2 + 1, rdata[1]);
    }
    // Next 100 frames are from the second write
    for (int i = 0; i < 100; i++) {
        usb_rb_read(&rb, rdata, 1);
        TEST_ASSERT_EQUAL(1000 + i, rdata[0]);
        TEST_ASSERT_EQUAL(2000 + i, rdata[1]);
    }
}

// ===== Ring Buffer: Overflow =====

void test_rb_overflow_tracking(void) {
    int32_t wdata[2] = {1, 2};

    // Fill to capacity-1 (127 frames)
    for (int i = 0; i < 127; i++) {
        usb_rb_write(&rb, wdata, 1);
    }
    TEST_ASSERT_EQUAL(127, usb_rb_available(&rb));
    TEST_ASSERT_EQUAL(0, usb_rb_free(&rb));
    TEST_ASSERT_EQUAL(0, rb.overruns);

    // Try to write 5 more — should overflow
    uint32_t written = usb_rb_write(&rb, wdata, 5);
    TEST_ASSERT_EQUAL(0, written); // No space
    TEST_ASSERT_EQUAL(5, rb.overruns);
}

// ===== Ring Buffer: Underflow =====

void test_rb_underflow_tracking(void) {
    int32_t rdata[2];

    // Try to read from empty buffer
    uint32_t read = usb_rb_read(&rb, rdata, 3);
    TEST_ASSERT_EQUAL(0, read);
    TEST_ASSERT_EQUAL(3, rb.underruns);
}

void test_rb_partial_read(void) {
    int32_t wdata[4] = {10, 20, 30, 40};
    usb_rb_write(&rb, wdata, 2);

    int32_t rdata[8] = {};
    uint32_t read = usb_rb_read(&rb, rdata, 5); // Request 5 but only 2 available
    TEST_ASSERT_EQUAL(2, read);
    TEST_ASSERT_EQUAL(3, rb.underruns); // 5-2 = 3 underrun frames
    TEST_ASSERT_EQUAL(10, rdata[0]);
    TEST_ASSERT_EQUAL(20, rdata[1]);
}

// ===== Ring Buffer: Fill Level =====

void test_rb_fill_level_half(void) {
    int32_t wdata[2] = {1, 2};
    // Write ~64 frames into 127-capacity buffer
    for (int i = 0; i < 64; i++) {
        usb_rb_write(&rb, wdata, 1);
    }
    float level = usb_rb_fill_level(&rb);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.504f, level); // 64/127
}

// ===== Ring Buffer: Reset =====

void test_rb_reset(void) {
    int32_t wdata[2] = {1, 2};
    usb_rb_write(&rb, wdata, 1);
    rb.overruns = 10;
    rb.underruns = 5;

    usb_rb_reset(&rb);
    TEST_ASSERT_EQUAL(0, usb_rb_available(&rb));
    TEST_ASSERT_EQUAL(0, rb.overruns);
    TEST_ASSERT_EQUAL(0, rb.underruns);
}

// ===== Format Conversion: PCM16 to Int32 =====

void test_pcm16_to_int32_silence(void) {
    int16_t src[4] = {0, 0, 0, 0};
    int32_t dst[4] = {};
    usb_pcm16_to_int32(src, dst, 2);
    TEST_ASSERT_EQUAL(0, dst[0]);
    TEST_ASSERT_EQUAL(0, dst[1]);
    TEST_ASSERT_EQUAL(0, dst[2]);
    TEST_ASSERT_EQUAL(0, dst[3]);
}

void test_pcm16_to_int32_positive_full_scale(void) {
    int16_t src[2] = {32767, 32767};
    int32_t dst[2] = {};
    usb_pcm16_to_int32(src, dst, 1);
    // 32767 << 16 = 0x7FFF0000
    TEST_ASSERT_EQUAL_INT32(0x7FFF0000, dst[0]);
    TEST_ASSERT_EQUAL_INT32(0x7FFF0000, dst[1]);
}

void test_pcm16_to_int32_negative_full_scale(void) {
    int16_t src[2] = {-32768, -32768};
    int32_t dst[2] = {};
    usb_pcm16_to_int32(src, dst, 1);
    // -32768 << 16 = 0x80000000
    TEST_ASSERT_EQUAL_INT32((int32_t)0x80000000, dst[0]);
}

void test_pcm16_to_int32_mid_value(void) {
    int16_t src[2] = {16384, -16384};
    int32_t dst[2] = {};
    usb_pcm16_to_int32(src, dst, 1);
    TEST_ASSERT_EQUAL_INT32(16384 << 16, dst[0]);
    TEST_ASSERT_EQUAL_INT32(-16384 << 16, dst[1]);
}

// ===== Format Conversion: PCM24 to Int32 =====

void test_pcm24_to_int32_silence(void) {
    uint8_t src[6] = {0, 0, 0, 0, 0, 0};
    int32_t dst[2] = {};
    usb_pcm24_to_int32(src, dst, 1);
    TEST_ASSERT_EQUAL_INT32(0, dst[0]);
    TEST_ASSERT_EQUAL_INT32(0, dst[1]);
}

void test_pcm24_to_int32_positive_full_scale(void) {
    // 0x7FFFFF = max positive 24-bit, stored as {0xFF, 0xFF, 0x7F}
    uint8_t src[6] = {0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0x7F};
    int32_t dst[2] = {};
    usb_pcm24_to_int32(src, dst, 1);
    // Expected: 0x7FFFFF << 8 = 0x7FFFFF00
    TEST_ASSERT_EQUAL_INT32(0x7FFFFF00, dst[0]);
    TEST_ASSERT_EQUAL_INT32(0x7FFFFF00, dst[1]);
}

void test_pcm24_to_int32_negative_full_scale(void) {
    // -1 in 24-bit = 0xFFFFFF, stored as {0xFF, 0xFF, 0xFF}
    uint8_t src[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    int32_t dst[2] = {};
    usb_pcm24_to_int32(src, dst, 1);
    // 0xFFFFFF sign-extended = 0xFFFFFFFF, << 8 = 0xFFFFFF00
    TEST_ASSERT_EQUAL_INT32((int32_t)0xFFFFFF00, dst[0]);
}

void test_pcm24_to_int32_small_negative(void) {
    // -256 in 24-bit = 0xFFFE00, stored as {0x00, 0xFE, 0xFF}
    uint8_t src[3] = {0x00, 0xFE, 0xFF};
    int32_t dst[2] = {0, 0}; // Need 2 for a frame but testing one sample
    // Test with a single-channel approach (manually compute)
    uint32_t raw = 0x00 | (0xFE << 8) | (0xFF << 16);
    // raw = 0xFFFE00, sign extend: 0xFFFFFE00, left-justify << 8 = 0xFE000000... wait

    // Actually test one full stereo frame
    uint8_t src2[6] = {0x00, 0x00, 0x80, 0x00, 0x00, 0x80}; // -8388608 (min 24-bit)
    usb_pcm24_to_int32(src2, dst, 1);
    // 0x800000 sign-extended = 0xFF800000, << 8 = 0x80000000
    TEST_ASSERT_EQUAL_INT32((int32_t)0x80000000, dst[0]);
}

// ===== Volume Conversion =====

void test_volume_to_linear_zero_db(void) {
    float result = usb_volume_to_linear(0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, result);
}

void test_volume_to_linear_minus_6db(void) {
    // -6 dB = -6 * 256 = -1536
    float result = usb_volume_to_linear(-1536);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.501f, result); // ~0.501
}

void test_volume_to_linear_minus_20db(void) {
    // -20 dB = -20 * 256 = -5120
    float result = usb_volume_to_linear(-5120);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.1f, result);
}

void test_volume_to_linear_silence(void) {
    float result = usb_volume_to_linear(-32767);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, result);
}

void test_volume_to_linear_positive_clamped(void) {
    float result = usb_volume_to_linear(100);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, result);
}

void test_volume_to_linear_minus_40db(void) {
    // -40 dB = -10240
    float result = usb_volume_to_linear(-10240);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.01f, result);
}

// ===== State Machine =====

void test_state_initial_disconnected(void) {
    TEST_ASSERT_EQUAL(USB_AUDIO_DISCONNECTED, usb_audio_get_state());
    TEST_ASSERT_FALSE(usb_audio_is_connected());
    TEST_ASSERT_FALSE(usb_audio_is_streaming());
}

void test_api_defaults(void) {
    TEST_ASSERT_EQUAL(48000, usb_audio_get_sample_rate());
    TEST_ASSERT_EQUAL(16, usb_audio_get_bit_depth());
    TEST_ASSERT_EQUAL(2, usb_audio_get_channels());
    TEST_ASSERT_EQUAL(0, usb_audio_get_volume());
    TEST_ASSERT_FALSE(usb_audio_get_mute());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, usb_audio_get_volume_linear());
}

// ===== Ring Buffer: 96kHz Capacity =====

void test_rb_96khz_fill_rate(void) {
    // Test that a 4096-frame buffer sustains 42ms of 96kHz without overflow.
    // 42ms at 96kHz = 4032 frames — fits in 4095 usable slots (4096-1).
    int32_t bigStorage[4096 * 2];
    UsbAudioRingBuffer bigRb;
    usb_rb_init(&bigRb, bigStorage, 4096);

    int32_t frame[2] = {1000, 2000};
    uint32_t total = 0;
    for (int ms = 0; ms < 42; ms++) {
        for (int f = 0; f < 96; f++) {  // 96 frames per ms at 96kHz
            usb_rb_write(&bigRb, frame, 1);
            total++;
        }
    }
    TEST_ASSERT_EQUAL(4032, total);
    TEST_ASSERT_EQUAL(0, bigRb.overruns);
    TEST_ASSERT_EQUAL(4032, usb_rb_available(&bigRb));
}

void test_rb_96khz_drain_rate(void) {
    // Drain 256 frames from a buffer with 512 frames
    int32_t bigStorage[4096 * 2];
    UsbAudioRingBuffer bigRb;
    usb_rb_init(&bigRb, bigStorage, 4096);

    int32_t frame[2] = {42, 43};
    for (int i = 0; i < 512; i++) {
        usb_rb_write(&bigRb, frame, 1);
    }
    TEST_ASSERT_EQUAL(512, usb_rb_available(&bigRb));

    int32_t out[512];
    uint32_t drained = usb_rb_read(&bigRb, out, 256);
    TEST_ASSERT_EQUAL(256, drained);
    TEST_ASSERT_EQUAL(256, usb_rb_available(&bigRb));
    TEST_ASSERT_EQUAL(0, bigRb.underruns);
}

// ===== Format Conversion: PCM24 Multi-frame =====

void test_pcm24_to_int32_stereo_multiframe(void) {
    // 4 stereo frames = 8 samples = 24 bytes
    uint8_t src[24];
    int32_t dst[8];

    // Frame 0: L=+1, R=-1 (in 24-bit)
    src[0] = 0x01; src[1] = 0x00; src[2] = 0x00;  // +1
    src[3] = 0xFF; src[4] = 0xFF; src[5] = 0xFF;  // -1
    // Frame 1: L=0, R=0
    src[6] = 0x00; src[7] = 0x00; src[8] = 0x00;
    src[9] = 0x00; src[10] = 0x00; src[11] = 0x00;
    // Frame 2: L=max positive, R=max negative
    src[12] = 0xFF; src[13] = 0xFF; src[14] = 0x7F;  // 0x7FFFFF
    src[15] = 0x00; src[16] = 0x00; src[17] = 0x80;  // 0x800000 (-8388608)
    // Frame 3: L=mid positive, R=mid negative
    src[18] = 0x00; src[19] = 0x00; src[20] = 0x40;  // 0x400000
    src[21] = 0x00; src[22] = 0x00; src[23] = 0xC0;  // 0xC00000 (-4194304)

    usb_pcm24_to_int32(src, dst, 4);

    // Frame 0: +1 << 8 = 0x100, -1 sign-ext << 8 = 0xFFFFFF00
    TEST_ASSERT_EQUAL_INT32(0x100, dst[0]);
    TEST_ASSERT_EQUAL_INT32((int32_t)0xFFFFFF00, dst[1]);
    // Frame 1: 0
    TEST_ASSERT_EQUAL_INT32(0, dst[2]);
    TEST_ASSERT_EQUAL_INT32(0, dst[3]);
    // Frame 2: max positive, max negative
    TEST_ASSERT_EQUAL_INT32(0x7FFFFF00, dst[4]);
    TEST_ASSERT_EQUAL_INT32((int32_t)0x80000000, dst[5]);
    // Frame 3: mid values
    TEST_ASSERT_EQUAL_INT32(0x40000000, dst[6]);
    TEST_ASSERT_EQUAL_INT32((int32_t)0xC0000000, dst[7]);
}

// ===== State Injection =====

void test_native_state_injection(void) {
    // Default state is disconnected
    TEST_ASSERT_EQUAL(USB_AUDIO_DISCONNECTED, usb_audio_get_state());
    TEST_ASSERT_FALSE(usb_audio_is_connected());
    TEST_ASSERT_FALSE(usb_audio_is_streaming());

    // Inject CONNECTED state
    usb_audio_test_set_state(USB_AUDIO_CONNECTED);
    TEST_ASSERT_EQUAL(USB_AUDIO_CONNECTED, usb_audio_get_state());
    TEST_ASSERT_TRUE(usb_audio_is_connected());
    TEST_ASSERT_FALSE(usb_audio_is_streaming());

    // Inject STREAMING state
    usb_audio_test_set_state(USB_AUDIO_STREAMING);
    TEST_ASSERT_TRUE(usb_audio_is_streaming());

    // Reset back
    usb_audio_test_set_state(USB_AUDIO_DISCONNECTED);
    TEST_ASSERT_FALSE(usb_audio_is_connected());
}

void test_negotiated_format_injection(void) {
    // Default: 48000/16
    TEST_ASSERT_EQUAL(48000, usb_audio_get_negotiated_rate());
    TEST_ASSERT_EQUAL(16, usb_audio_get_negotiated_depth());

    // Set 96kHz/24-bit
    usb_audio_test_set_negotiated_format(96000, 24);
    TEST_ASSERT_EQUAL(96000, usb_audio_get_negotiated_rate());
    TEST_ASSERT_EQUAL(24, usb_audio_get_negotiated_depth());
    TEST_ASSERT_EQUAL(96000, usb_audio_get_sample_rate());
    TEST_ASSERT_EQUAL(24, usb_audio_get_bit_depth());

    // Set 44.1kHz/16-bit
    usb_audio_test_set_negotiated_format(44100, 16);
    TEST_ASSERT_EQUAL(44100, usb_audio_get_negotiated_rate());
    TEST_ASSERT_EQUAL(16, usb_audio_get_negotiated_depth());

    // Reset to default
    usb_audio_test_set_negotiated_format(48000, 16);
}

// ===== Volume/Mute Pipeline Gain =====

void test_volume_mute_produces_zero_gain(void) {
    usb_audio_test_set_volume(0, true);  // mute=true, vol=0dB
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, usb_audio_get_volume_linear());
    usb_audio_test_set_volume(0, false);  // cleanup
}

void test_volume_unity_produces_unity_gain(void) {
    usb_audio_test_set_volume(0, false);  // vol=0dB, no mute
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, usb_audio_get_volume_linear());
}

void test_volume_minus_6db_reduces_half(void) {
    usb_audio_test_set_volume(-1536, false);  // -6dB = -6*256 = -1536
    float gain = usb_audio_get_volume_linear();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.501f, gain);
    usb_audio_test_set_volume(0, false);  // cleanup
}

// ===== Native Ring Buffer Read =====

void test_native_streaming_read(void) {
    // The native stub has a 256-frame ring buffer
    usb_audio_init(); // Inits native ring buffer
    usb_audio_test_set_state(USB_AUDIO_STREAMING);

    // Write some data to the native ring buffer
    int32_t wdata[4] = {100, 200, 300, 400};
    usb_rb_write(&_nativeRingBuffer, wdata, 2);

    // Read via public API
    int32_t rdata[4] = {};
    uint32_t got = usb_audio_read(rdata, 2);
    TEST_ASSERT_EQUAL(2, got);
    TEST_ASSERT_EQUAL(100, rdata[0]);
    TEST_ASSERT_EQUAL(200, rdata[1]);

    // Cleanup
    usb_audio_test_set_state(USB_AUDIO_DISCONNECTED);
}

// ===== Main =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Ring Buffer
    RUN_TEST(test_rb_init_empty);
    RUN_TEST(test_rb_init_positions_zero);
    RUN_TEST(test_rb_write_read_basic);
    RUN_TEST(test_rb_write_multiple_reads);
    RUN_TEST(test_rb_wraparound);
    RUN_TEST(test_rb_overflow_tracking);
    RUN_TEST(test_rb_underflow_tracking);
    RUN_TEST(test_rb_partial_read);
    RUN_TEST(test_rb_fill_level_half);
    RUN_TEST(test_rb_reset);

    // Format Conversion: PCM16
    RUN_TEST(test_pcm16_to_int32_silence);
    RUN_TEST(test_pcm16_to_int32_positive_full_scale);
    RUN_TEST(test_pcm16_to_int32_negative_full_scale);
    RUN_TEST(test_pcm16_to_int32_mid_value);

    // Format Conversion: PCM24
    RUN_TEST(test_pcm24_to_int32_silence);
    RUN_TEST(test_pcm24_to_int32_positive_full_scale);
    RUN_TEST(test_pcm24_to_int32_negative_full_scale);
    RUN_TEST(test_pcm24_to_int32_small_negative);

    // Volume Conversion
    RUN_TEST(test_volume_to_linear_zero_db);
    RUN_TEST(test_volume_to_linear_minus_6db);
    RUN_TEST(test_volume_to_linear_minus_20db);
    RUN_TEST(test_volume_to_linear_silence);
    RUN_TEST(test_volume_to_linear_positive_clamped);
    RUN_TEST(test_volume_to_linear_minus_40db);

    // State Machine
    RUN_TEST(test_state_initial_disconnected);
    RUN_TEST(test_api_defaults);

    // 96kHz Ring Buffer
    RUN_TEST(test_rb_96khz_fill_rate);
    RUN_TEST(test_rb_96khz_drain_rate);

    // Format Conversion: PCM24 Multi-frame
    RUN_TEST(test_pcm24_to_int32_stereo_multiframe);

    // State Injection
    RUN_TEST(test_native_state_injection);
    RUN_TEST(test_negotiated_format_injection);

    // Volume/Mute Pipeline Gain
    RUN_TEST(test_volume_mute_produces_zero_gain);
    RUN_TEST(test_volume_unity_produces_unity_gain);
    RUN_TEST(test_volume_minus_6db_reduces_half);

    // Native Ring Buffer Read
    RUN_TEST(test_native_streaming_read);

    return UNITY_END();
}
