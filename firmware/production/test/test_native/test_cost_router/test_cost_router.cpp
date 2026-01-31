#include <unity.h>
#include "../../../../../lib/xmesh-core/src/CostRouter.cpp"

using namespace xmesh;

void setUp(void) {}
void tearDown(void) {}

/**
 * 1. test_normalizeRSSI_boundaries:
 *    - normalizeRSSI(-120) == 0.0 (min)
 *    - normalizeRSSI(-30) == 1.0 (max)
 *    - normalizeRSSI(-75) == 0.5 (midpoint)
 */
void test_normalizeRSSI_boundaries(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, CostRouter::normalizeRSSI(-120));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.0, CostRouter::normalizeRSSI(-30));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.5, CostRouter::normalizeRSSI(-75));
    
    // Test clamping
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, CostRouter::normalizeRSSI(-130));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.0, CostRouter::normalizeRSSI(-20));
}

/**
 * 2. test_normalizeSNR_boundaries:
 *    - normalizeSNR(-20) == 0.0 (min)
 *    - normalizeSNR(10) == 1.0 (max)
 *    - normalizeSNR(-5) == 0.5 (midpoint)
 */
void test_normalizeSNR_boundaries(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, CostRouter::normalizeSNR(-20));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.0, CostRouter::normalizeSNR(10));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.5, CostRouter::normalizeSNR(-5));
    
    // Test clamping
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, CostRouter::normalizeSNR(-25));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.0, CostRouter::normalizeSNR(15));
}

/**
 * 3. test_calculateCost_perfectLink:
 *    - 1 hop, RSSI=-30, SNR=10, ETX=1.0, bias=0
 *    - Expected: ~1.0 (only hop count contributes)
 *    Formula: cost = W1*hops + W2*(1-norm_RSSI) + W3*(1-norm_SNR) + W4*(ETX-1) + W5*gateway_bias
 *    Defaults: W1=1.0, W2=0.3, W3=0.2, W4=0.4, W5=1.0
 *    Calc: 1.0*1 + 0.3*(1-1) + 0.2*(1-1) + 0.4*(1-1) + 1.0*0 = 1.0
 */
void test_calculateCost_perfectLink(void) {
    CostRouter router;
    float cost = router.calculateCost(1, 0x0002, 0x0001, -30, 10, 1.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.0, cost);
}

/**
 * 4. test_calculateCost_worstLink:
 *    - 3 hops, RSSI=-120, SNR=-20, ETX=5.0, bias=0.5
 *    - Expected: ~5.6 (3 + 0.3 + 0.2 + 1.6 + 0.5)
 *    Calc: 1.0*3 + 0.3*(1-0) + 0.2*(1-0) + 0.4*(5-1) + 1.0*0.5
 *          = 3.0 + 0.3 + 0.2 + 1.6 + 0.5 = 5.6
 */
void test_calculateCost_worstLink(void) {
    CostRouter router;
    float cost = router.calculateCost(3, 0x0002, 0x0001, -120, -20, 5.0f, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 5.6, cost);
}

/**
 * 5. test_weightGetters:
 *    - Default constructor: W1=1.0, W2=0.3, W3=0.2, W4=0.4, W5=1.0
 *    - Custom constructor: verify custom values returned
 */
void test_weightGetters(void) {
    CostRouter defaultRouter;
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.0, defaultRouter.getW1());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.3, defaultRouter.getW2());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.2, defaultRouter.getW3());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.4, defaultRouter.getW4());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.0, defaultRouter.getW5());
    
    CostRouter customRouter(2.0, 0.5, 0.1, 0.8, 1.5);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 2.0, customRouter.getW1());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.5, customRouter.getW2());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.1, customRouter.getW3());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.8, customRouter.getW4());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.5, customRouter.getW5());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_normalizeRSSI_boundaries);
    RUN_TEST(test_normalizeSNR_boundaries);
    RUN_TEST(test_calculateCost_perfectLink);
    RUN_TEST(test_calculateCost_worstLink);
    RUN_TEST(test_weightGetters);
    return UNITY_END();
}
