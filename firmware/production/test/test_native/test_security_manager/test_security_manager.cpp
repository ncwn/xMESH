#include <unity.h>
#include <cstring>
#include "../../mocks/MockFreeRTOS.h"

#include "../../../../../lib/xmesh-security/src/PayloadCrypto.cpp"
#include "../../../../../lib/xmesh-security/src/KeyManager.cpp"
#include "../../../../../lib/xmesh-security/src/FrameCounter.cpp"
#include "../../../../../lib/xmesh-security/src/DeviceAuth.cpp"
#include "../../../../../lib/xmesh-security/src/SecurityManager.cpp"

using namespace xmesh::security;

void setUp(void) {
    mock::reset();
}

void tearDown(void) {
}

void test_security_manager_begin_initializes(void) {
    SecurityManager& sm = SecurityManager::getInstance();
    TEST_ASSERT_TRUE(sm.begin(SecurityLevel::NONE));
}

void test_security_manager_set_security_level(void) {
    SecurityManager& sm = SecurityManager::getInstance();
    sm.begin(SecurityLevel::NONE);
    
    sm.setSecurityLevel(SecurityLevel::AUTH_ONLY);
    TEST_ASSERT_EQUAL(SecurityLevel::AUTH_ONLY, sm.getSecurityLevel());
    
    sm.setSecurityLevel(SecurityLevel::ENCRYPTED);
    TEST_ASSERT_EQUAL(SecurityLevel::ENCRYPTED, sm.getSecurityLevel());
}

void test_security_manager_get_security_level(void) {
    SecurityManager& sm = SecurityManager::getInstance();
    sm.begin(SecurityLevel::FULL);
    TEST_ASSERT_EQUAL(SecurityLevel::FULL, sm.getSecurityLevel());
}

void test_security_manager_set_auth_mode(void) {
    SecurityManager& sm = SecurityManager::getInstance();
    sm.begin(SecurityLevel::NONE);
    
    sm.setAuthMode(AuthMode::ALLOWLIST);
    TEST_ASSERT_EQUAL(AuthMode::ALLOWLIST, sm.getAuthMode());
    
    sm.setAuthMode(AuthMode::OPEN);
    TEST_ASSERT_EQUAL(AuthMode::OPEN, sm.getAuthMode());
}

void test_security_manager_secure_payload_none_passthrough(void) {
    SecurityManager& sm = SecurityManager::getInstance();
    sm.begin(SecurityLevel::NONE);
    
    uint8_t payload[32] = "Hello World";
    size_t payloadLen = strlen((char*)payload);
    size_t originalLen = payloadLen;
    
    TEST_ASSERT_TRUE(sm.securePayload(payload, &payloadLen, sizeof(payload), 0x0001, 0x0002));
    TEST_ASSERT_EQUAL(originalLen, payloadLen);
    TEST_ASSERT_EQUAL_STRING("Hello World", (char*)payload);
}

void test_security_manager_secure_payload_encrypted_adds_overhead(void) {
    SecurityManager& sm = SecurityManager::getInstance();
    sm.begin(SecurityLevel::ENCRYPTED);
    
    uint8_t dummyKey[32] = {0};
    sm.setEncryptionKey(dummyKey, 1);
    
    uint8_t payload[64] = "Secret Message";
    size_t payloadLen = strlen((char*)payload);
    size_t originalLen = payloadLen;
    
    TEST_ASSERT_TRUE(sm.securePayload(payload, &payloadLen, sizeof(payload), 0x0001, 0x0002));
    
    TEST_ASSERT_EQUAL(originalLen + SECURE_HEADER_SIZE + CRYPTO_OVERHEAD, payloadLen);
}

void test_security_manager_verify_and_decrypt_none_passthrough(void) {
    SecurityManager& sm = SecurityManager::getInstance();
    sm.begin(SecurityLevel::NONE);
    
    uint8_t payload[32] = "Plaintext";
    size_t payloadLen = strlen((char*)payload);
    size_t originalLen = payloadLen;
    
    TEST_ASSERT_TRUE(sm.verifyAndDecrypt(payload, &payloadLen, 0x0002, 0x0001));
    TEST_ASSERT_EQUAL(originalLen, payloadLen);
    TEST_ASSERT_EQUAL_STRING("Plaintext", (char*)payload);
}

void test_security_manager_add_remove_device(void) {
    SecurityManager& sm = SecurityManager::getInstance();
    sm.begin(SecurityLevel::AUTH_ONLY);
    
    TEST_ASSERT_EQUAL(0, sm.getAuthorizedDeviceCount());
    
    sm.addAuthorizedDevice(0x1234);
    TEST_ASSERT_EQUAL(1, sm.getAuthorizedDeviceCount());
    
    sm.removeAuthorizedDevice(0x1234);
    TEST_ASSERT_EQUAL(0, sm.getAuthorizedDeviceCount());
}

void test_security_manager_get_frame_counter_out_returns_current(void) {
    SecurityManager& sm = SecurityManager::getInstance();
    sm.begin(SecurityLevel::ENCRYPTED);
    
    uint8_t dummyKey[32] = {0};
    sm.setEncryptionKey(dummyKey, 1);
    
    uint8_t payload[64] = "Test Message";
    size_t payloadLen = strlen((char*)payload);
    
    // Secure a payload, which increments the frame counter
    sm.securePayload(payload, &payloadLen, sizeof(payload), 0x0001, 0x0002);
    
    // getFrameCounterOut should return the next counter value
    uint32_t counter = sm.getFrameCounterOut();
    TEST_ASSERT_GREATER_THAN_UINT32(0, counter);
}

void test_security_manager_get_frame_counter_reference(void) {
    SecurityManager& sm = SecurityManager::getInstance();
    sm.begin(SecurityLevel::NONE);
    
    // Verify getFrameCounter() returns a valid reference
    FrameCounter& fc = sm.getFrameCounter();
    
    // Should be able to call methods on the reference
    uint32_t counter = fc.getCurrentOutgoing();
    // Just verify we can call it without error - value could be 0 or higher
    (void)counter; // Suppress unused variable warning
    TEST_ASSERT_TRUE(true); // Test passes if we got here
}

void test_security_manager_get_key_manager_reference(void) {
    SecurityManager& sm = SecurityManager::getInstance();
    sm.begin(SecurityLevel::NONE);
    
    // Verify getKeyManager() returns a valid reference
    KeyManager& km = sm.getKeyManager();
    
    // Should be able to call methods on the reference
    uint8_t version = km.getCurrentVersion();
    // Just verify we can call it without error - value could be 0 or higher
    (void)version; // Suppress unused variable warning
    TEST_ASSERT_TRUE(true); // Test passes if we got here
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_security_manager_begin_initializes);
    RUN_TEST(test_security_manager_set_security_level);
    RUN_TEST(test_security_manager_get_security_level);
    RUN_TEST(test_security_manager_set_auth_mode);
    RUN_TEST(test_security_manager_secure_payload_none_passthrough);
    RUN_TEST(test_security_manager_secure_payload_encrypted_adds_overhead);
    RUN_TEST(test_security_manager_verify_and_decrypt_none_passthrough);
    RUN_TEST(test_security_manager_add_remove_device);
    RUN_TEST(test_security_manager_get_frame_counter_out_returns_current);
    RUN_TEST(test_security_manager_get_frame_counter_reference);
    RUN_TEST(test_security_manager_get_key_manager_reference);
    
    return UNITY_END();
}
