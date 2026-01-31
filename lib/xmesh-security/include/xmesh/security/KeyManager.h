/**
 * @file KeyManager.h
 * @brief Cryptographic key management with rotation support
 * 
 * Manages encryption keys for the mesh network:
 * - Primary key for current encryption
 * - Previous key for decryption during rotation
 * - Key versioning for coordinated rotation
 * - Secure storage in NVS (optionally encrypted)
 * 
 * Key rotation workflow:
 * 1. Generate new key (or receive from gateway)
 * 2. Announce key version change to mesh
 * 3. Nodes switch to new key for TX, accept both for RX
 * 4. After grace period, old key is discarded
 */

#ifndef XMESH_SECURITY_KEY_MANAGER_H
#define XMESH_SECURITY_KEY_MANAGER_H

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace xmesh {
namespace security {

// Key configuration
constexpr size_t KEY_SIZE = 32;           // 256-bit keys
constexpr uint32_t KEY_ROTATION_GRACE_MS = 300000;  // 5 minutes grace period

/**
 * @brief Key slot for multi-key support
 */
struct KeySlot {
    uint8_t key[KEY_SIZE];
    uint8_t version;          // Key version (1-255, 0 = invalid)
    uint32_t activeSinceMs;   // When this key became active
    bool valid;
    
    KeySlot() : version(0), activeSinceMs(0), valid(false) {
        memset(key, 0, KEY_SIZE);
    }
    
    void clear() {
        memset(key, 0, KEY_SIZE);
        version = 0;
        activeSinceMs = 0;
        valid = false;
    }
};

/**
 * @brief Key manager for mesh encryption
 */
class KeyManager {
public:
    KeyManager();
    ~KeyManager();
    
    /**
     * @brief Initialize key manager
     * @return true if initialized (keys loaded from NVS if available)
     */
    bool begin();
    
    /**
     * @brief Set the primary encryption key
     * @param key 32-byte key
     * @param version Key version number (1-255)
     * @return true if key set successfully
     */
    bool setPrimaryKey(const uint8_t* key, uint8_t version);
    
    /**
     * @brief Get current primary key for encryption
     * @param keyOut Buffer for 32-byte key
     * @return Key version, or 0 if no key set
     */
    uint8_t getPrimaryKey(uint8_t* keyOut) const;
    
    /**
     * @brief Get key by version (for decryption)
     * @param version Key version to find
     * @param keyOut Buffer for 32-byte key
     * @return true if key found
     */
    bool getKeyByVersion(uint8_t version, uint8_t* keyOut) const;
    
#ifdef XMESH_ENABLE_KEY_ROTATION
    /**
     * @brief Get previous key version (for rotation grace period)
     * @return Previous version, or 0 if none
     */
    uint8_t getPreviousVersion() const;
    
    /**
     * @brief Generate a random 256-bit key
     * @param keyOut Buffer for 32-byte key
     * @return true if generated successfully
     */
    bool generateRandomKey(uint8_t* keyOut);
    
    /**
     * @brief Rotate to a new key
     * @param newKey 32-byte new key
     * @param newVersion New key version
     * @return true if rotation started
     */
    bool rotateKey(const uint8_t* newKey, uint8_t newVersion);
#endif
    
    /**
     * @brief Check if a key version is known
     * @param version Key version to check
     * @return true if we have this key version
     */
    bool hasKeyVersion(uint8_t version) const;
    
    /**
     * @brief Get current primary key version
     * @return Version number, or 0 if no key set
     */
    uint8_t getCurrentVersion() const;
    
    /**
     * @brief Derive key from password/passphrase using PBKDF2
     * @param password Password string
     * @param salt Salt bytes (16 recommended)
     * @param saltLen Salt length
     * @param keyOut Buffer for 32-byte derived key
     * @return true if derived successfully
     */
    bool deriveKeyFromPassword(const char* password, const uint8_t* salt, 
                               size_t saltLen, uint8_t* keyOut);
    
    /**
     * @brief Persist keys to NVS
     * @return true if persisted
     */
    bool persist();
    
    /**
     * @brief Clear all keys (secure erase)
     */
    void clearAll();
    
    /**
     * @brief Check if any valid key is configured
     */
    bool hasValidKey() const;
    
    /**
     * @brief Clean up old keys past grace period
     */
    void cleanupOldKeys();

private:
    KeySlot primaryKey_;
    KeySlot previousKey_;
    bool initialized_;
    void* mutex_;
    
    /**
     * @brief Secure clear of key material
     */
    void secureClear(uint8_t* data, size_t len);
    
    /**
     * @brief Load keys from NVS
     */
    bool loadFromNVS();
    
    /**
     * @brief Save keys to NVS
     */
    bool saveToNVS();
};

} // namespace security
} // namespace xmesh

#endif // XMESH_SECURITY_KEY_MANAGER_H
