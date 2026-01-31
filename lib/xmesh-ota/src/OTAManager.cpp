#include "ota/OTAManager.h"
#include <ArduinoOTA.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_app_format.h>
#include <esp_log.h>
#include <esp_https_ota.h>
#include <cstring>

namespace xmesh {
namespace ota {

static const char* TAG = "OTA";
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
    if (strlen(versionCheckUrl_) == 0) {
        return false;
    }
    return checkForUpdates(versionCheckUrl_);
}

bool OTAManager::checkForUpdates(const char* versionUrl) {
    if (versionUrl == nullptr) return false;
    
    setState(OTAState::CHECKING);
    
    esp_http_client_config_t config = {};
    config.url = versionUrl;
    config.timeout_ms = 10000;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        setError(OTAError::HTTP_ERROR);
        setState(OTAState::IDLE);
        return false;
    }
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        setError(OTAError::HTTP_ERROR);
        setState(OTAState::IDLE);
        return false;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0 || content_length > 31) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        setState(OTAState::IDLE);
        return false;
    }
    
    char version_buf[32] = {0};
    int read_len = esp_http_client_read(client, version_buf, content_length);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    if (read_len <= 0) {
        setState(OTAState::IDLE);
        return false;
    }
    
    version_buf[read_len] = '\0';
    for (int i = 0; i < read_len; i++) {
        if (version_buf[i] == '\n' || version_buf[i] == '\r') {
            version_buf[i] = '\0';
            break;
        }
    }
    
    strncpy(availableVersion_, version_buf, sizeof(availableVersion_) - 1);
    
    const esp_app_desc_t* app_desc = esp_ota_get_app_description();
    updateAvailable_ = (strcmp(availableVersion_, app_desc->version) != 0);
    
    ESP_LOGI(TAG, "Current: %s, Available: %s, Update: %s", 
             app_desc->version, availableVersion_, 
             updateAvailable_ ? "YES" : "NO");
    
    setState(OTAState::IDLE);
    return updateAvailable_;
}

bool OTAManager::startHttpUpdate(const char* firmwareUrl, const char* caCert) {
    if (firmwareUrl == nullptr) {
        if (strlen(firmwareUrl_) == 0) {
            ESP_LOGE(TAG, "No firmware URL configured");
            return false;
        }
        firmwareUrl = firmwareUrl_;
    }
    
    return performHttpOta(firmwareUrl, caCert);
}

bool OTAManager::validateImageHeader(const esp_app_desc_t* newAppInfo) {
    if (newAppInfo == nullptr) return false;
    
    const esp_app_desc_t* running = esp_ota_get_app_description();
    
    ESP_LOGI(TAG, "Running version: %s", running->version);
    ESP_LOGI(TAG, "New version: %s", newAppInfo->version);
    
    if (memcmp(newAppInfo->version, running->version, sizeof(newAppInfo->version)) == 0) {
        ESP_LOGW(TAG, "Same version - skipping update");
        setError(OTAError::VERSION_REJECTED);
        return false;
    }
    
    return true;
}

bool OTAManager::performHttpOta(const char* url, const char* caCert) {
    ESP_LOGI(TAG, "Starting HTTP OTA from: %s", url);
    setState(OTAState::DOWNLOADING);
    progress_ = 0;
    
    esp_http_client_config_t http_config = {};
    http_config.url = url;
    http_config.timeout_ms = 30000;
    http_config.keep_alive_enable = true;
    if (caCert != nullptr) {
        http_config.cert_pem = caCert;
    }
    
    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &http_config;
    
    esp_https_ota_handle_t https_ota_handle = nullptr;
    
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(err));
        setError(OTAError::HTTP_ERROR);
        setState(OTAState::FAILED);
        return false;
    }
    
    esp_app_desc_t new_app_info;
    err = esp_https_ota_get_img_desc(https_ota_handle, &new_app_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
        esp_https_ota_abort(https_ota_handle);
        setError(OTAError::DOWNLOAD_FAILED);
        setState(OTAState::FAILED);
        return false;
    }
    
    if (!validateImageHeader(&new_app_info)) {
        esp_https_ota_abort(https_ota_handle);
        setState(OTAState::FAILED);
        return false;
    }
    
    int last_progress = -1;
    while (true) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        
        int image_read = esp_https_ota_get_image_len_read(https_ota_handle);
        int image_size = esp_https_ota_get_image_size(https_ota_handle);
        if (image_size > 0) {
            int current_progress = (image_read * 100) / image_size;
            if (current_progress != last_progress) {
                progress_ = current_progress;
                last_progress = current_progress;
                ESP_LOGI(TAG, "Progress: %d%%", current_progress);
            }
        }
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_perform failed: %s", esp_err_to_name(err));
        esp_https_ota_abort(https_ota_handle);
        setError(OTAError::DOWNLOAD_FAILED);
        setState(OTAState::FAILED);
        return false;
    }
    
    if (!esp_https_ota_is_complete_data_received(https_ota_handle)) {
        ESP_LOGE(TAG, "Complete data not received");
        esp_https_ota_abort(https_ota_handle);
        setError(OTAError::DOWNLOAD_FAILED);
        setState(OTAState::FAILED);
        return false;
    }
    
    setState(OTAState::VERIFYING);
    
    err = esp_https_ota_finish(https_ota_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image signature verification FAILED");
            setError(OTAError::SIGNATURE_FAILED);
        } else {
            ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(err));
            setError(OTAError::VERIFY_FAILED);
        }
        setState(OTAState::FAILED);
        return false;
    }
    
    ESP_LOGI(TAG, "OTA successful! Restarting...");
    setState(OTAState::APPLYING);
    progress_ = 100;
    
    delay(1000);
    esp_restart();
    
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

void OTAManager::setVersionCheckUrl(const char* url) {
    if (url != nullptr) {
        strncpy(versionCheckUrl_, url, sizeof(versionCheckUrl_) - 1);
        versionCheckUrl_[sizeof(versionCheckUrl_) - 1] = '\0';
    }
}

void OTAManager::setFirmwareUrl(const char* url) {
    if (url != nullptr) {
        strncpy(firmwareUrl_, url, sizeof(firmwareUrl_) - 1);
        firmwareUrl_[sizeof(firmwareUrl_) - 1] = '\0';
    }
}

const char* OTAManager::getCurrentVersion() const {
    const esp_app_desc_t* app_desc = esp_ota_get_app_description();
    return app_desc->version;
}

void OTAManager::setState(OTAState new_state) {
    state_ = new_state;
}

void OTAManager::setError(OTAError error) {
    last_error_ = error;
}

}
}
