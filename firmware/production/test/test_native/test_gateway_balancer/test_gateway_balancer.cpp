#include <unity.h>
#include "../../../../../lib/xmesh-core/src/GatewayBalancer.cpp"

using namespace xmesh;

void setUp(void) {
    mock::reset();
    mock::set_millis(1000);
}

void tearDown(void) {}

void test_encodeGatewayLoad_boundaries(void) {
    TEST_ASSERT_EQUAL_UINT8(0, GatewayBalancer::encodeGatewayLoad(0.0f));
    TEST_ASSERT_EQUAL_UINT8(100, GatewayBalancer::encodeGatewayLoad(100.0f));
    TEST_ASSERT_EQUAL_UINT8(254, GatewayBalancer::encodeGatewayLoad(254.5f));
    TEST_ASSERT_EQUAL_UINT8(254, GatewayBalancer::encodeGatewayLoad(300.0f));
    TEST_ASSERT_EQUAL_UINT8(0, GatewayBalancer::encodeGatewayLoad(-5.0f));
}

void test_decodeGatewayLoad_roundtrip(void) {
    TEST_ASSERT_EQUAL_FLOAT(100.0f, GatewayBalancer::decodeGatewayLoad(100));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, GatewayBalancer::decodeGatewayLoad(255));
    
    for (uint8_t i = 0; i < 255; i++) {
        TEST_ASSERT_EQUAL_UINT8(i, GatewayBalancer::encodeGatewayLoad(GatewayBalancer::decodeGatewayLoad(i)));
    }
}

void test_getGatewayBias_calculation(void) {
    GatewayBalancer gb;
    TEST_ASSERT_EQUAL_FLOAT(1.0f, gb.getGatewayBias(0x1234, 100));
    TEST_ASSERT_EQUAL_FLOAT(0.5f, gb.getGatewayBias(0x1234, 50));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, gb.getGatewayBias(0x1234, 255));
}

void test_neighborHealth_warningAfter180s(void) {
    GatewayBalancer gb;
    uint16_t addr = 0xABCD;
    
    gb.updateNeighborHealth(addr); // Initial heartbeat at t=0
    TEST_ASSERT_FALSE(gb.isNeighborFailed(addr));
    
    mock::advance_millis(180001);
    gb.monitorNeighborHealth();
    
    uint8_t missedHellos;
    uint32_t silence;
    TEST_ASSERT_TRUE(gb.getNeighborStats(addr, missedHellos, silence));
    TEST_ASSERT_EQUAL_UINT8(1, missedHellos);
    TEST_ASSERT_FALSE(gb.isNeighborFailed(addr));
}

void test_neighborHealth_failureAfter360s(void) {
    GatewayBalancer gb;
    uint16_t addr = 0xABCD;
    
    gb.updateNeighborHealth(addr); // Initial heartbeat at t=0
    
    mock::advance_millis(360001);
    uint8_t failedCount = gb.monitorNeighborHealth();
    
    TEST_ASSERT_EQUAL_UINT8(1, failedCount);
    TEST_ASSERT_TRUE(gb.isNeighborFailed(addr));
}

void test_gateway_record_load_sample(void) {
    GatewayBalancer gb;
    gb.setIsGateway(true);
    
    for (int i = 0; i < 5; i++) {
        gb.recordGatewayLoadSample();
    }
    
    mock::set_millis(1000);
    gb.sampleLocalGatewayLoadForHello(); 
    
    mock::advance_millis(60000); 
    for (int i = 0; i < 10; i++) {
        gb.recordGatewayLoadSample();
    }
    
    uint8_t encoded = gb.sampleLocalGatewayLoadForHello();
    TEST_ASSERT_EQUAL_UINT8(10, encoded);
}

void test_gateway_sample_local_load_for_hello(void) {
    GatewayBalancer gb;
    gb.setIsGateway(true);
    
    mock::set_millis(1000);
    gb.sampleLocalGatewayLoadForHello(); 
    
    mock::advance_millis(30000); 
    for (int i = 0; i < 50; i++) {
        gb.recordGatewayLoadSample();
    }
    
    uint8_t encoded = gb.sampleLocalGatewayLoadForHello();
    TEST_ASSERT_EQUAL_UINT8(100, encoded);
}

void test_gateway_peek_local_load(void) {
    GatewayBalancer gb;
    gb.setIsGateway(true);
    
    mock::set_millis(1000);
    gb.sampleLocalGatewayLoadForHello();
    
    mock::advance_millis(60000);
    for (int i = 0; i < 25; i++) {
        gb.recordGatewayLoadSample();
    }
    
    gb.sampleLocalGatewayLoadForHello(); 
    TEST_ASSERT_EQUAL_UINT8(25, gb.peekLocalGatewayLoad());
    
    TEST_ASSERT_EQUAL_UINT8(25, gb.peekLocalGatewayLoad());
}

void test_gateway_get_neighbor_count(void) {
    GatewayBalancer gb;
    TEST_ASSERT_EQUAL_UINT8(0, gb.getNeighborCount());
    
    gb.updateNeighborHealth(0x1111);
    TEST_ASSERT_EQUAL_UINT8(1, gb.getNeighborCount());
    
    gb.updateNeighborHealth(0x2222);
    TEST_ASSERT_EQUAL_UINT8(2, gb.getNeighborCount());
    
    gb.updateNeighborHealth(0x1111);
    TEST_ASSERT_EQUAL_UINT8(2, gb.getNeighborCount());
}

void test_gateway_get_neighbor_address(void) {
    GatewayBalancer gb;
    gb.updateNeighborHealth(0xAAAA);
    gb.updateNeighborHealth(0xBBBB);
    
    TEST_ASSERT_EQUAL_UINT16(0xAAAA, gb.getNeighborAddress(0));
    TEST_ASSERT_EQUAL_UINT16(0xBBBB, gb.getNeighborAddress(1));
    TEST_ASSERT_EQUAL_UINT16(0, gb.getNeighborAddress(2)); 
}

void test_gateway_set_is_gateway(void) {
    GatewayBalancer gb;
    gb.setIsGateway(false);
    TEST_ASSERT_EQUAL_UINT8(255, gb.sampleLocalGatewayLoadForHello());
    
    gb.setIsGateway(true);
    mock::set_millis(1000);
    TEST_ASSERT_EQUAL_UINT8(0, gb.sampleLocalGatewayLoadForHello()); 
}

void test_gateway_set_warning_threshold(void) {
    GatewayBalancer gb;
    uint16_t addr = 0x1234;
    
    gb.setWarningThreshold(50000);
    gb.updateNeighborHealth(addr);
    
    mock::advance_millis(50001);
    gb.monitorNeighborHealth();
    
    uint8_t missed;
    uint32_t silence;
    gb.getNeighborStats(addr, missed, silence);
    TEST_ASSERT_EQUAL_UINT8(1, missed);
}

void test_gateway_set_detection_threshold(void) {
    GatewayBalancer gb;
    uint16_t addr = 0x1234;
    
    gb.setWarningThreshold(50000);
    gb.setDetectionThreshold(100000);
    gb.updateNeighborHealth(addr);
    
    mock::advance_millis(100001);
    gb.monitorNeighborHealth();
    
    TEST_ASSERT_TRUE(gb.isNeighborFailed(addr));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_encodeGatewayLoad_boundaries);
    RUN_TEST(test_decodeGatewayLoad_roundtrip);
    RUN_TEST(test_getGatewayBias_calculation);
    RUN_TEST(test_neighborHealth_warningAfter180s);
    RUN_TEST(test_neighborHealth_failureAfter360s);
    RUN_TEST(test_gateway_record_load_sample);
    RUN_TEST(test_gateway_sample_local_load_for_hello);
    RUN_TEST(test_gateway_peek_local_load);
    RUN_TEST(test_gateway_get_neighbor_count);
    RUN_TEST(test_gateway_get_neighbor_address);
    RUN_TEST(test_gateway_set_is_gateway);
    RUN_TEST(test_gateway_set_warning_threshold);
    RUN_TEST(test_gateway_set_detection_threshold);
    return UNITY_END();
}
