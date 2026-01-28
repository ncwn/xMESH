/**
 * @file OTAManager.h
 * @brief OTA update manager for xMESH using ESP-IDF native OTA
 * 
 * Wraps ESP-IDF OTA API for firmware updates via WiFi (ArduinoOTA).
 * Supports dual partition scheme with automatic rollback on boot failure.
 */

#pragma once

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

namespace xmesh {
namespace ota {

/**
 * @brief OTA update state machine states
 */
enum class OTAState {
    IDLE,           ///< No OTA operation in progress
    DOWNLOADING,    ///< Downloading firmware image
    VERIFYING,      ///< Verifying downloaded firmware
    APPLYING,       ///< Writing firmware to OTA partition
    FAILED          ///< OTA operation failed
};

/**
 * @brief OTA error codes
 */
enum class OTAError {
    NONE,
    NO_PARTITION,
    DOWNLOAD_FAILED,
    VERIFY_FAILED,
    WRITE_FAILED,
    ROLLBACK_DETECTED
};

/**
 * @brief OTA Manager class
 * 
 * Manages firmware update lifecycle:
 * 1. Initialize OTA partition
 * 2. Download firmware (via WiFi/ArduinoOTA)
 * 3. Verify image integrity
 * 4. Write to OTA partition
 * 5. Reboot to apply (with automatic rollback)
 */
class OTAManager {
public:
    /**
     * @brief Initialize OTA manager
     * @return true if initialization successful
     */
    bool begin();

    /**
     * @brief Check for available firmware updates
     * @return true if update is available
     */
    bool checkForUpdates();

    /**
     * @brief Start OTA update process
     * @param url Firmware URL (for HTTP OTA) or empty for ArduinoOTA
     * @return true if OTA started successfully
     */
    bool startUpdate(const char* url = nullptr);

    /**
     * @brief Process OTA update (call in loop)
     * @return true if update is in progress
     */
    bool process();

    /**
     * @brief Get current OTA state
     * @return Current state
     */
    OTAState getState() const { return state_; }

    /**
     * @brief Get last error
     * @return Last error code
     */
    OTAError getLastError() const { return last_error_; }

    /**
     * @brief Check if app rolled back after failed update
     * @return true if rollback occurred
     */
    bool getRollbackReason();

    /**
     * @brief Mark current app as valid (prevents rollback)
     * Call after verifying app boots successfully
     */
    void markAppValid();

    /**
     * @brief Get update progress (0-100)
     * @return Progress percentage
     */
    uint8_t getProgress() const { return progress_; }

    /**
     * @brief Abort current update
     */
    void abort();

private:
    OTAState state_ = OTAState::IDLE;
    OTAError last_error_ = OTAError::NONE;
    uint8_t progress_ = 0;

    const esp_partition_t* update_partition_ = nullptr;
    esp_ota_handle_t update_handle_ = 0;

    bool verifyPartition();
    void setState(OTAState new_state);
    void setError(OTAError error);
};

} // namespace ota
} // namespace xmesh
