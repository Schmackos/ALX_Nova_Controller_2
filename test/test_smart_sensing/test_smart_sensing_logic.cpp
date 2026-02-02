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
    float voltageThreshold = 0.5;
    bool amplifierState = false;
    float lastVoltageReading = 0.0;
    bool previousVoltageState = false;
    unsigned long lastVoltageDetection = 0;

    void reset() {
        currentMode = ALWAYS_ON;
        timerDuration = 5;
        timerRemaining = 0;
        lastTimerUpdate = 0;
        voltageThreshold = 0.5;
        amplifierState = false;
        lastVoltageReading = 0.0;
        previousVoltageState = false;
        lastVoltageDetection = 0;
#ifdef NATIVE_TEST
        ArduinoMock::reset();
#endif
    }
}

// Mock voltage detection function
bool detectVoltage() {
    int rawValue = ArduinoMock::mockAnalogValue;
    TestState::lastVoltageReading = (rawValue / 4095.0) * 3.3;
    return (TestState::lastVoltageReading >= TestState::voltageThreshold);
}

// Mock amplifier state setter
void setAmplifierState(bool state) {
    TestState::amplifierState = state;
    digitalWrite(4, state ? HIGH : LOW); // AMPLIFIER_PIN = 4
}

// Core logic function extracted for testing
void updateSmartSensingLogic() {
    unsigned long currentMillis = millis();

    // Detect voltage on every call (no rate limiting for tests)
    bool voltageDetected = detectVoltage();

    switch (TestState::currentMode) {
        case TestState::ALWAYS_ON:
            setAmplifierState(true);
            TestState::timerRemaining = 0;
            TestState::previousVoltageState = voltageDetected;
            break;

        case TestState::ALWAYS_OFF:
            setAmplifierState(false);
            TestState::timerRemaining = 0;
            TestState::previousVoltageState = voltageDetected;
            break;

        case TestState::SMART_AUTO: {
            if (voltageDetected) {
                // Voltage is currently detected - keep timer at full value
                TestState::timerRemaining = TestState::timerDuration * 60;
                TestState::lastVoltageDetection = currentMillis;
                TestState::lastTimerUpdate = currentMillis;

                // Ensure amplifier is ON
                if (!TestState::amplifierState) {
                    setAmplifierState(true);
                }
            } else {
                // No voltage detected - countdown timer if amplifier is ON
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

            TestState::previousVoltageState = voltageDetected;
            break;
        }
    }
}

// ===== Test Setup/Teardown =====

void setUp(void) {
    TestState::reset();
}

void tearDown(void) {
    // Clean up after each test
}

// ===== Tier 1.1: Smart Sensing Logic Tests =====

// Test 1: Timer stays at full value when voltage is detected
void test_timer_stays_full_when_voltage_detected(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5; // 5 minutes = 300 seconds
    ArduinoMock::mockAnalogValue = 2000; // Above threshold (0.5V)
    ArduinoMock::mockMillis = 0;

    // First update - should detect voltage and set timer to full
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
    TEST_ASSERT_TRUE(TestState::amplifierState);

    // Advance time by 5 seconds (5000ms)
    ArduinoMock::mockMillis = 5000;

    // Update again - timer should still be at full value
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
    TEST_ASSERT_TRUE(TestState::amplifierState);
}

// Test 2: Timer counts down when no voltage detected
void test_timer_counts_down_without_voltage(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5; // 5 minutes = 300 seconds
    TestState::amplifierState = true; // Start with amp ON
    TestState::timerRemaining = 10; // 10 seconds remaining
    TestState::lastTimerUpdate = 0;
    ArduinoMock::mockAnalogValue = 0; // No voltage
    ArduinoMock::mockMillis = 0;

    // First update at 0ms
    updateSmartSensingLogic();

    // Advance time by 1 second
    ArduinoMock::mockMillis = 1000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(9, TestState::timerRemaining);

    // Advance another second
    ArduinoMock::mockMillis = 2000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(8, TestState::timerRemaining);
}

// Test 3: Timer resets when voltage reappears during countdown
void test_timer_resets_when_voltage_reappears(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5; // 5 minutes = 300 seconds
    TestState::amplifierState = true;
    TestState::timerRemaining = 10; // Counting down
    TestState::lastTimerUpdate = 0;
    ArduinoMock::mockMillis = 0;

    // No voltage - timer should count down
    ArduinoMock::mockAnalogValue = 0;
    updateSmartSensingLogic();

    ArduinoMock::mockMillis = 1000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(9, TestState::timerRemaining);

    // Voltage reappears - timer should reset to full
    ArduinoMock::mockAnalogValue = 2000; // Above threshold
    ArduinoMock::mockMillis = 2000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
    TEST_ASSERT_TRUE(TestState::amplifierState);
}

// Test 4: Amplifier turns OFF when timer reaches zero
void test_amplifier_turns_off_at_zero(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::amplifierState = true;
    TestState::timerRemaining = 2; // 2 seconds remaining
    TestState::lastTimerUpdate = 0;
    ArduinoMock::mockAnalogValue = 0; // No voltage
    ArduinoMock::mockMillis = 0;

    // First countdown
    updateSmartSensingLogic();
    ArduinoMock::mockMillis = 1000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(1, TestState::timerRemaining);
    TEST_ASSERT_TRUE(TestState::amplifierState);

    // Second countdown - should reach zero and turn off
    ArduinoMock::mockMillis = 2000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(0, TestState::timerRemaining);
    TEST_ASSERT_FALSE(TestState::amplifierState);
}

// Test 5: ALWAYS_ON mode keeps amplifier ON
void test_always_on_mode(void) {
    TestState::currentMode = TestState::ALWAYS_ON;
    ArduinoMock::mockAnalogValue = 0; // No voltage

    updateSmartSensingLogic();
    TEST_ASSERT_TRUE(TestState::amplifierState);
    TEST_ASSERT_EQUAL(0, TestState::timerRemaining);
}

// Test 6: ALWAYS_OFF mode keeps amplifier OFF
void test_always_off_mode(void) {
    TestState::currentMode = TestState::ALWAYS_OFF;
    ArduinoMock::mockAnalogValue = 2000; // Voltage present

    updateSmartSensingLogic();
    TEST_ASSERT_FALSE(TestState::amplifierState);
    TEST_ASSERT_EQUAL(0, TestState::timerRemaining);
}

// Test 7: Voltage threshold detection
void test_voltage_threshold_detection(void) {
    TestState::voltageThreshold = 0.5; // 0.5V threshold

    // Below threshold (0V)
    ArduinoMock::mockAnalogValue = 0;
    TEST_ASSERT_FALSE(detectVoltage());

    // Above threshold (2V = ~2482 raw value)
    ArduinoMock::mockAnalogValue = 2482;
    TEST_ASSERT_TRUE(detectVoltage());

    // Right at threshold (0.5V = ~620 raw value)
    ArduinoMock::mockAnalogValue = 621;
    TEST_ASSERT_TRUE(detectVoltage());
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

    // Switch to SMART_AUTO with voltage
    TestState::currentMode = TestState::SMART_AUTO;
    ArduinoMock::mockAnalogValue = 2000;
    updateSmartSensingLogic();
    TEST_ASSERT_TRUE(TestState::amplifierState);
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
}

// Test 9: Rapid voltage fluctuations
void test_rapid_voltage_fluctuations(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5;
    ArduinoMock::mockMillis = 0;

    // Voltage ON
    ArduinoMock::mockAnalogValue = 2000;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);

    // Voltage OFF for 100ms (should start countdown after 1s)
    ArduinoMock::mockAnalogValue = 0;
    ArduinoMock::mockMillis = 100;
    updateSmartSensingLogic();

    ArduinoMock::mockMillis = 1100;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(299, TestState::timerRemaining);

    // Voltage ON again - should reset
    ArduinoMock::mockAnalogValue = 2000;
    ArduinoMock::mockMillis = 1200;
    updateSmartSensingLogic();
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
}

// Test 10: Edge case - timer at 0 with voltage appearing
void test_timer_at_zero_with_voltage(void) {
    TestState::currentMode = TestState::SMART_AUTO;
    TestState::timerDuration = 5;
    TestState::amplifierState = false;
    TestState::timerRemaining = 0;
    ArduinoMock::mockMillis = 0;

    // Voltage appears - should turn ON and set timer
    ArduinoMock::mockAnalogValue = 2000;
    updateSmartSensingLogic();
    TEST_ASSERT_TRUE(TestState::amplifierState);
    TEST_ASSERT_EQUAL(300, TestState::timerRemaining);
}

// ===== Test Runner =====

int runUnityTests(void) {
    UNITY_BEGIN();

    RUN_TEST(test_timer_stays_full_when_voltage_detected);
    RUN_TEST(test_timer_counts_down_without_voltage);
    RUN_TEST(test_timer_resets_when_voltage_reappears);
    RUN_TEST(test_amplifier_turns_off_at_zero);
    RUN_TEST(test_always_on_mode);
    RUN_TEST(test_always_off_mode);
    RUN_TEST(test_voltage_threshold_detection);
    RUN_TEST(test_mode_transitions);
    RUN_TEST(test_rapid_voltage_fluctuations);
    RUN_TEST(test_timer_at_zero_with_voltage);

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
