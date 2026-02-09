#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Mock definitions for testing
namespace TestState {
    enum SensingMode { ALWAYS_ON, ALWAYS_OFF, SMART_AUTO };

    SensingMode currentMode = ALWAYS_ON;
    unsigned long timerDuration = 5; // 5 minutes
    unsigned long timerRemaining = 0;
    unsigned long lastTimerUpdate = 0;
    float audioThreshold_dBFS = -40.0f;
    bool amplifierState = false;
    float audioLevel_dBFS = -96.0f;
    bool previousSignalState = false;
    unsigned long lastSignalDetection = 0;
    float smoothedAudioLevel = -96.0f;

    void reset() {
        currentMode = ALWAYS_ON;
        timerDuration = 5;
        timerRemaining = 0;
        lastTimerUpdate = 0;
        audioThreshold_dBFS = -40.0f;
        amplifierState = false;
        audioLevel_dBFS = -96.0f;
        previousSignalState = false;
        lastSignalDetection = 0;
        smoothedAudioLevel = -96.0f;
#ifdef NATIVE_TEST
        ArduinoMock::reset();
#endif
    }
}

// Mock audio level (simulates dBFS reading from I2S)
static float mockAudioLevel_dBFS = -96.0f;

// Mock signal detection function (replaces old detectVoltage)
bool detectSignal() {
    TestState::audioLevel_dBFS = mockAudioLevel_dBFS;
    return (TestState::audioLevel_dBFS >= TestState::audioThreshold_dBFS);
}

// Mock amplifier state setter
void setAmplifierState(bool state) {
    TestState::amplifierState = state;
    digitalWrite(4, state ? HIGH : LOW); // AMPLIFIER_PIN = 4
}

// Core logic function extracted for testing (mirrors src/smart_sensing.cpp)
void updateSmartSensingLogic() {
    unsigned long currentMillis = millis();

    detectSignal();
    // Smooth audio level for stable signal detection (α=0.15, τ≈308ms)
    TestState::smoothedAudioLevel += (TestState::audioLevel_dBFS - TestState::smoothedAudioLevel) * 0.15f;

    // Use smoothed level for signal presence decision
    bool signalPresent = (TestState::smoothedAudioLevel >= TestState::audioThreshold_dBFS);

    switch (TestState::currentMode) {
        case TestState::ALWAYS_ON:
            setAmplifierState(true);
            TestState::timerRemaining = 0;
            TestState::previousSignalState = signalPresent;
            break;

        case TestState::ALWAYS_OFF:
            setAmplifierState(false);
            TestState::timerRemaining = 0;
            TestState::previousSignalState = signalPresent;
            break;

        case TestState::SMART_AUTO: {
            if (signalPresent) {
                TestState::timerRemaining = TestState::timerDuration * 60;
                TestState::lastSignalDetection = currentMillis;
                TestState::lastTimerUpdate = currentMillis;

                if (!TestState::amplifierState) {
                    setAmplifierState(true);
                }
            } else {
                if (TestState::amplifierState && TestState::timerRemaining > 0) {
                    if (currentMillis - TestState::lastTimerUpdate >= 1000) {
                        TestState::lastTimerUpdate = currentMillis;
                        TestState::timerRemaining--;

                        if (TestState::timerRemaining == 0) {
                            setAmplifierState(false);
                        }
                    }
                }
            }

            TestState::previousSignalState = signalPresent;
            break;
        }
    }
}

// ===== Test Setup/Teardown =====

// Helper: run multiple EMA iterations to let smoothed level converge
// With α=0.15, ~30 iterations gets within 1% of target
void convergeSmoothedLevel(int iterations = 40) {
    for (int i = 0; i < iterations; i++) {
        TestState::smoothedAudioLevel += (mockAudioLevel_dBFS - TestState::smoothedAudioLevel) * 0.15f;
    }
}

void setUp(void) {
    TestState::reset();
    mockAudioLevel_dBFS = -96.0f;
}

void tearDown(void) {
    // Clean up after each test
}

// ===== Tier 1.1: Smart Sensing Logic Tests =====

// Test 1: Timer stays at full value when signal is detected
void test_timer_stays_full_when_signal_detected(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5; // 5 minutes = 300 seconds
    mockAudioLevel_dBFS = -20.0f; // Above threshold (-40 dBFS)
    convergeSmoothedLevel(); // Let EMA settle to -20 dBFS
    ArduinoMock::mockMillis = 0;

    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
    TEST_ASSERT_TRUE(TestState::amplifierState);

    ArduinoMock::mockMillis = 5000;

    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
    TEST_ASSERT_TRUE(TestState::amplifierState);
}

// Test 2: Timer counts down when no signal detected
void test_timer_counts_down_without_signal(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5;
    TestState::amplifierState = true;
    TestState::timerRemaining = 10;
    TestState::lastTimerUpdate = 0;
    mockAudioLevel_dBFS = -96.0f; // No signal (silence)
    ArduinoMock::mockMillis = 0;

    updateSmartSensingLogic();

    ArduinoMock::mockMillis = 1000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(9, TestState::timerRemaining);

    ArduinoMock::mockMillis = 2000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(8, TestState::timerRemaining);
}

// Test 3: Timer resets when signal reappears during countdown
void test_timer_resets_when_signal_reappears(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5;
    TestState::amplifierState = true;
    TestState::timerRemaining = 10;
    TestState::lastTimerUpdate = 0;
    ArduinoMock::mockMillis = 0;

    // No signal - timer should count down
    mockAudioLevel_dBFS = -96.0f;
    updateSmartSensingLogic();

    ArduinoMock::mockMillis = 1000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(9, TestState::timerRemaining);

    // Signal reappears - converge smoothed level then check timer resets
    mockAudioLevel_dBFS = -20.0f;
    convergeSmoothedLevel();
    ArduinoMock::mockMillis = 2000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
    TEST_ASSERT_TRUE(TestState::amplifierState);
}

// Test 4: Amplifier turns OFF when timer reaches zero
void test_amplifier_turns_off_at_zero(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::amplifierState = true;
    TestState::timerRemaining = 2;
    TestState::lastTimerUpdate = 0;
    mockAudioLevel_dBFS = -96.0f;
    ArduinoMock::mockMillis = 0;

    updateSmartSensingLogic();
    ArduinoMock::mockMillis = 1000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(1, TestState::timerRemaining);
    TEST_ASSERT_TRUE(TestState::amplifierState);

    ArduinoMock::mockMillis = 2000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(0, TestState::timerRemaining);
    TEST_ASSERT_FALSE(TestState::amplifierState);
}

// Test 5: ALWAYS_ON mode keeps amplifier ON
void test_always_on_mode(void) {
    TestState::currentMode = TestState::ALWAYS_ON;
    mockAudioLevel_dBFS = -96.0f; // No signal

    updateSmartSensingLogic();
    TEST_ASSERT_TRUE(TestState::amplifierState);
    TEST_ASSERT_EQUAL(0, TestState::timerRemaining);
}

// Test 6: ALWAYS_OFF mode keeps amplifier OFF
void test_always_off_mode(void) {
    TestState::currentMode = TestState::ALWAYS_OFF;
    mockAudioLevel_dBFS = -20.0f; // Signal present

    updateSmartSensingLogic();
    TEST_ASSERT_FALSE(TestState::amplifierState);
    TEST_ASSERT_EQUAL(0, TestState::timerRemaining);
}

// Test 7: Audio threshold detection (dBFS)
void test_audio_threshold_detection(void) {
    TestState::audioThreshold_dBFS = -40.0f;

    // Below threshold (silence)
    mockAudioLevel_dBFS = -96.0f;
    TEST_ASSERT_FALSE(detectSignal());

    // Above threshold
    mockAudioLevel_dBFS = -20.0f;
    TEST_ASSERT_TRUE(detectSignal());

    // At threshold (boundary)
    mockAudioLevel_dBFS = -40.0f;
    TEST_ASSERT_TRUE(detectSignal());

    // Just below threshold
    mockAudioLevel_dBFS = -40.1f;
    TEST_ASSERT_FALSE(detectSignal());
}

// Test 8: Mode transitions
void test_mode_transitions(void) {
    // Start in ALWAYS_ON
    TestState::currentMode = TestState::ALWAYS_ON;
    updateSmartSensingLogic();
    TEST_ASSERT_TRUE(TestState::amplifierState);

    // Switch to ALWAYS_OFF
    TestState::currentMode = TestState::ALWAYS_OFF;
    updateSmartSensingLogic();
    TEST_ASSERT_FALSE(TestState::amplifierState);

    // Switch to SMART_AUTO with signal
    TestState::currentMode = TestState::SMART_AUTO;
    mockAudioLevel_dBFS = -20.0f;
    convergeSmoothedLevel(); // Let EMA settle before SMART_AUTO evaluation
    updateSmartSensingLogic();
    TEST_ASSERT_TRUE(TestState::amplifierState);
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
}

// Test 9: Sustained signal with smoothing converges and timer resets
void test_sustained_signal_with_smoothing(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5;
    TestState::amplifierState = true;
    TestState::timerRemaining = 100;
    TestState::lastTimerUpdate = 0;
    ArduinoMock::mockMillis = 0;

    // No signal initially, smoothed level at -96
    mockAudioLevel_dBFS = -96.0f;
    updateSmartSensingLogic();

    // Count down one tick
    ArduinoMock::mockMillis = 1000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(99, TestState::timerRemaining);

    // Sustained signal appears - converge EMA to above threshold
    mockAudioLevel_dBFS = -20.0f;
    convergeSmoothedLevel();
    ArduinoMock::mockMillis = 2000;
    updateSmartSensingLogic();

    // Timer should reset to full
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
    TEST_ASSERT_TRUE(TestState::amplifierState);
}

// Test 10: Edge case - timer at 0 with signal appearing
void test_timer_at_zero_with_signal(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5;
    TestState::amplifierState = false;
    TestState::timerRemaining = 0;
    ArduinoMock::mockMillis = 0;

    // Signal appears - converge smoothed level, then should turn ON and set timer
    mockAudioLevel_dBFS = -20.0f;
    convergeSmoothedLevel();
    updateSmartSensingLogic();
    TEST_ASSERT_TRUE(TestState::amplifierState);
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
}

// ===== Tier 1.2: EMA Smoothing Tests =====

// Test 11: Noise near threshold - alternating above/below doesn't prevent countdown
void test_noise_near_threshold_timer_counts_down(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5;
    TestState::audioThreshold_dBFS = -60.0f;
    TestState::amplifierState = true;
    TestState::timerRemaining = 300;
    TestState::lastTimerUpdate = 0;
    // Start smoothed level at -62 (below threshold, typical noise floor)
    mockAudioLevel_dBFS = -62.0f;
    TestState::smoothedAudioLevel = -62.0f;
    ArduinoMock::mockMillis = 0;

    // Simulate 20 iterations alternating -58 (above) and -64 (below)
    // Average = -61 dBFS, which is below -60 threshold
    // EMA should stay below threshold despite spikes above it
    for (int i = 0; i < 20; i++) {
        mockAudioLevel_dBFS = (i % 2 == 0) ? -58.0f : -64.0f;
        updateSmartSensingLogic();
    }

    // Smoothed level should be near -61 dBFS (below -60 threshold)
    TEST_ASSERT_TRUE(TestState::smoothedAudioLevel < -60.0f);

    // Timer should have started counting down (not stuck at 300)
    ArduinoMock::mockMillis = 1000;
    updateSmartSensingLogic();
    TEST_ASSERT_TRUE(TestState::timerRemaining < 300);
}

// Test 12: Real signal appears - sustained above-threshold causes timer reset
void test_real_signal_resets_timer_after_smoothing(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5;
    TestState::audioThreshold_dBFS = -60.0f;
    TestState::amplifierState = true;
    TestState::timerRemaining = 200;
    TestState::lastTimerUpdate = 0;
    // Start with smoothed level well below threshold
    mockAudioLevel_dBFS = -80.0f;
    TestState::smoothedAudioLevel = -80.0f;
    ArduinoMock::mockMillis = 0;

    // Real signal at -30 dBFS (well above threshold)
    mockAudioLevel_dBFS = -30.0f;

    // Run enough iterations for EMA to cross -60 threshold
    // From -80, each step: new = old + (-30 - old) * 0.15
    // After ~15 iterations smoothed crosses -60
    int crossedAt = -1;
    for (int i = 0; i < 40; i++) {
        updateSmartSensingLogic();
        if (crossedAt == -1 && TestState::smoothedAudioLevel >= -60.0f) {
            crossedAt = i;
        }
    }

    // Should have crossed threshold within reasonable iterations
    TEST_ASSERT_TRUE(crossedAt > 0);
    TEST_ASSERT_TRUE(crossedAt < 25);

    // After convergence, timer should be at full value
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
}

// Test 13: Signal disappears - smoothing delays timer start
void test_signal_disappears_smoothing_delay(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5;
    TestState::audioThreshold_dBFS = -60.0f;
    TestState::amplifierState = true;
    TestState::timerRemaining = 300;
    TestState::lastTimerUpdate = 0;
    // Start with smoothed level above threshold (signal present)
    mockAudioLevel_dBFS = -30.0f;
    TestState::smoothedAudioLevel = -30.0f;
    ArduinoMock::mockMillis = 0;

    // Signal drops to silence
    mockAudioLevel_dBFS = -96.0f;

    // First few iterations: smoothed is still above -60, timer stays at 300
    updateSmartSensingLogic();
    TEST_ASSERT_TRUE(TestState::smoothedAudioLevel > -60.0f);
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);

    // Run until smoothed crosses below threshold
    int crossedAt = -1;
    for (int i = 1; i < 40; i++) {
        updateSmartSensingLogic();
        if (crossedAt == -1 && TestState::smoothedAudioLevel < -60.0f) {
            crossedAt = i;
        }
    }

    // Should eventually cross below threshold
    TEST_ASSERT_TRUE(crossedAt > 0);

    // After crossing, timer should no longer be at full 300
    // (it may have started counting down once smoothed went below threshold)
    ArduinoMock::mockMillis = 1000;
    mockAudioLevel_dBFS = -96.0f;
    updateSmartSensingLogic();
    TEST_ASSERT_TRUE(TestState::timerRemaining < 300);
}

// ===== Test Runner =====

int runUnityTests(void) {
    UNITY_BEGIN();

    RUN_TEST(test_timer_stays_full_when_signal_detected);
    RUN_TEST(test_timer_counts_down_without_signal);
    RUN_TEST(test_timer_resets_when_signal_reappears);
    RUN_TEST(test_amplifier_turns_off_at_zero);
    RUN_TEST(test_always_on_mode);
    RUN_TEST(test_always_off_mode);
    RUN_TEST(test_audio_threshold_detection);
    RUN_TEST(test_mode_transitions);
    RUN_TEST(test_sustained_signal_with_smoothing);
    RUN_TEST(test_timer_at_zero_with_signal);
    RUN_TEST(test_noise_near_threshold_timer_counts_down);
    RUN_TEST(test_real_signal_resets_timer_after_smoothing);
    RUN_TEST(test_signal_disappears_smoothing_delay);

    return UNITY_END();
}

// For native platform
#ifdef NATIVE_TEST
int main(void) {
    return runUnityTests();
}
#endif

// For Arduino platform
#ifndef NATIVE_TEST
void setup() {
    delay(2000);
    runUnityTests();
}

void loop() {
    // Do nothing
}
#endif
