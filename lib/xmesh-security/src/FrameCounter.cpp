#include "xmesh/security/FrameCounter.h"

#ifdef NATIVE_BUILD
#include "MockFreeRTOS.h"
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_log.h>
#endif

namespace xmesh {
namespace security {

static const char* FC_TAG = "FrameCounter";

FrameCounter::FrameCounter() 
    : outgoingCounter_(0), replayRejectCount_(0), initialized_(false), mutex_(nullptr) {
    for (uint8_t i = 0; i < MAX_TRACKED_PEERS; i++) {
        peers_[i] = PeerCounter();
    }
}

FrameCounter::~FrameCounter() {
#ifndef NATIVE_BUILD
    if (mutex_ != nullptr) {
        vSemaphoreDelete(static_cast<SemaphoreHandle_t>(mutex_));
    }
#endif
}

bool FrameCounter::begin() {
#ifndef NATIVE_BUILD
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        ESP_LOGE(FC_TAG, "Failed to create mutex");
        return false;
    }
#endif
    
    if (!loadFromNVS()) {
        outgoingCounter_ = 1;
    }
    
    initialized_ = true;
    return true;
}

uint32_t FrameCounter::getNextOutgoing() {
    if (!initialized_) return 0;
    
#ifndef NATIVE_BUILD
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
#endif
    
    uint32_t counter = outgoingCounter_++;
    
#ifndef NATIVE_BUILD
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
    
    return counter;
}

bool FrameCounter::validateIncoming(uint16_t srcAddress, uint32_t counter) {
    if (!initialized_) return false;
    
#ifndef NATIVE_BUILD
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
#endif
    
    PeerCounter* peer = findOrCreatePeer(srcAddress);
    if (peer == nullptr) {
#ifndef NATIVE_BUILD
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
        return false;
    }
    
    bool valid = false;
    
    if (peer->lastCounter == 0) {
        peer->lastCounter = counter;
        peer->windowBitmap = 0;
        valid = true;
    } else if (counter > peer->lastCounter) {
        uint32_t diff = counter - peer->lastCounter;
        if (diff < 32) {
            peer->windowBitmap = (peer->windowBitmap << diff) | 1;
        } else {
            peer->windowBitmap = 1;
        }
        peer->lastCounter = counter;
        valid = true;
    } else if (counter == peer->lastCounter) {
        replayRejectCount_++;
        valid = false;
    } else {
        uint32_t diff = peer->lastCounter - counter;
        if (diff < COUNTER_WINDOW_SIZE && diff < 32) {
            uint32_t bit = 1U << diff;
            if (!(peer->windowBitmap & bit)) {
                peer->windowBitmap |= bit;
                valid = true;
            } else {
                replayRejectCount_++;
                valid = false;
            }
        } else {
            replayRejectCount_++;
            valid = false;
        }
    }
    
#ifndef NATIVE_BUILD
    if (valid) {
        peer->lastSeenMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
#endif
    
    return valid;
}

bool FrameCounter::persist() {
    return saveToNVS();
}

#if defined(UNIT_TEST) || defined(NATIVE_BUILD)
uint8_t FrameCounter::getTrackedPeerCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_TRACKED_PEERS; i++) {
        if (peers_[i].address != 0) count++;
    }
    return count;
}
#endif

uint8_t FrameCounter::cleanupStalePeers(uint32_t timeoutMs) {
#ifndef NATIVE_BUILD
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint8_t cleaned = 0;
    
    for (uint8_t i = 0; i < MAX_TRACKED_PEERS; i++) {
        if (peers_[i].address != 0 && (now - peers_[i].lastSeenMs) > timeoutMs) {
            peers_[i] = PeerCounter();
            cleaned++;
        }
    }
    
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    return cleaned;
#else
    (void)timeoutMs;
    return 0;
#endif
}

PeerCounter* FrameCounter::findOrCreatePeer(uint16_t address) {
    PeerCounter* freeSlot = nullptr;
    
    for (uint8_t i = 0; i < MAX_TRACKED_PEERS; i++) {
        if (peers_[i].address == address) {
            return &peers_[i];
        }
        if (peers_[i].address == 0 && freeSlot == nullptr) {
            freeSlot = &peers_[i];
        }
    }
    
    if (freeSlot != nullptr) {
        freeSlot->address = address;
        freeSlot->lastCounter = 0;
        freeSlot->windowBitmap = 0;
        freeSlot->lastSeenMs = 0;
        return freeSlot;
    }
    
    return nullptr;
}

bool FrameCounter::loadFromNVS() {
#ifndef NATIVE_BUILD
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_SECURITY_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;
    
    err = nvs_get_u32(handle, "fc_out", &outgoingCounter_);
    nvs_close(handle);
    
    return (err == ESP_OK);
#else
    return false;
#endif
}

bool FrameCounter::saveToNVS() {
#ifndef NATIVE_BUILD
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_SECURITY_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;
    
    err = nvs_set_u32(handle, "fc_out", outgoingCounter_);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    
    return (err == ESP_OK);
#else
    return true;
#endif
}

}
}
