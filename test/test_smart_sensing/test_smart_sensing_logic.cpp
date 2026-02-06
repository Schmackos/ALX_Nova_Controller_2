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

// Core logic function extracted for testing
void updateSmartSensingLogic() {
    unsigned long currentMillis = millis();

    bool signalDetected = detectSignal();

    switch (TestState::currentMode) {
        case TestState::ALWAYS_ON:
            setAmplifierState(true);
            TestState::timerRemaining = 0;
            TestState::previousSignalState = signalDetected;
            break;

        case TestState::ALWAYS_OFF:
            setAmplifierState(false);
            TestState::timerRemaining = 0;
            TestState::previousSignalState = signalDetected;
            break;

        case TestState::SMART_AUTO: {
            if (signalDetected) {
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

            TestState::previousSignalState = signalDetected;
            break;
        }
    }
}

// ===== Test Setup/Teardown =====

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

    // Signal reappears - timer should reset to full
    mockAudioLevel_dBFS = -20.0f;
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
    updateSmartSensingLogic();
    TEST_ASSERT_TRUE(TestState::amplifierState);
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
}

// Test 9: Rapid signal fluctuations
void test_rapid_signal_fluctuations(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5;
    ArduinoMock::mockMillis = 0;

    // Signal ON
    mockAudioLevel_dBFS = -20.0f;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);

    // Signal OFF for 100ms (should start countdown after 1s)
    mockAudioLevel_dBFS = -96.0f;
    ArduinoMock::mockMillis = 100;
    updateSmartSensingLogic();

    ArduinoMock::mockMillis = 1100;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(299, TestState::timerRemaining);

    // Signal ON again - should reset
    mockAudioLevel_dBFS = -20.0f;
    ArduinoMock::mockMillis = 1200;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
}

// Test 10: Edge case - timer at 0 with signal appearing
void test_timer_at_zero_with_signal(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5;
    TestState::amplifierState = false;
    TestState::timerRemaining = 0;
    ArduinoMock::mockMillis = 0;

    // Signal appears - should turn ON and set timer
    mockAudioLevel_dBFS = -20.0f;
    updateSmartSensingLogic();
    TEST_ASSERT_TRUE(TestState::amplifierState);
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
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
    RUN_TEST(test_rapid_signal_fluctuations);
    RUN_TEST(test_timer_at_zero_with_signal);

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
