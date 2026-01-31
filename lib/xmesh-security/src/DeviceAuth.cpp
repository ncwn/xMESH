#include "xmesh/security/DeviceAuth.h"
#include <cstring>

#ifdef NATIVE_BUILD
#include "MockFreeRTOS.h"
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_log.h>
#include <mbedtls/sha256.h>
#endif

namespace xmesh {
namespace security {

static const char* DA_TAG = "DeviceAuth";
static const char* NVS_AUTH_NAMESPACE = "xmesh_auth";

DeviceAuth::DeviceAuth() 
    : mode_(AuthMode::OPEN), authSuccessCount_(0), authFailCount_(0), 
      initialized_(false), mutex_(nullptr) {
    for (uint8_t i = 0; i < MAX_ALLOWED_DEVICES; i++) {
        devices_[i] = DeviceEntry();
    }
}

DeviceAuth::~DeviceAuth() {
#ifndef NATIVE_BUILD
    if (mutex_ != nullptr) {
        vSemaphoreDelete(static_cast<SemaphoreHandle_t>(mutex_));
    }
#endif
}

bool DeviceAuth::begin(AuthMode mode) {
#ifndef NATIVE_BUILD
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        ESP_LOGE(DA_TAG, "Failed to create mutex");
        return false;
    }
#endif
    
    mode_ = mode;
    loadFromNVS();
    initialized_ = true;
    return true;
}

void DeviceAuth::setMode(AuthMode mode) {
    mode_ = mode;
}

bool DeviceAuth::isAuthorized(uint16_t address, const uint8_t* macHash) {
    if (!initialized_) return false;
    
    if (mode_ == AuthMode::OPEN) {
        authSuccessCount_++;
        return true;
    }
    
#ifndef NATIVE_BUILD
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
#endif
    
    DeviceEntry* device = findDevice(address);
    bool authorized = false;
    
    if (device != nullptr && device->valid) {
        if (macHash == nullptr) {
            authorized = true;
        } else {
            authorized = (memcmp(device->macHash, macHash, 4) == 0);
        }
    } else if (mode_ == AuthMode::LEARNING) {
        DeviceEntry* slot = findFreeSlot();
        if (slot != nullptr) {
            slot->address = address;
            slot->valid = true;
#ifndef NATIVE_BUILD
            slot->firstSeenMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
            slot->lastSeenMs = slot->firstSeenMs;
#endif
            if (macHash != nullptr) {
                memcpy(slot->macHash, macHash, 4);
            }
            authorized = true;
        }
    }
    
    if (authorized) {
        authSuccessCount_++;
        if (device != nullptr) {
#ifndef NATIVE_BUILD
            device->lastSeenMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
#endif
        }
    } else {
        authFailCount_++;
    }
    
#ifndef NATIVE_BUILD
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
    
    return authorized;
}

bool DeviceAuth::addDevice(uint16_t address, const uint8_t* macHash, uint16_t flags) {
#ifndef NATIVE_BUILD
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
#endif
    
    DeviceEntry* existing = findDevice(address);
    if (existing != nullptr) {
        existing->flags = flags;
        if (macHash != nullptr) {
            memcpy(existing->macHash, macHash, 4);
        }
#ifndef NATIVE_BUILD
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
        return true;
    }
    
    DeviceEntry* slot = findFreeSlot();
    if (slot == nullptr) {
#ifndef NATIVE_BUILD
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
        return false;
    }
    
    slot->address = address;
    slot->flags = flags;
    slot->valid = true;
#ifndef NATIVE_BUILD
    slot->firstSeenMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
    slot->lastSeenMs = slot->firstSeenMs;
#endif
    
    if (macHash != nullptr) {
        memcpy(slot->macHash, macHash, 4);
    }
    
#ifndef NATIVE_BUILD
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
    
    return true;
}

bool DeviceAuth::removeDevice(uint16_t address) {
#ifndef NATIVE_BUILD
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
#endif
    
    DeviceEntry* device = findDevice(address);
    bool removed = false;
    
    if (device != nullptr) {
        *device = DeviceEntry();
        removed = true;
    }
    
#ifndef NATIVE_BUILD
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
    
    return removed;
}

void DeviceAuth::updateLastSeen(uint16_t address) {
#ifndef NATIVE_BUILD
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    
    DeviceEntry* device = findDevice(address);
    if (device != nullptr) {
        device->lastSeenMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#else
    (void)address;
#endif
}

uint8_t DeviceAuth::getDeviceCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_ALLOWED_DEVICES; i++) {
        if (devices_[i].valid) count++;
    }
    return count;
}

bool DeviceAuth::persist() {
    return saveToNVS();
}

#if defined(UNIT_TEST) || defined(NATIVE_BUILD)
void DeviceAuth::computeMacHash(const uint8_t* mac, uint8_t* hashOut) {
#ifndef NATIVE_BUILD
    uint8_t fullHash[32];
    mbedtls_sha256(mac, 6, fullHash, 0);
    memcpy(hashOut, fullHash, 4);
#else
    hashOut[0] = mac[0] ^ mac[3];
    hashOut[1] = mac[1] ^ mac[4];
    hashOut[2] = mac[2] ^ mac[5];
    hashOut[3] = mac[0] ^ mac[1] ^ mac[2];
#endif
}
#endif

DeviceEntry* DeviceAuth::findDevice(uint16_t address) {
    for (uint8_t i = 0; i < MAX_ALLOWED_DEVICES; i++) {
        if (devices_[i].address == address && devices_[i].valid) {
            return &devices_[i];
        }
    }
    return nullptr;
}

DeviceEntry* DeviceAuth::findFreeSlot() {
    for (uint8_t i = 0; i < MAX_ALLOWED_DEVICES; i++) {
        if (!devices_[i].valid) {
            return &devices_[i];
        }
    }
    return nullptr;
}

bool DeviceAuth::loadFromNVS() {
#ifndef NATIVE_BUILD
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_AUTH_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;
    
    uint8_t modeVal = 0;
    if (nvs_get_u8(handle, "auth_mode", &modeVal) == ESP_OK) {
        mode_ = static_cast<AuthMode>(modeVal);
    }
    
    nvs_close(handle);
    return true;
#else
    return false;
#endif
}

bool DeviceAuth::saveToNVS() {
#ifndef NATIVE_BUILD
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_AUTH_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;
    
    nvs_set_u8(handle, "auth_mode", static_cast<uint8_t>(mode_));
    nvs_commit(handle);
    nvs_close(handle);
    
    return true;
#else
    return true;
#endif
}

}
}
