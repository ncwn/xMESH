#include <unity.h>
#include "../../../../../lib/xmesh-core/src/MobilityDetector.cpp"

using namespace xmesh;

void setUp(void) {
    mock::set_millis(100000);
}

void tearDown(void) {}

/**
 * 1. test_staticState_lowVariance:
 *    - Enable detector
 *    - Feed 10 constant SNR values: [5, 5, 5, 5, 5, 5, 5, 5, 5, 5]
 *    - tick(true)
 *    - getState() == MobilityState::STATIC
 *    - getAggregateVariance() ~= 0.0
 */
void test_staticState_lowVariance(void) {
    MobilityDetector detector;
    detector.enable();
    
    for (int i = 0; i < 10; i++) {
        detector.feedSNR(0x0002, 5);
    }
    
    detector.tick(true);
    
    TEST_ASSERT_EQUAL(MobilityState::STATIC, detector.getState());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, detector.getAggregateVariance());
}

/**
 * 2. test_staticToMobile_highVariance:
 *    - Enable detector
 *    - Feed high variance SNR: [-5, 10, -3, 8, -2, 9, -4, 7, -1, 6]
 *    - tick(false) 3 times (exceeds HIGH_VARIANCE_COUNT_THRESHOLD)
 *    - getState() == MobilityState::MOBILE
 */
void test_staticToMobile_highVariance(void) {
    MobilityDetector detector;
    detector.enable();
    
    int8_t snr_values[] = {-5, 10, -3, 8, -2, 9, -4, 7, -1, 6};
    for (int i = 0; i < 10; i++) {
        detector.feedSNR(0x0002, snr_values[i]);
    }
    
    mock::advance_millis(61000);
    detector.tick(false);
    mock::advance_millis(61000);
    detector.tick(false);
    mock::advance_millis(61000);
    detector.tick(false);
    
    TEST_ASSERT_EQUAL(MobilityState::MOBILE, detector.getState());
}

/**
 * 3. test_mobileToStatic_lowVarianceAtMax:
 *    - simulateState(MOBILE)
 *    - Feed low variance SNR (constant values)
 *    - tick(true) with trickleAtMax=true for 120s
 *    - getState() == MobilityState::STATIC
 */
void test_mobileToStatic_lowVarianceAtMax(void) {
    MobilityDetector detector;
    detector.enable();
    detector.simulateState(MobilityState::MOBILE);
    
    for (int i = 0; i < 10; i++) {
        detector.feedSNR(0x0002, 5);
    }
    
    mock::advance_millis(61000);
    detector.tick(true);
    mock::advance_millis(121000);
    detector.tick(true);
    
    TEST_ASSERT_EQUAL(MobilityState::STATIC, detector.getState());
}

/**
 * 4. test_emergencyTrigger:
 *    - Enable detector
 *    - triggerEmergency()
 *    - getState() == MobilityState::EMERGENCY
 *    - getStateName() == "EMERGENCY"
 */
void test_emergencyTrigger(void) {
    MobilityDetector detector;
    detector.enable();
    
    detector.triggerEmergency();
    
    TEST_ASSERT_EQUAL(MobilityState::EMERGENCY, detector.getState());
    TEST_ASSERT_EQUAL_STRING("EMERGENCY", detector.getStateName());
}

/**
 * 5. test_emergencyExpires:
 *    - triggerEmergency()
 *    - Advance time by 60001ms (> EMERGENCY_HOLD_MS)
 *    - tick(true)
 *    - getState() == MobilityState::MOBILE (not STATIC)
 */
void test_emergencyExpires(void) {
    MobilityDetector detector;
    detector.enable();
    
    detector.triggerEmergency();
    TEST_ASSERT_EQUAL(MobilityState::EMERGENCY, detector.getState());
    
    mock::advance_millis(60001);
    detector.tick(true);
    
    TEST_ASSERT_EQUAL(MobilityState::MOBILE, detector.getState());
}

/**
 * 6. test_simulateState_works:
 *    - simulateState(MobilityState::MOBILE)
 *    - getState() == MobilityState::MOBILE
 *    - simulateState(MobilityState::STATIC)
 *    - getState() == MobilityState::STATIC
 */
void test_simulateState_works(void) {
    MobilityDetector detector;
    detector.enable();
    
    detector.simulateState(MobilityState::MOBILE);
    TEST_ASSERT_EQUAL(MobilityState::MOBILE, detector.getState());
    
    detector.simulateState(MobilityState::STATIC);
    TEST_ASSERT_EQUAL(MobilityState::STATIC, detector.getState());
}

/**
 * 7. test_mobility_disable_stops_detection:
 *    - Enable then Disable detector
 *    - Feed high variance SNR
 *    - tick(false) 3 times
 *    - Verify state remains STATIC (default)
 */
void test_mobility_disable_stops_detection(void) {
    MobilityDetector detector;
    detector.enable();
    detector.disable();
    
    int8_t snr_values[] = {-5, 10, -3, 8, -2, 9, -4, 7, -1, 6};
    for (int i = 0; i < 10; i++) {
        detector.feedSNR(0x0002, snr_values[i]);
    }
    
    mock::advance_millis(61000);
    detector.tick(false);
    mock::advance_millis(61000);
    detector.tick(false);
    mock::advance_millis(61000);
    detector.tick(false);
    
    TEST_ASSERT_EQUAL(MobilityState::STATIC, detector.getState());
}

/**
 * 8. test_mobility_is_enabled_reflects_state:
 *    - Verify isEnabled() returns true initially (after enable), false after disable(), true after enable()
 */
void test_mobility_is_enabled_reflects_state(void) {
    MobilityDetector detector;
    detector.enable();
    TEST_ASSERT_TRUE(detector.isEnabled());
    
    detector.disable();
    TEST_ASSERT_FALSE(detector.isEnabled());
    
    detector.enable();
    TEST_ASSERT_TRUE(detector.isEnabled());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_staticState_lowVariance);
    RUN_TEST(test_staticToMobile_highVariance);
    RUN_TEST(test_mobileToStatic_lowVarianceAtMax);
    RUN_TEST(test_emergencyTrigger);
    RUN_TEST(test_emergencyExpires);
    RUN_TEST(test_simulateState_works);
    RUN_TEST(test_mobility_disable_stops_detection);
    RUN_TEST(test_mobility_is_enabled_reflects_state);
    return UNITY_END();
}
