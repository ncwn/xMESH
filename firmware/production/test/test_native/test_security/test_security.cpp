#include <unity.h>
#include <cstring>
#include "../../../../../lib/xmesh-security/src/FrameCounter.cpp"
#include "../../../../../lib/xmesh-security/src/DeviceAuth.cpp"

using namespace xmesh::security;

void setUp(void) {}
void tearDown(void) {}

void test_frame_counter_outgoing_increments(void) {
    FrameCounter fc;
    fc.begin();
    
    uint32_t first = fc.getNextOutgoing();
    uint32_t second = fc.getNextOutgoing();
    uint32_t third = fc.getNextOutgoing();
    
    TEST_ASSERT_EQUAL_UINT32(1, first);
    TEST_ASSERT_EQUAL_UINT32(2, second);
    TEST_ASSERT_EQUAL_UINT32(3, third);
    TEST_ASSERT_EQUAL_UINT32(4, fc.getCurrentOutgoing());
}

void test_frame_counter_incoming_sequential_validation(void) {
    FrameCounter fc;
    fc.begin();
    
    uint16_t peerAddr = 0x1234;
    
    TEST_ASSERT_TRUE(fc.validateIncoming(peerAddr, 100));
    TEST_ASSERT_TRUE(fc.validateIncoming(peerAddr, 101));
    TEST_ASSERT_TRUE(fc.validateIncoming(peerAddr, 102));
    
    TEST_ASSERT_FALSE(fc.validateIncoming(peerAddr, 102));
    TEST_ASSERT_EQUAL_UINT32(1, fc.getReplayRejectCount());
}

void test_frame_counter_incoming_window_validation(void) {
    FrameCounter fc;
    fc.begin();
    
    uint16_t peerAddr = 0x5678;
    
    TEST_ASSERT_TRUE(fc.validateIncoming(peerAddr, 1000));
    TEST_ASSERT_TRUE(fc.validateIncoming(peerAddr, 1010));
    TEST_ASSERT_TRUE(fc.validateIncoming(peerAddr, 1005));
    TEST_ASSERT_TRUE(fc.validateIncoming(peerAddr, 1001));
    
    TEST_ASSERT_FALSE(fc.validateIncoming(peerAddr, 1005));
    TEST_ASSERT_FALSE(fc.validateIncoming(peerAddr, 990));
}

void test_frame_counter_table_full_handling(void) {
    FrameCounter fc;
    fc.begin();
    
    for (uint16_t i = 1; i <= MAX_TRACKED_PEERS; i++) {
        TEST_ASSERT_TRUE(fc.validateIncoming(i, 1));
    }
    
    TEST_ASSERT_EQUAL_UINT8(MAX_TRACKED_PEERS, fc.getTrackedPeerCount());
    TEST_ASSERT_FALSE(fc.validateIncoming(MAX_TRACKED_PEERS + 1, 1));
}

void test_device_auth_open_mode_allows_all(void) {
    DeviceAuth auth;
    auth.begin(AuthMode::OPEN);
    
    TEST_ASSERT_TRUE(auth.isAuthorized(0x1111));
    TEST_ASSERT_TRUE(auth.isAuthorized(0x2222));
    TEST_ASSERT_EQUAL_UINT32(2, auth.getAuthSuccessCount());
}

void test_device_auth_allowlist_mode_logic(void) {
    DeviceAuth auth;
    auth.begin(AuthMode::ALLOWLIST);
    
    uint16_t deviceAddr = 0x3333;
    uint8_t deviceMacHash[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    
    TEST_ASSERT_FALSE(auth.isAuthorized(deviceAddr));
    TEST_ASSERT_EQUAL_UINT32(1, auth.getAuthFailCount());
    
    TEST_ASSERT_TRUE(auth.addDevice(deviceAddr, deviceMacHash));
    TEST_ASSERT_EQUAL_UINT8(1, auth.getDeviceCount());
    
    TEST_ASSERT_TRUE(auth.isAuthorized(deviceAddr, deviceMacHash));
    
    uint8_t wrongMacHash[4] = {0x00, 0x00, 0x00, 0x00};
    TEST_ASSERT_FALSE(auth.isAuthorized(deviceAddr, wrongMacHash));
    
    TEST_ASSERT_TRUE(auth.removeDevice(deviceAddr));
    TEST_ASSERT_FALSE(auth.isAuthorized(deviceAddr));
}

void test_device_auth_learning_mode_auto_add(void) {
    DeviceAuth auth;
    auth.begin(AuthMode::LEARNING);
    
    uint16_t deviceAddr = 0x4444;
    
    TEST_ASSERT_TRUE(auth.isAuthorized(deviceAddr));
    TEST_ASSERT_EQUAL_UINT8(1, auth.getDeviceCount());
    
    TEST_ASSERT_TRUE(auth.isAuthorized(deviceAddr));
    TEST_ASSERT_EQUAL_UINT8(1, auth.getDeviceCount());
}

void test_device_auth_mac_hash_calculation(void) {
    uint8_t macAddress[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t calculatedHash[4];
    
    DeviceAuth::computeMacHash(macAddress, calculatedHash);
    
    TEST_ASSERT_EQUAL_UINT8(0x05, calculatedHash[0]);
    TEST_ASSERT_EQUAL_UINT8(0x07, calculatedHash[1]);
    TEST_ASSERT_EQUAL_UINT8(0x05, calculatedHash[2]);
    TEST_ASSERT_EQUAL_UINT8(0x00, calculatedHash[3]);
}

void test_frame_counter_persist_returns_true(void) {
    FrameCounter fc;
    fc.begin();
    TEST_ASSERT_TRUE(fc.persist());
}

void test_frame_counter_multiple_peers(void) {
    FrameCounter fc;
    fc.begin();
    
    uint16_t addr1 = 0x1111;
    uint16_t addr2 = 0x2222;
    uint16_t addr3 = 0x3333;
    
    TEST_ASSERT_TRUE(fc.validateIncoming(addr1, 10));
    TEST_ASSERT_TRUE(fc.validateIncoming(addr2, 20));
    TEST_ASSERT_TRUE(fc.validateIncoming(addr3, 30));
    
    TEST_ASSERT_FALSE(fc.validateIncoming(addr1, 10));
    TEST_ASSERT_FALSE(fc.validateIncoming(addr2, 20));
    TEST_ASSERT_FALSE(fc.validateIncoming(addr3, 30));
    
    TEST_ASSERT_TRUE(fc.validateIncoming(addr1, 11));
    TEST_ASSERT_TRUE(fc.validateIncoming(addr2, 21));
    TEST_ASSERT_TRUE(fc.validateIncoming(addr3, 31));
}

void test_device_auth_set_mode_changes_behavior(void) {
    DeviceAuth auth;
    auth.begin(AuthMode::OPEN);
    
    uint16_t deviceAddr = 0x5555;
    TEST_ASSERT_TRUE(auth.isAuthorized(deviceAddr));
    
    auth.setMode(AuthMode::ALLOWLIST);
    TEST_ASSERT_FALSE(auth.isAuthorized(deviceAddr));
}

void test_device_auth_get_mode_returns_current(void) {
    DeviceAuth auth;
    auth.begin(AuthMode::OPEN);
    TEST_ASSERT_EQUAL(AuthMode::OPEN, auth.getMode());
    
    auth.setMode(AuthMode::ALLOWLIST);
    TEST_ASSERT_EQUAL(AuthMode::ALLOWLIST, auth.getMode());
    
    auth.setMode(AuthMode::LEARNING);
    TEST_ASSERT_EQUAL(AuthMode::LEARNING, auth.getMode());
}

void test_device_auth_device_count_limit(void) {
    DeviceAuth auth;
    auth.begin(AuthMode::ALLOWLIST);
    
    for (uint16_t i = 1; i <= MAX_ALLOWED_DEVICES; i++) {
        TEST_ASSERT_TRUE(auth.addDevice(i));
    }
    
    TEST_ASSERT_EQUAL_UINT8(MAX_ALLOWED_DEVICES, auth.getDeviceCount());
    TEST_ASSERT_FALSE(auth.addDevice(MAX_ALLOWED_DEVICES + 1));
}

void test_device_auth_clear_all_resets(void) {
    DeviceAuth auth;
    auth.begin(AuthMode::ALLOWLIST);
    
    auth.addDevice(0x1111);
    auth.addDevice(0x2222);
    TEST_ASSERT_EQUAL_UINT8(2, auth.getDeviceCount());
    
    auth.removeDevice(0x1111);
    auth.removeDevice(0x2222);
    TEST_ASSERT_EQUAL_UINT8(0, auth.getDeviceCount());
}

void test_device_auth_persist_returns_true(void) {
    DeviceAuth auth;
    auth.begin(AuthMode::ALLOWLIST);
    
    auth.addDevice(0x1234);
    
    // In native build, persist() is stubbed to return true
    TEST_ASSERT_TRUE(auth.persist());
}

void test_device_auth_add_device_updates_existing(void) {
    DeviceAuth auth;
    auth.begin(AuthMode::ALLOWLIST);
    
    uint16_t deviceAddr = 0x6666;
    uint8_t macHash1[4] = {0x11, 0x22, 0x33, 0x44};
    uint8_t macHash2[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint16_t flags1 = 0x0001;
    uint16_t flags2 = 0x0002;
    
    // Add device with first macHash and flags
    TEST_ASSERT_TRUE(auth.addDevice(deviceAddr, macHash1, flags1));
    TEST_ASSERT_EQUAL_UINT8(1, auth.getDeviceCount());
    
    // Add same device with different macHash and flags - should update, not add new entry
    TEST_ASSERT_TRUE(auth.addDevice(deviceAddr, macHash2, flags2));
    TEST_ASSERT_EQUAL_UINT8(1, auth.getDeviceCount()); // Count should still be 1
    
    // Verify it accepts the new macHash
    TEST_ASSERT_TRUE(auth.isAuthorized(deviceAddr, macHash2));
}

void test_device_auth_remove_nonexistent_returns_false(void) {
    DeviceAuth auth;
    auth.begin(AuthMode::ALLOWLIST);
    
    // Try to remove a device that was never added
    TEST_ASSERT_FALSE(auth.removeDevice(0xFFFF));
    TEST_ASSERT_EQUAL_UINT8(0, auth.getDeviceCount());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_frame_counter_outgoing_increments);
    RUN_TEST(test_frame_counter_incoming_sequential_validation);
    RUN_TEST(test_frame_counter_incoming_window_validation);
    RUN_TEST(test_frame_counter_table_full_handling);
    
    RUN_TEST(test_device_auth_open_mode_allows_all);
    RUN_TEST(test_device_auth_allowlist_mode_logic);
    RUN_TEST(test_device_auth_learning_mode_auto_add);
    RUN_TEST(test_device_auth_mac_hash_calculation);

    RUN_TEST(test_frame_counter_persist_returns_true);
    RUN_TEST(test_frame_counter_multiple_peers);
    RUN_TEST(test_device_auth_set_mode_changes_behavior);
    RUN_TEST(test_device_auth_get_mode_returns_current);
    RUN_TEST(test_device_auth_device_count_limit);
    RUN_TEST(test_device_auth_clear_all_resets);
    
    RUN_TEST(test_device_auth_persist_returns_true);
    RUN_TEST(test_device_auth_add_device_updates_existing);
    RUN_TEST(test_device_auth_remove_nonexistent_returns_false);
    
    return UNITY_END();
}
