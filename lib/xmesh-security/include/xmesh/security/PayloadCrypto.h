/**
 * @file PayloadCrypto.h
 * @brief AES-256-GCM payload encryption/decryption for mesh packets
 * 
 * Provides authenticated encryption for mesh packet payloads using:
 * - AES-256-GCM (hardware accelerated on ESP32-S3)
 * - 12-byte nonce (4-byte counter + 8-byte random)
 * - 16-byte authentication tag (truncated to 4 bytes for space efficiency)
 * 
 * Encrypted packet format:
 * [Original Header (unchanged)] [Nonce:12] [Ciphertext:N] [Tag:4]
 */

#ifndef XMESH_SECURITY_PAYLOAD_CRYPTO_H
#define XMESH_SECURITY_PAYLOAD_CRYPTO_H

#include <cstdint>
#include <cstddef>

namespace xmesh {
namespace security {

// Key sizes
constexpr size_t AES_KEY_SIZE = 32;        // 256 bits
constexpr size_t AES_NONCE_SIZE = 12;      // 96 bits (GCM standard)
constexpr size_t AES_TAG_SIZE = 4;         // Truncated to 4 bytes (32 bits)
constexpr size_t AES_FULL_TAG_SIZE = 16;   // Full 128-bit tag for computation

// Overhead added by encryption: nonce + tag
constexpr size_t CRYPTO_OVERHEAD = AES_NONCE_SIZE + AES_TAG_SIZE;  // 16 bytes

// Maximum plaintext size (considering 91-byte max payload - overhead)
constexpr size_t MAX_PLAINTEXT_SIZE = 75;  // 91 - 16 = 75 bytes

/**
 * @brief Result codes for crypto operations
 */
enum class CryptoResult : uint8_t {
    OK = 0,
    ERROR_NOT_INITIALIZED,
    ERROR_INVALID_KEY,
    ERROR_BUFFER_TOO_SMALL,
    ERROR_ENCRYPTION_FAILED,
    ERROR_DECRYPTION_FAILED,
    ERROR_AUTH_FAILED,         // Tag verification failed
    ERROR_INVALID_INPUT
};

/**
 * @brief AES-256-GCM payload encryption/decryption
 * 
 * Thread-safe via internal mutex.
 * Uses ESP32 hardware AES acceleration when available.
 */
class PayloadCrypto {
public:
    PayloadCrypto();
    ~PayloadCrypto();
    
    /**
     * @brief Initialize with encryption key
     * @param key 32-byte AES-256 key
     * @return true if initialized successfully
     */
    bool begin(const uint8_t* key);
    
    /**
     * @brief Update encryption key (for key rotation)
     * @param key New 32-byte key
     * @return true if key updated successfully
     */
    bool setKey(const uint8_t* key);
    
    /**
     * @brief Encrypt payload in place (expands buffer)
     * 
     * @param payload Input plaintext, output: [nonce][ciphertext][tag]
     * @param payloadLen Input length, output: new length
     * @param maxLen Maximum buffer size (must accommodate CRYPTO_OVERHEAD)
     * @param frameCounter Frame counter for nonce generation
     * @param aad Additional authenticated data (e.g., packet header), can be NULL
     * @param aadLen Length of AAD
     * @return CryptoResult::OK on success
     */
    CryptoResult encrypt(uint8_t* payload, size_t* payloadLen, size_t maxLen,
                         uint32_t frameCounter, const uint8_t* aad = nullptr, size_t aadLen = 0);
    
    /**
     * @brief Decrypt payload in place (shrinks buffer)
     * 
     * @param payload Input: [nonce][ciphertext][tag], output: plaintext
     * @param payloadLen Input/output length
     * @param aad Additional authenticated data (must match encryption), can be NULL
     * @param aadLen Length of AAD
     * @return CryptoResult::OK on success, ERROR_AUTH_FAILED if tampered
     */
    CryptoResult decrypt(uint8_t* payload, size_t* payloadLen,
                         const uint8_t* aad = nullptr, size_t aadLen = 0);
    
    /**
     * @brief Check if crypto is initialized with a valid key
     */
    bool isInitialized() const { return initialized_; }

private:
    uint8_t key_[AES_KEY_SIZE];
    bool initialized_;
    void* mutex_;
    
    // Random bytes for nonce uniqueness
    uint8_t nodeRandom_[8];
    
    /**
     * @brief Generate nonce from frame counter and random bytes
     */
    void generateNonce(uint32_t frameCounter, uint8_t* nonce);
    
    /**
     * @brief Perform AES-GCM encryption
     */
    CryptoResult doEncrypt(const uint8_t* plaintext, size_t plaintextLen,
                           const uint8_t* nonce, const uint8_t* aad, size_t aadLen,
                           uint8_t* ciphertext, uint8_t* tag);
    
    /**
     * @brief Perform AES-GCM decryption with authentication
     */
    CryptoResult doDecrypt(const uint8_t* ciphertext, size_t ciphertextLen,
                           const uint8_t* nonce, const uint8_t* tag,
                           const uint8_t* aad, size_t aadLen,
                           uint8_t* plaintext);
};

} // namespace security
} // namespace xmesh

#endif // XMESH_SECURITY_PAYLOAD_CRYPTO_H
