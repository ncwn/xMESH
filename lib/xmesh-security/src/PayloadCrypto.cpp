#include "xmesh/security/PayloadCrypto.h"
#include <cstring>

#ifdef NATIVE_BUILD
#include "MockFreeRTOS.h"
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <mbedtls/gcm.h>
#include <esp_random.h>
#include <esp_log.h>
#endif

namespace xmesh {
namespace security {

static const char* PC_TAG = "PayloadCrypto";

PayloadCrypto::PayloadCrypto() : initialized_(false), mutex_(nullptr) {
    memset(key_, 0, AES_KEY_SIZE);
    memset(nodeRandom_, 0, 8);
}

PayloadCrypto::~PayloadCrypto() {
    memset(key_, 0, AES_KEY_SIZE);
#ifndef NATIVE_BUILD
    if (mutex_ != nullptr) {
        vSemaphoreDelete(static_cast<SemaphoreHandle_t>(mutex_));
    }
#endif
}

bool PayloadCrypto::begin(const uint8_t* key) {
    if (key == nullptr) return false;
    
#ifndef NATIVE_BUILD
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        ESP_LOGE(PC_TAG, "Failed to create mutex");
        return false;
    }
    
    esp_fill_random(nodeRandom_, 8);
#endif
    
    memcpy(key_, key, AES_KEY_SIZE);
    initialized_ = true;
    return true;
}

bool PayloadCrypto::setKey(const uint8_t* key) {
    if (key == nullptr) return false;
    
#ifndef NATIVE_BUILD
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
#endif
    
    memcpy(key_, key, AES_KEY_SIZE);
    
#ifndef NATIVE_BUILD
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
    
    return true;
}

void PayloadCrypto::generateNonce(uint32_t frameCounter, uint8_t* nonce) {
    nonce[0] = (frameCounter >> 24) & 0xFF;
    nonce[1] = (frameCounter >> 16) & 0xFF;
    nonce[2] = (frameCounter >> 8) & 0xFF;
    nonce[3] = frameCounter & 0xFF;
    memcpy(nonce + 4, nodeRandom_, 8);
}

CryptoResult PayloadCrypto::encrypt(uint8_t* payload, size_t* payloadLen, size_t maxLen,
                                     uint32_t frameCounter, const uint8_t* aad, size_t aadLen) {
    if (!initialized_) return CryptoResult::ERROR_NOT_INITIALIZED;
    if (payload == nullptr || payloadLen == nullptr) return CryptoResult::ERROR_INVALID_INPUT;
    
    size_t plaintextLen = *payloadLen;
    size_t requiredLen = plaintextLen + CRYPTO_OVERHEAD;
    
    if (requiredLen > maxLen) return CryptoResult::ERROR_BUFFER_TOO_SMALL;
    if (plaintextLen > MAX_PLAINTEXT_SIZE) return CryptoResult::ERROR_BUFFER_TOO_SMALL;
    
#ifndef NATIVE_BUILD
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
#endif
    
    uint8_t nonce[AES_NONCE_SIZE];
    generateNonce(frameCounter, nonce);
    
    uint8_t fullTag[AES_FULL_TAG_SIZE];
    
    // Copy plaintext to temp buffer to avoid overlap with ciphertext output
    uint8_t tempPlaintext[MAX_PLAINTEXT_SIZE];
    memcpy(tempPlaintext, payload, plaintextLen);
    
    // Encrypt from temp buffer to payload+nonce position (no overlap)
    CryptoResult result = doEncrypt(tempPlaintext, plaintextLen, nonce, aad, aadLen, 
                                     payload + AES_NONCE_SIZE, fullTag);
    
    if (result == CryptoResult::OK) {
        // Final layout: [nonce (12)] [ciphertext (plaintextLen)] [tag (4)]
        memcpy(payload, nonce, AES_NONCE_SIZE);
        // Ciphertext is already at payload + AES_NONCE_SIZE from doEncrypt
        memcpy(payload + AES_NONCE_SIZE + plaintextLen, fullTag, AES_TAG_SIZE);
        *payloadLen = requiredLen;
    }
    
#ifndef NATIVE_BUILD
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
    
    return result;
}

CryptoResult PayloadCrypto::decrypt(uint8_t* payload, size_t* payloadLen,
                                     const uint8_t* aad, size_t aadLen) {
    if (!initialized_) return CryptoResult::ERROR_NOT_INITIALIZED;
    if (payload == nullptr || payloadLen == nullptr) return CryptoResult::ERROR_INVALID_INPUT;
    
    size_t encryptedLen = *payloadLen;
    if (encryptedLen < CRYPTO_OVERHEAD) return CryptoResult::ERROR_INVALID_INPUT;
    
    size_t ciphertextLen = encryptedLen - CRYPTO_OVERHEAD;
    
#ifndef NATIVE_BUILD
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
#endif
    
    uint8_t* nonce = payload;
    uint8_t* ciphertext = payload + AES_NONCE_SIZE;
    uint8_t* tag = payload + AES_NONCE_SIZE + ciphertextLen;
    
    uint8_t fullTag[AES_FULL_TAG_SIZE];
    memset(fullTag, 0, AES_FULL_TAG_SIZE);
    memcpy(fullTag, tag, AES_TAG_SIZE);
    
    CryptoResult result = doDecrypt(ciphertext, ciphertextLen, nonce, fullTag, 
                                     aad, aadLen, payload);
    
    if (result == CryptoResult::OK) {
        *payloadLen = ciphertextLen;
    }
    
#ifndef NATIVE_BUILD
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
    
    return result;
}

CryptoResult PayloadCrypto::doEncrypt(const uint8_t* plaintext, size_t plaintextLen,
                                       const uint8_t* nonce, const uint8_t* aad, size_t aadLen,
                                       uint8_t* ciphertext, uint8_t* tag) {
#ifndef NATIVE_BUILD
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key_, 256);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        return CryptoResult::ERROR_INVALID_KEY;
    }
    
    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plaintextLen,
                                    nonce, AES_NONCE_SIZE,
                                    aad, aadLen,
                                    plaintext, ciphertext,
                                    AES_FULL_TAG_SIZE, tag);
    
    mbedtls_gcm_free(&gcm);
    
    return (ret == 0) ? CryptoResult::OK : CryptoResult::ERROR_ENCRYPTION_FAILED;
#else
    (void)nonce; (void)aad; (void)aadLen;
    memcpy(ciphertext, plaintext, plaintextLen);
    memset(tag, 0xAB, AES_FULL_TAG_SIZE);
    return CryptoResult::OK;
#endif
}

CryptoResult PayloadCrypto::doDecrypt(const uint8_t* ciphertext, size_t ciphertextLen,
                                       const uint8_t* nonce, const uint8_t* tag,
                                       const uint8_t* aad, size_t aadLen,
                                       uint8_t* plaintext) {
#ifndef NATIVE_BUILD
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key_, 256);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        return CryptoResult::ERROR_INVALID_KEY;
    }
    
    ret = mbedtls_gcm_auth_decrypt(&gcm, ciphertextLen,
                                   nonce, AES_NONCE_SIZE,
                                   aad, aadLen,
                                   tag, AES_TAG_SIZE,
                                   ciphertext, plaintext);
    
    mbedtls_gcm_free(&gcm);
    
    if (ret == MBEDTLS_ERR_GCM_AUTH_FAILED) {
        return CryptoResult::ERROR_AUTH_FAILED;
    }
    return (ret == 0) ? CryptoResult::OK : CryptoResult::ERROR_DECRYPTION_FAILED;
#else
    (void)nonce; (void)tag; (void)aad; (void)aadLen;
    memcpy(plaintext, ciphertext, ciphertextLen);
    return CryptoResult::OK;
#endif
}

}
}
