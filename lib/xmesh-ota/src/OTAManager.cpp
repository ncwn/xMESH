/**
 * @file OTAManager.cpp
 * @brief OTA update manager implementation using ESP-IDF native OTA
 * 
 * Implements WiFi-based OTA updates using ArduinoOTA library with:
 * - Dual partition scheme (ota_0, ota_1)
 * - Automatic rollback on boot failure (NVS-tracked)
 * - ESP-IDF native partition management
 */

#include "ota/OTAManager.h"
#include <ArduinoOTA.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_app_format.h>
#include <esp_log.h>

namespace xmesh {
namespace ota {

static const char* TAG = "OTA";

// NVS namespace and key for boot failure tracking
static const char* NVS_NAMESPACE = "xmesh_ota";
static const char* NVS_FAIL_COUNT_KEY = "ota_fail_count";
static const uint8_t MAX_BOOT_FAILURES = 3;

bool OTAManager::begin() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        setError(OTAError::WRITE_FAILED);
        return false;
    }

    nvs_handle_t nvs_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        uint8_t fail_count = 0;
        err = nvs_get_u8(nvs_handle, NVS_FAIL_COUNT_KEY, &fail_count);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            fail_count = 0;
        }

        fail_count++;
        nvs_set_u8(nvs_handle, NVS_FAIL_COUNT_KEY, fail_count);
        nvs_commit(nvs_handle);

        ESP_LOGI(TAG, "Boot count: %d", fail_count);

        if (fail_count >= MAX_BOOT_FAILURES) {
            ESP_LOGE(TAG, "Max boot failures reached - triggering rollback");
            nvs_close(nvs_handle);
            setError(OTAError::ROLLBACK_DETECTED);
            esp_ota_mark_app_invalid_rollback_and_reboot();
            return false;
        }

        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    }

    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running == nullptr) {
        ESP_LOGE(TAG, "Failed to get running partition");
        setError(OTAError::NO_PARTITION);
        return false;
    }

    ESP_LOGI(TAG, "Running partition: %s (offset 0x%lx)", 
                  running->label, running->address);

    update_partition_ = esp_ota_get_next_update_partition(nullptr);
    if (update_partition_ == nullptr) {
        ESP_LOGE(TAG, "Failed to get update partition");
        setError(OTAError::NO_PARTITION);
        return false;
    }

    ESP_LOGI(TAG, "Update partition: %s (offset 0x%lx, size %lu bytes)",
                  update_partition_->label, update_partition_->address, update_partition_->size);

    ArduinoOTA.setHostname("xmesh-gateway");
    
    ArduinoOTA.onStart([this]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        ESP_LOGI(TAG, "Start updating %s", type.c_str());
        setState(OTAState::DOWNLOADING);
        progress_ = 0;

        esp_err_t err = esp_ota_begin(update_partition_, OTA_SIZE_UNKNOWN, &update_handle_);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
            setError(OTAError::WRITE_FAILED);
            abort();
        }
    });

    ArduinoOTA.onEnd([this]() {
        ESP_LOGI(TAG, "Update complete - finalizing...");
        setState(OTAState::APPLYING);
        
        esp_err_t err = esp_ota_end(update_handle_);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
            setError(OTAError::WRITE_FAILED);
            return;
        }

        err = esp_ota_set_boot_partition(update_partition_);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
            setError(OTAError::WRITE_FAILED);
            return;
        }

        ESP_LOGI(TAG, "Rebooting to apply update...");
        delay(1000);
        esp_restart();
    });

    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
        uint8_t percent = (progress * 100) / total;
        if (percent != progress_) {
            progress_ = percent;
            ESP_LOGI(TAG, "Progress: %u%%", progress_);
        }

        if (update_handle_ != 0) {
            setState(OTAState::DOWNLOADING);
        }
    });

    ArduinoOTA.onError([this](ota_error_t error) {
        switch (error) {
            case OTA_AUTH_ERROR:
                ESP_LOGE(TAG, "Error[%u]: Auth Failed", error);
                setError(OTAError::VERIFY_FAILED);
                break;
            case OTA_BEGIN_ERROR:
                ESP_LOGE(TAG, "Error[%u]: Begin Failed", error);
                setError(OTAError::WRITE_FAILED);
                break;
            case OTA_CONNECT_ERROR:
                ESP_LOGE(TAG, "Error[%u]: Connect Failed", error);
                setError(OTAError::DOWNLOAD_FAILED);
                break;
            case OTA_RECEIVE_ERROR:
                ESP_LOGE(TAG, "Error[%u]: Receive Failed", error);
                setError(OTAError::DOWNLOAD_FAILED);
                break;
            case OTA_END_ERROR:
                ESP_LOGE(TAG, "Error[%u]: End Failed", error);
                setError(OTAError::WRITE_FAILED);
                break;
            default:
                ESP_LOGE(TAG, "Error[%u]", error);
                break;
        }
        setState(OTAState::FAILED);
        abort();
    });

    ArduinoOTA.begin();
    ESP_LOGI(TAG, "ArduinoOTA service started");

    setState(OTAState::IDLE);
    return true;
}

bool OTAManager::checkForUpdates() {
    // TODO: Implement HTTP-based update check against version server
    // Currently using ArduinoOTA push model instead
    return false;
}

bool OTAManager::startUpdate(const char* url) {
    // TODO: Implement HTTP pull-based OTA using esp_http_client
    if (url != nullptr) {
        ESP_LOGW(TAG, "HTTP OTA not implemented - use ArduinoOTA");
        return false;
    }
    
    return true;
}

bool OTAManager::process() {
    ArduinoOTA.handle();
    
    return (state_ == OTAState::DOWNLOADING || 
            state_ == OTAState::VERIFYING || 
            state_ == OTAState::APPLYING);
}

bool OTAManager::getRollbackReason() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    esp_err_t err = esp_ota_get_state_partition(running, &ota_state);
    if (err == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "App is pending verification (first boot after update)");
            return true;
        } else if (ota_state == ESP_OTA_IMG_INVALID || ota_state == ESP_OTA_IMG_ABORTED) {
            ESP_LOGW(TAG, "App was marked invalid - rollback occurred");
            return true;
        }
    }
    
    return false;
}

void OTAManager::markAppValid() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    esp_err_t err = esp_ota_get_state_partition(running, &ota_state);
    if (err == ESP_OK && ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "Marking app as valid - canceling rollback");
        err = esp_ota_mark_app_valid_cancel_rollback();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mark app valid: %s", esp_err_to_name(err));
            return;
        }
    }

    nvs_handle_t nvs_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_u8(nvs_handle, NVS_FAIL_COUNT_KEY, 0);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Boot failure count reset to 0");
    } else {
        ESP_LOGE(TAG, "Failed to reset boot counter: %s", esp_err_to_name(err));
    }
}

void OTAManager::abort() {
    if (update_handle_ != 0) {
        esp_ota_abort(update_handle_);
        update_handle_ = 0;
    }
    setState(OTAState::IDLE);
    progress_ = 0;
}

bool OTAManager::verifyPartition() {
    // TODO: Integrate into startUpdate() when HTTP pull-based OTA is implemented
    // Currently unused as ArduinoOTA handles its own verification
    if (update_partition_ == nullptr) {
        setError(OTAError::NO_PARTITION);
        return false;
    }
    
    if (update_partition_->type != ESP_PARTITION_TYPE_APP) {
        setError(OTAError::NO_PARTITION);
        return false;
    }
    
    return true;
}

void OTAManager::setState(OTAState new_state) {
    state_ = new_state;
}

void OTAManager::setError(OTAError error) {
    last_error_ = error;
}

} // namespace ota
} // namespace xmesh
