#include "xmesh/security/SecurityManager.h"
#include <cstring>

#ifdef NATIVE_BUILD
#include "MockFreeRTOS.h"
#else
#include <esp_log.h>
#endif

namespace xmesh {
namespace security {

#ifndef NATIVE_BUILD
static const char* SM_TAG = "SecurityManager";
#endif

static const uint8_t SECURE_PACKET_VERSION = 1;
static const uint8_t DEFAULT_SALT[] = {0x78, 0x6D, 0x65, 0x73, 0x68, 0x2D, 0x73, 0x61, 
                                        0x6C, 0x74, 0x2D, 0x76, 0x31, 0x00, 0x00, 0x00};

bool SecurityManager::begin(SecurityLevel level) {
    level_ = level;
    
    if (!frameCounter_.begin()) {
#ifndef NATIVE_BUILD
        ESP_LOGE(SM_TAG, "FrameCounter init failed");
#endif
        return false;
    }
    
    AuthMode authMode = (level >= SecurityLevel::AUTH_ONLY) ? AuthMode::LEARNING : AuthMode::OPEN;
    if (!deviceAuth_.begin(authMode)) {
#ifndef NATIVE_BUILD
        ESP_LOGE(SM_TAG, "DeviceAuth init failed");
#endif
        return false;
    }
    
    if (!keyManager_.begin()) {
#ifndef NATIVE_BUILD
        ESP_LOGE(SM_TAG, "KeyManager init failed");
#endif
        return false;
    }
    
    if (level >= SecurityLevel::ENCRYPTED && keyManager_.hasValidKey()) {
        uint8_t key[32];
        keyManager_.getPrimaryKey(key);
        if (!payloadCrypto_.begin(key)) {
#ifndef NATIVE_BUILD
            ESP_LOGE(SM_TAG, "PayloadCrypto init failed");
#endif
            memset(key, 0, 32);
            return false;
        }
        memset(key, 0, 32);
    }
    
    initialized_ = true;
#ifndef NATIVE_BUILD
    ESP_LOGI(SM_TAG, "Security initialized at level %d", static_cast<int>(level));
#endif
    return true;
}

bool SecurityManager::setEncryptionKey(const uint8_t* key, uint8_t version) {
    if (key == nullptr || version == 0) return false;
    
    if (!keyManager_.setPrimaryKey(key, version)) return false;
    
    if (payloadCrypto_.isInitialized()) {
        return payloadCrypto_.setKey(key);
    } else {
        return payloadCrypto_.begin(key);
    }
}

bool SecurityManager::setEncryptionKeyFromPassword(const char* password) {
    if (password == nullptr) return false;
    
    uint8_t derivedKey[32];
    if (!keyManager_.deriveKeyFromPassword(password, DEFAULT_SALT, sizeof(DEFAULT_SALT), derivedKey)) {
        return false;
    }
    
    uint8_t version = keyManager_.getCurrentVersion();
    if (version == 0) version = 1;
    else version++;
    
    bool result = setEncryptionKey(derivedKey, version);
    memset(derivedKey, 0, 32);
    
    return result;
}

void SecurityManager::setAuthMode(AuthMode mode) {
    deviceAuth_.setMode(mode);
}

AuthMode SecurityManager::getAuthMode() const {
    return deviceAuth_.getMode();
}

bool SecurityManager::securePayload(uint8_t* payload, size_t* payloadLen, size_t maxLen,
                                     uint16_t srcAddr, uint16_t dstAddr) {
    if (!initialized_) return false;
    if (level_ == SecurityLevel::NONE) return true;
    
    (void)srcAddr;
    (void)dstAddr;
    
    size_t originalLen = *payloadLen;
    size_t requiredLen = originalLen + SECURE_HEADER_SIZE;
    
    if (level_ >= SecurityLevel::ENCRYPTED) {
        requiredLen += CRYPTO_OVERHEAD;
    }
    
    if (requiredLen > maxLen) return false;
    
    memmove(payload + SECURE_HEADER_SIZE, payload, originalLen);
    
    SecurePacketHeader* header = reinterpret_cast<SecurePacketHeader*>(payload);
    header->version = SECURE_PACKET_VERSION;
    header->keyVersion = keyManager_.getCurrentVersion();
    header->frameCounter = frameCounter_.getNextOutgoing();
    
    *payloadLen = originalLen + SECURE_HEADER_SIZE;
    
    if (level_ >= SecurityLevel::ENCRYPTED && payloadCrypto_.isInitialized()) {
        uint8_t* dataStart = payload + SECURE_HEADER_SIZE;
        size_t dataLen = originalLen;
        size_t dataMaxLen = maxLen - SECURE_HEADER_SIZE;
        
        CryptoResult result = payloadCrypto_.encrypt(dataStart, &dataLen, dataMaxLen,
                                                      header->frameCounter,
                                                      payload, SECURE_HEADER_SIZE);
        if (result != CryptoResult::OK) {
            return false;
        }
        
        *payloadLen = SECURE_HEADER_SIZE + dataLen;
    }
    
    return true;
}

bool SecurityManager::verifyAndDecrypt(uint8_t* payload, size_t* payloadLen,
                                        uint16_t srcAddr, uint16_t dstAddr) {
    if (!initialized_) return false;
    if (level_ == SecurityLevel::NONE) return true;
    
    (void)dstAddr;
    
    if (*payloadLen < SECURE_HEADER_SIZE) return false;
    
    SecurePacketHeader* header = reinterpret_cast<SecurePacketHeader*>(payload);
    
    if (header->version != SECURE_PACKET_VERSION) {
        return false;
    }
    
    if (level_ >= SecurityLevel::AUTH_ONLY) {
        if (!deviceAuth_.isAuthorized(srcAddr, nullptr)) {
            return false;
        }
    }
    
    if (!frameCounter_.validateIncoming(srcAddr, header->frameCounter)) {
        return false;
    }
    
    if (level_ >= SecurityLevel::ENCRYPTED) {
        if (!keyManager_.hasKeyVersion(header->keyVersion)) {
            return false;
        }
        
        uint8_t key[32];
        if (!keyManager_.getKeyByVersion(header->keyVersion, key)) {
            return false;
        }
        
        if (header->keyVersion != keyManager_.getCurrentVersion()) {
            payloadCrypto_.setKey(key);
        }
        memset(key, 0, 32);
        
        uint8_t* dataStart = payload + SECURE_HEADER_SIZE;
        size_t dataLen = *payloadLen - SECURE_HEADER_SIZE;
        
        CryptoResult result = payloadCrypto_.decrypt(dataStart, &dataLen,
                                                      payload, SECURE_HEADER_SIZE);
        if (result != CryptoResult::OK) {
            return false;
        }
        
        memmove(payload, dataStart, dataLen);
        *payloadLen = dataLen;
    } else {
        memmove(payload, payload + SECURE_HEADER_SIZE, *payloadLen - SECURE_HEADER_SIZE);
        *payloadLen -= SECURE_HEADER_SIZE;
    }
    
    deviceAuth_.updateLastSeen(srcAddr);
    
    return true;
}

bool SecurityManager::addAuthorizedDevice(uint16_t address, uint16_t flags) {
    return deviceAuth_.addDevice(address, nullptr, flags);
}

bool SecurityManager::removeAuthorizedDevice(uint16_t address) {
    return deviceAuth_.removeDevice(address);
}

bool SecurityManager::persist() {
    bool success = true;
    
    if (!frameCounter_.persist()) success = false;
    if (!deviceAuth_.persist()) success = false;
    if (!keyManager_.persist()) success = false;
    
    return success;
}

}
}
