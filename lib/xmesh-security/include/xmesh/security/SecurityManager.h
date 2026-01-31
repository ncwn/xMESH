#ifndef XMESH_SECURITY_MANAGER_H
#define XMESH_SECURITY_MANAGER_H

#include "xmesh/security/FrameCounter.h"
#include "xmesh/security/PayloadCrypto.h"
#include "xmesh/security/DeviceAuth.h"
#include "xmesh/security/KeyManager.h"
#include <cstdint>
#include <cstddef>

namespace xmesh {
namespace security {

enum class SecurityLevel : uint8_t {
    NONE = 0,
    AUTH_ONLY = 1,
    ENCRYPTED = 2,
    FULL = 3
};

struct SecurePacketHeader {
    uint8_t version;
    uint8_t keyVersion;
    uint32_t frameCounter;
} __attribute__((packed));

constexpr size_t SECURE_HEADER_SIZE = sizeof(SecurePacketHeader);

class SecurityManager {
public:
    static SecurityManager& getInstance() {
        static SecurityManager instance;
        return instance;
    }
    
    bool begin(SecurityLevel level = SecurityLevel::NONE);
    
    bool setEncryptionKey(const uint8_t* key, uint8_t version);
    bool setEncryptionKeyFromPassword(const char* password);
    
    void setSecurityLevel(SecurityLevel level) { level_ = level; }
    SecurityLevel getSecurityLevel() const { return level_; }
    
    void setAuthMode(AuthMode mode);
    AuthMode getAuthMode() const;
    
    bool securePayload(uint8_t* payload, size_t* payloadLen, size_t maxLen,
                       uint16_t srcAddr, uint16_t dstAddr);
    
    bool verifyAndDecrypt(uint8_t* payload, size_t* payloadLen,
                          uint16_t srcAddr, uint16_t dstAddr);
    
    bool addAuthorizedDevice(uint16_t address, uint16_t flags = 0);
    bool removeAuthorizedDevice(uint16_t address);
    
    bool persist();
    
    uint32_t getFrameCounterOut() const { return frameCounter_.getCurrentOutgoing(); }
    uint32_t getReplayRejectCount() const { return frameCounter_.getReplayRejectCount(); }
    uint8_t getAuthorizedDeviceCount() const { return deviceAuth_.getDeviceCount(); }
    
    FrameCounter& getFrameCounter() { return frameCounter_; }
    KeyManager& getKeyManager() { return keyManager_; }

private:
    SecurityManager() : level_(SecurityLevel::NONE), initialized_(false) {}
    ~SecurityManager() = default;
    SecurityManager(const SecurityManager&) = delete;
    SecurityManager& operator=(const SecurityManager&) = delete;
    
    SecurityLevel level_;
    bool initialized_;
    
    FrameCounter frameCounter_;
    DeviceAuth deviceAuth_;
    PayloadCrypto payloadCrypto_;
    KeyManager keyManager_;
};

}
}

#endif
