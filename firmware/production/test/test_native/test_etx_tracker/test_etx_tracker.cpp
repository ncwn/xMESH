#include <unity.h>
#include "../../../../../lib/xmesh-core/src/ETXTracker.cpp"

using namespace xmesh;

void setUp(void) {
    mock::set_millis(1000);
}

void tearDown(void) {
}

// 1. test_perfectLink_etxApproachesOne:
// Send sequential packets 1-10. ETX should approach 1.0.
void test_perfectLink_etxApproachesOne(void) {
    ETXTracker tracker;
    uint16_t addr = 0x1234;
    
    for (uint32_t i = 1; i <= 10; i++) {
        tracker.updateLinkMetrics(addr, -70, 5, i);
    }
    
    LinkMetrics* metrics = tracker.getLinkMetrics(addr);
    TEST_ASSERT_NOT_NULL(metrics);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 1.0f, metrics->etx);
    TEST_ASSERT_EQUAL(10, metrics->lastSeqNum);
}

// 2. test_gapDetection_increasesEtx:
// Send: seqNum 1, 3, 5 (50% loss). ETX should approach 2.0.
void test_gapDetection_increasesEtx(void) {
    ETXTracker tracker;
    uint16_t addr = 0x1234;
    
    tracker.updateLinkMetrics(addr, -70, 5, 1);
    
    LinkMetrics* metrics = tracker.getLinkMetrics(addr);
    TEST_ASSERT_NOT_NULL(metrics);
    float initialEtx = metrics->etx;
    
    tracker.updateLinkMetrics(addr, -70, 5, 5);
    tracker.updateLinkMetrics(addr, -70, 5, 9);
    
    TEST_ASSERT_GREATER_THAN(initialEtx, metrics->etx);
    TEST_ASSERT_GREATER_THAN(3, metrics->totalTxFailures);
}

// 3. test_largeGap_capped:
// Send: seqNum 1, then 100. Gap should be capped at 10 failures.
void test_largeGap_capped(void) {
    ETXTracker tracker;
    uint16_t addr = 0x1234;
    
    tracker.updateLinkMetrics(addr, -70, 5, 1);
    tracker.updateLinkMetrics(addr, -70, 5, 100);
    
    LinkMetrics* metrics = tracker.getLinkMetrics(addr);
    TEST_ASSERT_NOT_NULL(metrics);
    
    TEST_ASSERT_EQUAL(10, metrics->totalTxFailures);
    TEST_ASSERT_LESS_OR_EQUAL(10.0f, metrics->etx);
    TEST_ASSERT_GREATER_THAN(2.0f, metrics->etx);
}

void test_rssiSmoothing_ewma(void) {
    ETXTracker tracker;
    uint16_t addr = 0x1234;
    
    mock::set_millis(1000);
    tracker.updateLinkMetrics(addr, -80, 0, 1);
    LinkMetrics* metrics = tracker.getLinkMetrics(addr);
    TEST_ASSERT_EQUAL_INT16(-80, metrics->rssi);
    
    mock::set_millis(2000);
    tracker.updateLinkMetrics(addr, -60, 0, 2);
    TEST_ASSERT_EQUAL_INT16(-74, metrics->rssi);
}

void test_snrSmoothing_ewma(void) {
    ETXTracker tracker;
    uint16_t addr = 0x1234;
    
    mock::set_millis(1000);
    tracker.updateLinkMetrics(addr, -70, 0, 1);
    LinkMetrics* metrics = tracker.getLinkMetrics(addr);
    TEST_ASSERT_EQUAL_INT8(0, metrics->snr);
    
    mock::set_millis(2000);
    tracker.updateLinkMetrics(addr, -70, 10, 2);
    TEST_ASSERT_EQUAL_INT8(3, metrics->snr);
}

// 6. test_newNeighborEviction:
// Add 10 neighbors, then 11th. Oldest should be evicted.
void test_newNeighborEviction(void) {
    ETXTracker tracker;
    
    for (uint16_t i = 1; i <= 10; i++) {
        mock::set_millis(i * 1000);
        tracker.updateLinkMetrics(i, -70, 5, 1);
    }
    TEST_ASSERT_EQUAL(10, tracker.getNumTrackedLinks());
    
    mock::set_millis(11000);
    tracker.updateLinkMetrics(11, -70, 5, 1);
    
    TEST_ASSERT_EQUAL(10, tracker.getNumTrackedLinks());
    
    LinkMetrics* m11 = tracker.getLinkMetrics(11);
    TEST_ASSERT_NOT_NULL(m11);
    TEST_ASSERT_EQUAL(11, m11->address);
    
    tracker.updateLinkMetrics(1, -70, 5, 1);
    LinkMetrics* m1 = tracker.getLinkMetrics(1);
    TEST_ASSERT_NOT_NULL(m1);
    TEST_ASSERT_EQUAL(1, m1->address);
    TEST_ASSERT_EQUAL(11000, m1->lastUpdate);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_perfectLink_etxApproachesOne);
    RUN_TEST(test_gapDetection_increasesEtx);
    RUN_TEST(test_largeGap_capped);
    RUN_TEST(test_rssiSmoothing_ewma);
    RUN_TEST(test_snrSmoothing_ewma);
    RUN_TEST(test_newNeighborEviction);
    return UNITY_END();
}
