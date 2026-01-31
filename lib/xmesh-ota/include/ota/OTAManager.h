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
#include <esp_http_client.h>
#include <esp_app_format.h>

namespace xmesh {
namespace ota {

enum class OTAState {
    IDLE,
    CHECKING,
    DOWNLOADING,
    VERIFYING,
    APPLYING,
    FAILED
};

enum class OTAError {
    NONE,
    NO_PARTITION,
    DOWNLOAD_FAILED,
    VERIFY_FAILED,
    SIGNATURE_FAILED,
    VERSION_REJECTED,
    WRITE_FAILED,
    ROLLBACK_DETECTED,
    HTTP_ERROR
};

class OTAManager {
public:
    bool begin();
    bool checkForUpdates();
    bool checkForUpdates(const char* versionUrl);
    bool startHttpUpdate(const char* firmwareUrl, const char* caCert = nullptr);
    bool process();
    
    OTAState getState() const { return state_; }
    OTAError getLastError() const { return last_error_; }
    bool getRollbackReason();
    void markAppValid();
    uint8_t getProgress() const { return progress_; }
    void abort();
    
    void setVersionCheckUrl(const char* url);
    void setFirmwareUrl(const char* url);
    const char* getCurrentVersion() const;
    const char* getAvailableVersion() const { return availableVersion_; }
    bool isUpdateAvailable() const { return updateAvailable_; }

private:
    OTAState state_ = OTAState::IDLE;
    OTAError last_error_ = OTAError::NONE;
    uint8_t progress_ = 0;
    bool updateAvailable_ = false;

    const esp_partition_t* update_partition_ = nullptr;
    esp_ota_handle_t update_handle_ = 0;
    
    char versionCheckUrl_[128] = {0};
    char firmwareUrl_[256] = {0};
    char availableVersion_[32] = {0};
    
    bool validateImageHeader(const esp_app_desc_t* newAppInfo);
    bool performHttpOta(const char* url, const char* caCert);
    void setState(OTAState new_state);
    void setError(OTAError error);
};

} // namespace ota
} // namespace xmesh
