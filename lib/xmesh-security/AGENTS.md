# lib/xmesh-security - Encryption & Auth

## PURPOSE

Optional AES-CTR encryption, frame counter replay protection, and device authorization.

## ARCHITECTURE

`SecurityManager` is the **only public interface**. It composes:
- `PayloadCrypto` - AES-CTR encryption/decryption
- `KeyManager` - Key derivation (PBKDF2), NVS persistence
- `FrameCounter` - Monotonic counter, replay window tracking
- `DeviceAuth` - Whitelist/blacklist by node address

## SECURITY LEVELS

```cpp
enum class SecurityLevel : uint8_t {
    NONE = 0,      // Plaintext
    AUTH_ONLY = 1, // Device auth check only
    ENCRYPTED = 2, // AES + auth
    FULL = 3       // AES + auth + frame counter
};
```

## USAGE

```cpp
auto& security = xmesh::security::SecurityManager::getInstance();
security.begin(SecurityLevel::ENCRYPTED);
security.setEncryptionKeyFromPassword("my-secret");

// Encrypt outgoing
security.securePayload(buffer, &len, maxLen, srcAddr, dstAddr);

// Decrypt incoming
security.verifyAndDecrypt(buffer, &len, srcAddr, dstAddr);
```

## FRAME COUNTER

- 32-bit monotonic counter per node
- Stored in NVS, survives reboot
- Window-based replay detection (default: 32 packets)
- Rejects out-of-order beyond window

## ANTI-PATTERNS

- **DO NOT** include `PayloadCrypto.h` directly - use `SecurityManager`
- **DO NOT** hardcode keys - use `setEncryptionKeyFromPassword()`
- **DO NOT** skip `persist()` after config changes

## TESTS

`firmware/production/test/test_native/test_security_manager/` covers full API.
