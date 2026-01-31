#include <unity.h>
#include <cstring>
#include "../../mocks/MockFreeRTOS.h"
#include "../../../../../lib/xmesh-security/src/PayloadCrypto.cpp"

using namespace xmesh::security;

static uint8_t dummyKey[32] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
};

void setUp(void) {
    mock::reset();
}

void tearDown(void) {
}

void test_payload_crypto_begin_initializes(void) {
    PayloadCrypto pc;
    TEST_ASSERT_FALSE(pc.isInitialized());
    
    TEST_ASSERT_TRUE(pc.begin(dummyKey));
    TEST_ASSERT_TRUE(pc.isInitialized());
}

void test_payload_crypto_setkey_updates_key(void) {
    PayloadCrypto pc;
    pc.begin(dummyKey);
    
    uint8_t newKey[32];
    memset(newKey, 0xAA, 32);
    
    TEST_ASSERT_TRUE(pc.setKey(newKey));
    TEST_ASSERT_TRUE(pc.isInitialized());
}

void test_payload_crypto_encrypt_adds_overhead(void) {
    PayloadCrypto pc;
    pc.begin(dummyKey);
    
    uint8_t buffer[64];
    memset(buffer, 0xCC, 64);
    size_t payloadLen = 10;
    
    CryptoResult res = pc.encrypt(buffer, &payloadLen, 64, 100);
    
    TEST_ASSERT_EQUAL(CryptoResult::OK, res);
    TEST_ASSERT_EQUAL(10 + CRYPTO_OVERHEAD, payloadLen);
}

void test_payload_crypto_decrypt_removes_overhead(void) {
    PayloadCrypto pc;
    pc.begin(dummyKey);
    
    uint8_t buffer[64];
    const char* plaintext = "HelloMesh";
    size_t plaintextLen = strlen(plaintext);
    memcpy(buffer, plaintext, plaintextLen);
    
    size_t payloadLen = plaintextLen;
    CryptoResult res = pc.encrypt(buffer, &payloadLen, 64, 123);
    TEST_ASSERT_EQUAL(CryptoResult::OK, res);
    
    res = pc.decrypt(buffer, &payloadLen);
    TEST_ASSERT_EQUAL(CryptoResult::OK, res);
    TEST_ASSERT_EQUAL(plaintextLen, payloadLen);
    TEST_ASSERT_EQUAL_INT8_ARRAY(plaintext, buffer, plaintextLen);
}

void test_payload_crypto_encrypt_fails_uninitialized(void) {
    PayloadCrypto pc;
    uint8_t buffer[64];
    size_t payloadLen = 10;
    
    CryptoResult res = pc.encrypt(buffer, &payloadLen, 64, 100);
    TEST_ASSERT_EQUAL(CryptoResult::ERROR_NOT_INITIALIZED, res);
}

void test_payload_crypto_encrypt_fails_buffer_too_small(void) {
    PayloadCrypto pc;
    pc.begin(dummyKey);
    
    uint8_t buffer[20];
    size_t payloadLen = 10;
    
    CryptoResult res = pc.encrypt(buffer, &payloadLen, 20, 100);
    TEST_ASSERT_EQUAL(CryptoResult::ERROR_BUFFER_TOO_SMALL, res);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_payload_crypto_begin_initializes);
    RUN_TEST(test_payload_crypto_setkey_updates_key);
    RUN_TEST(test_payload_crypto_encrypt_adds_overhead);
    RUN_TEST(test_payload_crypto_decrypt_removes_overhead);
    RUN_TEST(test_payload_crypto_encrypt_fails_uninitialized);
    RUN_TEST(test_payload_crypto_encrypt_fails_buffer_too_small);
    return UNITY_END();
}
