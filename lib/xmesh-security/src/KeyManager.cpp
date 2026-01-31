#include "xmesh/security/KeyManager.h"

#ifdef NATIVE_BUILD
#include "MockFreeRTOS.h"
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_random.h>
#include <esp_log.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/md.h>
#endif

namespace xmesh {
namespace security {

static const char* KM_TAG = "KeyManager";
static const char* NVS_KEY_NAMESPACE = "xmesh_keys";

KeyManager::KeyManager() : initialized_(false), mutex_(nullptr) {}

KeyManager::~KeyManager() {
    clearAll();
#ifndef NATIVE_BUILD
    if (mutex_ != nullptr) {
        vSemaphoreDelete(static_cast<SemaphoreHandle_t>(mutex_));
    }
#endif
}

bool KeyManager::begin() {
#ifndef NATIVE_BUILD
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        ESP_LOGE(KM_TAG, "Failed to create mutex");
        return false;
    }
#endif
    
    loadFromNVS();
    initialized_ = true;
    return true;
}

bool KeyManager::setPrimaryKey(const uint8_t* key, uint8_t version) {
    if (key == nullptr || version == 0) return false;
    
#ifndef NATIVE_BUILD
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
#endif
    
    memcpy(primaryKey_.key, key, KEY_SIZE);
    primaryKey_.version = version;
#ifndef NATIVE_BUILD
    primaryKey_.activeSinceMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
#endif
    primaryKey_.valid = true;
    
#ifndef NATIVE_BUILD
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
    
    return true;
}

uint8_t KeyManager::getPrimaryKey(uint8_t* keyOut) const {
    if (!primaryKey_.valid || keyOut == nullptr) return 0;
    memcpy(keyOut, primaryKey_.key, KEY_SIZE);
    return primaryKey_.version;
}

bool KeyManager::getKeyByVersion(uint8_t version, uint8_t* keyOut) const {
    if (keyOut == nullptr || version == 0) return false;
    
    if (primaryKey_.valid && primaryKey_.version == version) {
        memcpy(keyOut, primaryKey_.key, KEY_SIZE);
        return true;
    }
    
    if (previousKey_.valid && previousKey_.version == version) {
        memcpy(keyOut, previousKey_.key, KEY_SIZE);
        return true;
    }
    
    return false;
}

#ifdef XMESH_ENABLE_KEY_ROTATION
bool KeyManager::rotateKey(const uint8_t* newKey, uint8_t newVersion) {
    if (newKey == nullptr || newVersion == 0) return false;
    
#ifndef NATIVE_BUILD
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
#endif
    
    if (primaryKey_.valid) {
        memcpy(&previousKey_, &primaryKey_, sizeof(KeySlot));
    }
    
    memcpy(primaryKey_.key, newKey, KEY_SIZE);
    primaryKey_.version = newVersion;
#ifndef NATIVE_BUILD
    primaryKey_.activeSinceMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
#endif
    primaryKey_.valid = true;
    
#ifndef NATIVE_BUILD
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
    
    return true;
}

uint8_t KeyManager::getPreviousVersion() const {
    return previousKey_.valid ? previousKey_.version : 0;
}

bool KeyManager::generateRandomKey(uint8_t* keyOut) {
    if (keyOut == nullptr) return false;
    
#ifndef NATIVE_BUILD
    esp_fill_random(keyOut, KEY_SIZE);
    return true;
#else
    for (size_t i = 0; i < KEY_SIZE; i++) {
        keyOut[i] = static_cast<uint8_t>(i * 7 + 0x42);
    }
    return true;
#endif
}
#endif

bool KeyManager::hasKeyVersion(uint8_t version) const {
    if (version == 0) return false;
    return (primaryKey_.valid && primaryKey_.version == version) ||
           (previousKey_.valid && previousKey_.version == version);
}

uint8_t KeyManager::getCurrentVersion() const {
    return primaryKey_.valid ? primaryKey_.version : 0;
}

bool KeyManager::deriveKeyFromPassword(const char* password, const uint8_t* salt, 
                                        size_t saltLen, uint8_t* keyOut) {
    if (password == nullptr || salt == nullptr || keyOut == nullptr) return false;
    
#ifndef NATIVE_BUILD
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    
    const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mdInfo == nullptr) {
        return false;
    }
    
    int ret = mbedtls_md_setup(&ctx, mdInfo, 1);
    if (ret != 0) {
        mbedtls_md_free(&ctx);
        return false;
    }
    
    ret = mbedtls_pkcs5_pbkdf2_hmac(&ctx, 
                                    reinterpret_cast<const unsigned char*>(password), 
                                    strlen(password),
                                    salt, saltLen,
                                    10000,
                                    KEY_SIZE, keyOut);
    
    mbedtls_md_free(&ctx);
    return (ret == 0);
#else
    (void)salt; (void)saltLen;
    for (size_t i = 0; i < KEY_SIZE; i++) {
        keyOut[i] = password[i % strlen(password)] ^ 0x55;
    }
    return true;
#endif
}

bool KeyManager::persist() {
    return saveToNVS();
}

void KeyManager::clearAll() {
#ifndef NATIVE_BUILD
    if (mutex_ != nullptr) {
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    }
#endif
    
    secureClear(primaryKey_.key, KEY_SIZE);
    primaryKey_.clear();
    secureClear(previousKey_.key, KEY_SIZE);
    previousKey_.clear();
    
#ifndef NATIVE_BUILD
    if (mutex_ != nullptr) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    }
#endif
}

bool KeyManager::hasValidKey() const {
    return primaryKey_.valid;
}

void KeyManager::cleanupOldKeys() {
#ifndef NATIVE_BUILD
    if (!previousKey_.valid) return;
    
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if ((now - primaryKey_.activeSinceMs) > KEY_ROTATION_GRACE_MS) {
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
        secureClear(previousKey_.key, KEY_SIZE);
        previousKey_.clear();
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    }
#endif
}

void KeyManager::secureClear(uint8_t* data, size_t len) {
    volatile uint8_t* p = data;
    while (len--) {
        *p++ = 0;
    }
}

bool KeyManager::loadFromNVS() {
#ifndef NATIVE_BUILD
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_KEY_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;
    
    size_t keyLen = KEY_SIZE;
    if (nvs_get_blob(handle, "primary_key", primaryKey_.key, &keyLen) == ESP_OK) {
        nvs_get_u8(handle, "primary_ver", &primaryKey_.version);
        primaryKey_.valid = (primaryKey_.version != 0);
    }
    
    nvs_close(handle);
    return primaryKey_.valid;
#else
    return false;
#endif
}

bool KeyManager::saveToNVS() {
#ifndef NATIVE_BUILD
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_KEY_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;
    
    if (primaryKey_.valid) {
        nvs_set_blob(handle, "primary_key", primaryKey_.key, KEY_SIZE);
        nvs_set_u8(handle, "primary_ver", primaryKey_.version);
    }
    
    nvs_commit(handle);
    nvs_close(handle);
    return true;
#else
    return true;
#endif
}

}
}
