#include "xmesh/GatewayBalancer.h"
#include <Arduino.h>
#include <algorithm>
#include <new>
#include <esp_log.h>
#include <esp_heap_caps.h>

static const char* TAG = "GATEWAY";
constexpr size_t HEAP_WARNING_THRESHOLD = 15360;

namespace xmesh {

GatewayBalancer::GatewayBalancer(uint8_t maxNeighbors)
    : isGatewayNode(false),
      maxNeighbors(maxNeighbors),
      numNeighbors(0),
      lastStatusLog(0) {
    neighbors = new (std::nothrow) NeighborHealth[maxNeighbors];
    if (!neighbors) {
        ESP_LOGE(TAG, "Failed to allocate neighbor health array (%d entries)", maxNeighbors);
        this->maxNeighbors = 0;
    }
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ESP_LOGE(TAG, "Failed to create mutex");
    }
}

GatewayBalancer::~GatewayBalancer() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
    delete[] neighbors;
}

uint8_t GatewayBalancer::encodeGatewayLoad(float packetsPerMinute) {
    if (packetsPerMinute < 0.0f) {
        ESP_LOGW(TAG, "Negative packets per minute: %.2f, clamping to 0", packetsPerMinute);
        packetsPerMinute = 0.0f;
    }
    if (packetsPerMinute > 254.0f) {
        ESP_LOGW(TAG, "Excessive load: %.2f packets/min, clamping to 254", packetsPerMinute);
    }
    float clamped = constrain(packetsPerMinute, 0.0f, 254.0f);
    return static_cast<uint8_t>(clamped + 0.5f);
}

float GatewayBalancer::decodeGatewayLoad(uint8_t encodedLoad) {
    if (encodedLoad == 255) {
        return 0.0f;
    }
    return static_cast<float>(encodedLoad);
}

void GatewayBalancer::recordGatewayLoadSample() {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex in recordGatewayLoadSample");
        return;
    }
    if (!isGatewayNode) {
        xSemaphoreGive(mutex_);
        return;
    }
    localLoadState.packetsSinceLastSample++;
    xSemaphoreGive(mutex_);
}

uint8_t GatewayBalancer::sampleLocalGatewayLoadForHello() {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex in sampleLocalGatewayLoadForHello");
        return 255;
    }
    if (!isGatewayNode) {
        xSemaphoreGive(mutex_);
        return 255;
    }

    uint32_t now = millis();
    if (localLoadState.lastSampleTimestamp == 0) {
        localLoadState.lastSampleTimestamp = now;
        localLoadState.lastEncodedLoad = 0;
        localLoadState.packetsSinceLastSample = 0;
        xSemaphoreGive(mutex_);
        return 0;
    }

    uint32_t elapsed = now - localLoadState.lastSampleTimestamp;
    if (elapsed < MIN_GATEWAY_LOAD_WINDOW_MS) {
        elapsed = MIN_GATEWAY_LOAD_WINDOW_MS;
    }

    float packetsPerMinute = 0.0f;
    if (elapsed > 0) {
        packetsPerMinute = (localLoadState.packetsSinceLastSample * 60000.0f) / elapsed;
    }

    uint8_t encoded = encodeGatewayLoad(packetsPerMinute);
    localLoadState.packetsSinceLastSample = 0;
    localLoadState.lastSampleTimestamp = now;
    localLoadState.lastEncodedLoad = encoded;
    xSemaphoreGive(mutex_);
    return encoded;
}

uint8_t GatewayBalancer::peekLocalGatewayLoad() const {
    return localLoadState.lastEncodedLoad;
}

float GatewayBalancer::getGatewayBias(uint16_t gatewayAddr, uint8_t encodedLoad) const {
    if (encodedLoad == 255) {
        return 0.0f;
    }
    
    float load = decodeGatewayLoad(encodedLoad);
    return load * 0.01f;
}

void GatewayBalancer::updateNeighborHealth(uint16_t addr) {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex in updateNeighborHealth");
        return;
    }
    uint32_t now = millis();

    int8_t idx = findNeighborIndex(addr);
    if (idx >= 0) {
        uint32_t silence = now - neighbors[idx].lastHeard;

        if (neighbors[idx].failureFlagged) {
            ESP_LOGI(TAG, "Neighbor %04X: RECOVERED after %lus offline",
                         addr, silence/1000);
        }

        neighbors[idx].lastHeard = now;
        neighbors[idx].missedHellos = 0;
        neighbors[idx].failureFlagged = false;

        ESP_LOGD(TAG, "Neighbor %04X: Heartbeat (silence: %lus, status: HEALTHY)",
                     addr, silence/1000);
        xSemaphoreGive(mutex_);
        return;
    }

    if (addNeighbor(addr)) {
        neighbors[numNeighbors - 1].lastHeard = now;
        ESP_LOGI(TAG, "NEW neighbor %04X detected (total neighbors: %d)",
                     addr, numNeighbors);
    } else {
        ESP_LOGW(TAG, "Cannot track neighbor %04X (max %d reached)", 
                     addr, maxNeighbors);
    }
    xSemaphoreGive(mutex_);
}

uint8_t GatewayBalancer::monitorNeighborHealth() {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex in monitorNeighborHealth");
        return 0;
    }
    uint32_t now = millis();
    uint8_t failedCount = 0;

    size_t freeHeap = esp_get_free_heap_size();
    if (freeHeap < HEAP_WARNING_THRESHOLD) {
        ESP_LOGW(TAG, "Low heap memory: %d bytes free (threshold: %d)", freeHeap, HEAP_WARNING_THRESHOLD);
    }

    if (now - lastStatusLog > STATUS_LOG_INTERVAL_MS) {
        lastStatusLog = now;
        ESP_LOGI(TAG, "==== Neighbor Health Status (Tracking: %d neighbors) ====", numNeighbors);
        ESP_LOGI(TAG, "Free heap: %d bytes", freeHeap);
        for (uint8_t i = 0; i < numNeighbors; i++) {
            if (neighbors[i].address == 0) continue;
            uint32_t silence = now - neighbors[i].lastHeard;
            ESP_LOGI(TAG, "  %04X: silence=%lus, missed=%d, status=%s",
                         neighbors[i].address, silence/1000,
                         neighbors[i].missedHellos,
                         neighbors[i].failureFlagged ? "FAILED" : "HEALTHY");
        }
    }

    for (uint8_t i = 0; i < numNeighbors; i++) {
        NeighborHealth* n = &neighbors[i];
        if (n->address == 0 || n->lastHeard == 0) continue;

        uint32_t silence = now - n->lastHeard;

        if (silence > warningThresholdMs_ && silence < detectionThresholdMs_ && n->missedHellos == 0) {
            n->missedHellos = 1;
            ESP_LOGW(TAG, "Neighbor %04X: WARNING - %lus silence (miss 1 HELLO)",
                         n->address, silence/1000);
            ESP_LOGD(TAG, "  Detection threshold: %lus remaining until FAULT",
                         (detectionThresholdMs_ - silence)/1000);
        }

        if (silence > detectionThresholdMs_ && !n->failureFlagged) {
            n->missedHellos = 2;
            n->failureFlagged = true;
            failedCount++;

            ESP_LOGE(TAG, "========================================");
            ESP_LOGE(TAG, "Neighbor %04X: FAILURE DETECTED", n->address);
            ESP_LOGE(TAG, "  Silence duration: %lus (%lu min %lu sec)",
                         silence/1000, silence/60000, (silence%60000)/1000);
            ESP_LOGE(TAG, "  Missed HELLOs: %d (expected every 180s)", n->missedHellos);
            ESP_LOGE(TAG, "========================================");

            ESP_LOGW(TAG, "Node failure detected - application should remove failed route");
            ESP_LOGW(TAG, "Failed neighbor: %04X", n->address);
            ESP_LOGW(TAG, "========================================");
        }
    }

    xSemaphoreGive(mutex_);
    return failedCount;
}

void GatewayBalancer::setWarningThreshold(uint32_t ms) {
    if (ms >= detectionThresholdMs_) {
        ESP_LOGE(TAG, "setWarningThreshold failed: %lu >= detection (%lu)", ms, detectionThresholdMs_);
        return;
    }
    ESP_LOGI(TAG, "Warning threshold: %lu -> %lu ms", warningThresholdMs_, ms);
    warningThresholdMs_ = ms;
}

void GatewayBalancer::setDetectionThreshold(uint32_t ms) {
    if (ms <= warningThresholdMs_) {
        ESP_LOGE(TAG, "setDetectionThreshold failed: %lu <= warning (%lu)", ms, warningThresholdMs_);
        return;
    }
    ESP_LOGI(TAG, "Detection threshold: %lu -> %lu ms", detectionThresholdMs_, ms);
    detectionThresholdMs_ = ms;
}

uint32_t GatewayBalancer::getWarningThreshold() const {
    return warningThresholdMs_;
}

uint32_t GatewayBalancer::getDetectionThreshold() const {
    return detectionThresholdMs_;
}

bool GatewayBalancer::isNeighborFailed(uint16_t addr) const {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex in isNeighborFailed");
        return false;
    }
    int8_t idx = findNeighborIndex(addr);
    if (idx >= 0) {
        bool failed = neighbors[idx].failureFlagged;
        xSemaphoreGive(mutex_);
        return failed;
    }
    xSemaphoreGive(mutex_);
    return false;
}

uint16_t GatewayBalancer::getNeighborAddress(uint8_t index) const {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex in getNeighborAddress");
        return 0;
    }
    uint16_t addr = 0;
    if (index < numNeighbors) {
        addr = neighbors[index].address;
    }
    xSemaphoreGive(mutex_);
    return addr;
}

bool GatewayBalancer::getNeighborStats(uint16_t addr, uint8_t& missedHellos, uint32_t& silenceDuration) const {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex in getNeighborStats");
        return false;
    }
    int8_t idx = findNeighborIndex(addr);
    if (idx >= 0) {
        missedHellos = neighbors[idx].missedHellos;
        silenceDuration = millis() - neighbors[idx].lastHeard;
        xSemaphoreGive(mutex_);
        return true;
    }
    xSemaphoreGive(mutex_);
    return false;
}

int8_t GatewayBalancer::findNeighborIndex(uint16_t addr) const {
    for (uint8_t i = 0; i < numNeighbors; i++) {
        if (neighbors[i].address == addr) {
            return i;
        }
    }
    return -1;
}

bool GatewayBalancer::addNeighbor(uint16_t addr) {
    if (numNeighbors >= maxNeighbors) {
        ESP_LOGE(TAG, "Cannot add neighbor %04X: max capacity (%d) reached", addr, maxNeighbors);
        return false;
    }

    neighbors[numNeighbors].address = addr;
    neighbors[numNeighbors].lastHeard = 0;
    neighbors[numNeighbors].missedHellos = 0;
    neighbors[numNeighbors].failureFlagged = false;
    numNeighbors++;
    return true;
}

} // namespace xmesh
