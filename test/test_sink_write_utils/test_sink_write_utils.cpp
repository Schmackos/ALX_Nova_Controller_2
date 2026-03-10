#include <unity.h>
#include <math.h>

// Pure functions with no Arduino/framework deps -- include implementation directly
#include "../../src/sink_write_utils.h"
#include "../../src/sink_write_utils.cpp"

void setUp(void) {}
void tearDown(void) {}

// --- Volume tests ---
void test_apply_volume_scales_buffer(void) {
    float buf[4] = {1.0f, -1.0f, 0.5f, -0.5f};
    sink_apply_volume(buf, 4, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, buf[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, buf[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, buf[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.25f, buf[3]);
}

void test_apply_volume_unity_is_noop(void) {
    float buf[4] = {1.0f, -1.0f, 0.5f, -0.5f};
    float expected[4] = {1.0f, -1.0f, 0.5f, -0.5f};
    sink_apply_volume(buf, 4, 1.0f);
    for (int i = 0; i < 4; i++)
        TEST_ASSERT_FLOAT_WITHIN(0.001f, expected[i], buf[i]);
}

void test_apply_volume_zero_mutes(void) {
    float buf[4] = {1.0f, -1.0f, 0.5f, -0.5f};
    sink_apply_volume(buf, 4, 0.0f);
    for (int i = 0; i < 4; i++)
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, buf[i]);
}

// --- Mute ramp tests ---
void test_mute_ramp_fades_to_zero(void) {
    float buf[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float rampState = 1.0f;
    sink_apply_mute_ramp(buf, 4, &rampState, true);
    TEST_ASSERT_TRUE(rampState < 1.0f);
}

void test_mute_ramp_recovers_to_unity(void) {
    float buf[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float rampState = 0.0f;
    sink_apply_mute_ramp(buf, 4, &rampState, false);
    TEST_ASSERT_TRUE(rampState > 0.0f);
}

void test_mute_ramp_already_muted_zeros_buffer(void) {
    float buf[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float rampState = 0.0f;
    sink_apply_mute_ramp(buf, 4, &rampState, true);
    for (int i = 0; i < 4; i++)
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, buf[i]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, rampState);
}

void test_mute_ramp_already_unmuted_is_noop(void) {
    float buf[4] = {0.5f, -0.5f, 0.25f, -0.25f};
    float expected[4] = {0.5f, -0.5f, 0.25f, -0.25f};
    float rampState = 1.0f;
    sink_apply_mute_ramp(buf, 4, &rampState, false);
    for (int i = 0; i < 4; i++)
        TEST_ASSERT_FLOAT_WITHIN(0.001f, expected[i], buf[i]);
}

// --- Float to I2S int32 tests ---
// Scale factor is 2147483520.0f (largest exact float32 below 2^31-1)
// to avoid int32 overflow from float rounding of 2147483647.0f.
void test_float_to_i2s_full_scale(void) {
    float in[2] = {1.0f, -1.0f};
    int32_t out[2];
    sink_float_to_i2s_int32(in, out, 2);
    TEST_ASSERT_EQUAL_INT32(2147483520, out[0]);
    TEST_ASSERT_EQUAL_INT32(-2147483520, out[1]);
}

void test_float_to_i2s_clamps(void) {
    float in[2] = {1.5f, -1.5f};
    int32_t out[2];
    sink_float_to_i2s_int32(in, out, 2);
    TEST_ASSERT_EQUAL_INT32(2147483520, out[0]);
    TEST_ASSERT_EQUAL_INT32(-2147483520, out[1]);
}

void test_float_to_i2s_zero(void) {
    float in[1] = {0.0f};
    int32_t out[1];
    sink_float_to_i2s_int32(in, out, 1);
    TEST_ASSERT_EQUAL_INT32(0, out[0]);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_apply_volume_scales_buffer);
    RUN_TEST(test_apply_volume_unity_is_noop);
    RUN_TEST(test_apply_volume_zero_mutes);
    RUN_TEST(test_mute_ramp_fades_to_zero);
    RUN_TEST(test_mute_ramp_recovers_to_unity);
    RUN_TEST(test_mute_ramp_already_muted_zeros_buffer);
    RUN_TEST(test_mute_ramp_already_unmuted_is_noop);
    RUN_TEST(test_float_to_i2s_full_scale);
    RUN_TEST(test_float_to_i2s_clamps);
    RUN_TEST(test_float_to_i2s_zero);
    return UNITY_END();
}
