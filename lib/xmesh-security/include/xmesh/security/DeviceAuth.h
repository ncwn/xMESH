/**
 * @file DeviceAuth.h
 * @brief Device authentication and allowlist management
 * 
 * Provides device-level authentication for the mesh network:
 * - Allowlist of authorized device addresses
 * - Optional MAC address binding (address spoofing protection)
 * - Challenge-response authentication (future)
 * 
 * Security model:
 * - In ALLOWLIST mode: only pre-registered devices can join
 * - In LEARNING mode: automatically adds new devices (initial setup)
 * - In OPEN mode: all devices allowed (development only)
 */

#ifndef XMESH_SECURITY_DEVICE_AUTH_H
#define XMESH_SECURITY_DEVICE_AUTH_H

#include <cstdint>

namespace xmesh {
namespace security {

// Maximum devices in allowlist
constexpr uint8_t MAX_ALLOWED_DEVICES = 64;

/**
 * @brief Authentication mode
 */
enum class AuthMode : uint8_t {
    OPEN = 0,       // All devices allowed (no authentication)
    LEARNING = 1,   // Auto-add new devices to allowlist
    ALLOWLIST = 2   // Only allowlisted devices allowed
};

/**
 * @brief Device entry in allowlist
 */
struct DeviceEntry {
    uint16_t address;           // LoRa mesh address
    uint8_t macHash[4];         // First 4 bytes of SHA256(MAC address)
    uint32_t firstSeenMs;       // First seen timestamp
    uint32_t lastSeenMs;        // Last seen timestamp
    uint16_t flags;             // Device flags (gateway, trusted, etc.)
    bool valid;                 // Entry is in use
    
    DeviceEntry() : address(0), firstSeenMs(0), lastSeenMs(0), flags(0), valid(false) {
        macHash[0] = macHash[1] = macHash[2] = macHash[3] = 0;
    }
};

// Device flags (reserved for future use)
constexpr uint16_t DEVICE_FLAG_GATEWAY = 0x0001;  // Reserved
constexpr uint16_t DEVICE_FLAG_TRUSTED = 0x0002;  // Reserved
constexpr uint16_t DEVICE_FLAG_SENSOR = 0x0004;   // Reserved
constexpr uint16_t DEVICE_FLAG_RELAY = 0x0008;    // Reserved

/**
 * @brief Device authentication manager
 */
class DeviceAuth {
public:
    DeviceAuth();
    ~DeviceAuth();
    
    /**
     * @brief Initialize device authentication
     * @param mode Initial authentication mode
     * @return true if initialized successfully
     */
    bool begin(AuthMode mode = AuthMode::OPEN);
    
    /**
     * @brief Set authentication mode
     * @param mode New mode
     */
    void setMode(AuthMode mode);
    
    /**
     * @brief Get current authentication mode
     */
    AuthMode getMode() const { return mode_; }
    
    /**
     * @brief Check if a device is authorized
     * @param address Device mesh address
     * @param macHash Optional MAC hash for binding verification (4 bytes)
     * @return true if device is authorized
     */
    bool isAuthorized(uint16_t address, const uint8_t* macHash = nullptr);
    
    /**
     * @brief Add device to allowlist
     * @param address Device mesh address
     * @param macHash Optional MAC hash for binding (4 bytes)
     * @param flags Device flags
     * @return true if added successfully
     */
    bool addDevice(uint16_t address, const uint8_t* macHash = nullptr, uint16_t flags = 0);
    
    /**
     * @brief Remove device from allowlist
     * @param address Device mesh address
     * @return true if removed
     */
    bool removeDevice(uint16_t address);
    
    /**
     * @brief Update device last seen time
     * @param address Device mesh address
     */
    void updateLastSeen(uint16_t address);
    
    /**
     * @brief Get number of allowed devices
     */
    uint8_t getDeviceCount() const;
    
    /**
     * @brief Persist allowlist to NVS
     * @return true if persisted successfully
     */
    bool persist();
    
#if defined(UNIT_TEST) || defined(NATIVE_BUILD)
    uint32_t getAuthSuccessCount() const { return authSuccessCount_; }
    uint32_t getAuthFailCount() const { return authFailCount_; }
    static void computeMacHash(const uint8_t* mac, uint8_t* hashOut);
#endif

private:
    AuthMode mode_;
    DeviceEntry devices_[MAX_ALLOWED_DEVICES];
    uint32_t authSuccessCount_;
    uint32_t authFailCount_;
    bool initialized_;
    void* mutex_;
    
    /**
     * @brief Find device entry by address
     */
    DeviceEntry* findDevice(uint16_t address);
    
    /**
     * @brief Find free slot in device list
     */
    DeviceEntry* findFreeSlot();
    
    /**
     * @brief Load allowlist from NVS
     */
    bool loadFromNVS();
    
    /**
     * @brief Save allowlist to NVS
     */
    bool saveToNVS();
};

} // namespace security
} // namespace xmesh

#endif // XMESH_SECURITY_DEVICE_AUTH_H
