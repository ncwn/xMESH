#include <unity.h>
#include "../../../../../lib/xmesh-core/src/TrickleScheduler.cpp"

using namespace xmesh;

void setUp(void) {
    mock::set_millis(0);
    mock::set_zero_random(true);
}

void tearDown(void) {}

void test_start_setsIntervalToImin(void) {
    TrickleScheduler scheduler(60000, 600000, 1, true);
    scheduler.start();
    TEST_ASSERT_EQUAL_FLOAT(60.0, scheduler.getCurrentIntervalSec());
}

void test_reset_resetsToImin(void) {
    TrickleScheduler scheduler(60000, 600000, 1, true);
    scheduler.start();
    
    mock::advance_millis(60001);
    scheduler.shouldTransmit();
    TEST_ASSERT_EQUAL_FLOAT(120.0, scheduler.getCurrentIntervalSec());
    
    scheduler.reset();
    TEST_ASSERT_EQUAL_FLOAT(60.0, scheduler.getCurrentIntervalSec());
}

void test_intervalDoubling(void) {
    TrickleScheduler scheduler(60000, 600000, 1, true);
    scheduler.start();
    
    mock::advance_millis(60001);
    scheduler.shouldTransmit();
    TEST_ASSERT_EQUAL_FLOAT(120.0, scheduler.getCurrentIntervalSec());
    
    mock::advance_millis(120001);
    scheduler.shouldTransmit();
    TEST_ASSERT_EQUAL_FLOAT(240.0, scheduler.getCurrentIntervalSec());
    
    mock::advance_millis(240001);
    scheduler.shouldTransmit();
    TEST_ASSERT_EQUAL_FLOAT(480.0, scheduler.getCurrentIntervalSec());
    
    mock::advance_millis(480001);
    scheduler.shouldTransmit();
    TEST_ASSERT_EQUAL_FLOAT(600.0, scheduler.getCurrentIntervalSec());
}

void test_suppression_k1(void) {
    TrickleScheduler scheduler(60000, 600000, 1, true);
    scheduler.start();
    
    scheduler.onHelloReceived();
    
    mock::advance_millis(30000);
    bool tx = scheduler.shouldTransmit();
    
    TEST_ASSERT_FALSE(tx);
    TEST_ASSERT_EQUAL(1, scheduler.getSuppressCount());
}

void test_noSuppression_whenNoHellos(void) {
    TrickleScheduler scheduler(60000, 600000, 1, true);
    scheduler.start();
    
    mock::advance_millis(30000);
    bool tx = scheduler.shouldTransmit();
    
    TEST_ASSERT_TRUE(tx);
    TEST_ASSERT_EQUAL(1, scheduler.getTransmitCount());
}

void test_isAtMaxInterval(void) {
    TrickleScheduler scheduler(60000, 120000, 1, true);
    scheduler.start();
    
    TEST_ASSERT_FALSE(scheduler.isAtMaxInterval());
    
    mock::advance_millis(60001);
    scheduler.shouldTransmit();
    
    TEST_ASSERT_TRUE(scheduler.isAtMaxInterval());
}

void test_transmitCount_increments(void) {
    TrickleScheduler scheduler(60000, 600000, 1, true);
    scheduler.start();
    
    mock::advance_millis(30000);
    scheduler.shouldTransmit();
    
    TEST_ASSERT_EQUAL(1, scheduler.getTransmitCount());
    
    mock::advance_millis(30001);
    scheduler.shouldTransmit();
    
    mock::advance_millis(60000);
    scheduler.shouldTransmit();
    
    TEST_ASSERT_EQUAL(2, scheduler.getTransmitCount());
}

void test_suppressCount_increments(void) {
    TrickleScheduler scheduler(60000, 600000, 1, true);
    scheduler.start();
    
    scheduler.onHelloReceived();
    mock::advance_millis(30000);
    scheduler.shouldTransmit();
    
    TEST_ASSERT_EQUAL(1, scheduler.getSuppressCount());
}

void test_trickle_on_inconsistent_hello_resets_interval(void) {
    TrickleScheduler scheduler(1000, 4000, 1, true);
    scheduler.start();
    
    mock::advance_millis(1001);
    scheduler.shouldTransmit();
    
    mock::advance_millis(2001);
    scheduler.shouldTransmit();
    
    TEST_ASSERT_EQUAL_FLOAT(4.0, scheduler.getCurrentIntervalSec());
    
    scheduler.onInconsistentHello();
    TEST_ASSERT_EQUAL_FLOAT(1.0, scheduler.getCurrentIntervalSec());
}

void test_trickle_is_enabled_returns_true(void) {
    TrickleScheduler scheduler(1000, 4000, 1, true);
    TEST_ASSERT_TRUE(scheduler.isEnabled());
}

void test_trickle_set_imin_updates_interval(void) {
    TrickleScheduler scheduler(1000, 4000, 1, true);
    scheduler.start();
    scheduler.setIMin(2000);
    scheduler.reset();
    TEST_ASSERT_EQUAL_FLOAT(2.0, scheduler.getCurrentIntervalSec());
}

void test_trickle_set_imax_updates_interval(void) {
    TrickleScheduler scheduler(1000, 2000, 1, true);
    scheduler.start();
    scheduler.setIMax(4000);
    
    mock::advance_millis(1001);
    scheduler.shouldTransmit();
    
    mock::advance_millis(2001);
    scheduler.shouldTransmit();
    
    TEST_ASSERT_EQUAL_FLOAT(4.0, scheduler.getCurrentIntervalSec());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_start_setsIntervalToImin);
    RUN_TEST(test_reset_resetsToImin);
    RUN_TEST(test_intervalDoubling);
    RUN_TEST(test_suppression_k1);
    RUN_TEST(test_noSuppression_whenNoHellos);
    RUN_TEST(test_isAtMaxInterval);
    RUN_TEST(test_transmitCount_increments);
    RUN_TEST(test_suppressCount_increments);
    RUN_TEST(test_trickle_on_inconsistent_hello_resets_interval);
    RUN_TEST(test_trickle_is_enabled_returns_true);
    RUN_TEST(test_trickle_set_imin_updates_interval);
    RUN_TEST(test_trickle_set_imax_updates_interval);
    return UNITY_END();
}
