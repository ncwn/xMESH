#include <unity.h>
#include <cstring>
#include "../../mocks/MockFreeRTOS.h"
#include "../../../lib/xmesh-security/src/KeyManager.cpp"

using namespace xmesh::security;

void setUp(void) {
    mock::reset();
}

void tearDown(void) {}

void test_key_manager_begin_initializes(void) {
    KeyManager km;
    TEST_ASSERT_TRUE(km.begin());
}

void test_key_manager_set_primary_key(void) {
    KeyManager km;
    km.begin();
    
    uint8_t testKey[KEY_SIZE];
    memset(testKey, 0xAA, KEY_SIZE);
    uint8_t version = 5;
    
    TEST_ASSERT_TRUE(km.setPrimaryKey(testKey, version));
    TEST_ASSERT_TRUE(km.hasValidKey());
    TEST_ASSERT_EQUAL_UINT8(version, km.getCurrentVersion());
}

void test_key_manager_get_primary_key(void) {
    KeyManager km;
    km.begin();
    
    uint8_t testKey[KEY_SIZE];
    memset(testKey, 0xBB, KEY_SIZE);
    uint8_t version = 10;
    km.setPrimaryKey(testKey, version);
    
    uint8_t retrievedKey[KEY_SIZE];
    uint8_t retrievedVersion = km.getPrimaryKey(retrievedKey);
    
    TEST_ASSERT_EQUAL_UINT8(version, retrievedVersion);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(testKey, retrievedKey, KEY_SIZE);
}

void test_key_manager_version_management(void) {
    KeyManager km;
    km.begin();
    
    TEST_ASSERT_EQUAL_UINT8(0, km.getCurrentVersion());
    
    uint8_t testKey[KEY_SIZE];
    memset(testKey, 0xCC, KEY_SIZE);
    km.setPrimaryKey(testKey, 42);
    
    TEST_ASSERT_EQUAL_UINT8(42, km.getCurrentVersion());
}

void test_key_manager_derive_key_from_password(void) {
    KeyManager km;
    km.begin();
    
    const char* password = "secret_password";
    uint8_t salt[16] = {0};
    uint8_t derivedKey[KEY_SIZE];
    
    TEST_ASSERT_TRUE(km.deriveKeyFromPassword(password, salt, sizeof(salt), derivedKey));
    
    // Check XOR mock: keyOut[i] = password[i % strlen(password)] ^ 0x55;
    size_t passLen = strlen(password);
    for (size_t i = 0; i < KEY_SIZE; i++) {
        uint8_t expected = password[i % passLen] ^ 0x55;
        TEST_ASSERT_EQUAL_UINT8(expected, derivedKey[i]);
    }
}

void test_key_manager_clear_all(void) {
    KeyManager km;
    km.begin();
    
    uint8_t testKey[KEY_SIZE];
    memset(testKey, 0xDD, KEY_SIZE);
    km.setPrimaryKey(testKey, 1);
    
    TEST_ASSERT_TRUE(km.hasValidKey());
    
    km.clearAll();
    
    TEST_ASSERT_FALSE(km.hasValidKey());
    TEST_ASSERT_EQUAL_UINT8(0, km.getCurrentVersion());
    
    uint8_t retrievedKey[KEY_SIZE];
    uint8_t ver = km.getPrimaryKey(retrievedKey);
    TEST_ASSERT_EQUAL_UINT8(0, ver);
}

void test_key_manager_has_key_version(void) {
    KeyManager km;
    km.begin();
    
    uint8_t testKey[KEY_SIZE];
    memset(testKey, 0xEE, KEY_SIZE);
    km.setPrimaryKey(testKey, 7);
    
    TEST_ASSERT_TRUE(km.hasKeyVersion(7));
    TEST_ASSERT_FALSE(km.hasKeyVersion(8));
    TEST_ASSERT_FALSE(km.hasKeyVersion(0));
}

void test_key_manager_get_key_by_version(void) {
    KeyManager km;
    km.begin();
    
    uint8_t testKey[KEY_SIZE];
    memset(testKey, 0xFF, KEY_SIZE);
    km.setPrimaryKey(testKey, 99);
    
    uint8_t retrievedKey[KEY_SIZE];
    TEST_ASSERT_TRUE(km.getKeyByVersion(99, retrievedKey));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(testKey, retrievedKey, KEY_SIZE);
    
    TEST_ASSERT_FALSE(km.getKeyByVersion(1, retrievedKey));
}

void test_key_manager_persist_returns_true(void) {
    KeyManager km;
    km.begin();
    
    uint8_t testKey[KEY_SIZE];
    memset(testKey, 0x12, KEY_SIZE);
    km.setPrimaryKey(testKey, 1);
    
    // In native build, persist() is stubbed to return true
    TEST_ASSERT_TRUE(km.persist());
}

void test_key_manager_set_primary_key_null_rejected(void) {
    KeyManager km;
    km.begin();
    
    // Setting a null key should return false
    TEST_ASSERT_FALSE(km.setPrimaryKey(nullptr, 1));
    TEST_ASSERT_FALSE(km.hasValidKey());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_key_manager_begin_initializes);
    RUN_TEST(test_key_manager_set_primary_key);
    RUN_TEST(test_key_manager_get_primary_key);
    RUN_TEST(test_key_manager_version_management);
    RUN_TEST(test_key_manager_derive_key_from_password);
    RUN_TEST(test_key_manager_clear_all);
    RUN_TEST(test_key_manager_has_key_version);
    RUN_TEST(test_key_manager_get_key_by_version);
    RUN_TEST(test_key_manager_persist_returns_true);
    RUN_TEST(test_key_manager_set_primary_key_null_rejected);
    return UNITY_END();
}
