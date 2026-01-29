#include "xmesh/MobilityDetector.h"
#include <Arduino.h>
#include <esp_log.h>
#include <cmath>

namespace xmesh {

static const char* TAG = "MOBILITY";

MobilityDetector::MobilityDetector() {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ESP_LOGE(TAG, "Failed to create mutex");
    }
}

MobilityDetector::~MobilityDetector() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

void MobilityDetector::enable() {
    enabled_ = true;
}

void MobilityDetector::disable() {
    enabled_ = false;
}

bool MobilityDetector::isEnabled() const {
    return enabled_;
}

void MobilityDetector::feedSNR(uint16_t addr, int8_t snr) {
    if (!enabled_) {
        ESP_LOGD(TAG, "feedSNR ignored - detector disabled");
        return;
    }

    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;

    NeighborSNR* neighbor = findOrCreateNeighbor(addr);
    if (!neighbor) {
        ESP_LOGW(TAG, "feedSNR: failed to track neighbor %04X", addr);
        xSemaphoreGive(mutex_);
        return;
    }

    neighbor->snrWindow[neighbor->windowIndex] = snr;
    neighbor->windowIndex = (neighbor->windowIndex + 1) % SNR_WINDOW_SIZE;
    if (neighbor->windowFilled < SNR_WINDOW_SIZE) {
        neighbor->windowFilled++;
    }
    neighbor->lastUpdate = millis();
    xSemaphoreGive(mutex_);
}

void MobilityDetector::tick(bool trickleAtMax) {
    if (!enabled_) {
        ESP_LOGD(TAG, "tick ignored - detector disabled");
        return;
    }

    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;

    uint32_t now = millis();
    if (now - lastTransitionTime_ < HYSTERESIS_MS) {
        xSemaphoreGive(mutex_);
        return;
    }

    float totalVariance = 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < neighborCount_; i++) {
        float v = calculateNeighborVariance(neighbors_[i]);
        if (neighbors_[i].windowFilled >= 5) {
            totalVariance += v;
            count++;
        }
    }
    float variance = (count > 0) ? (totalVariance / count) : 0.0f;

    switch (state_) {
        case MobilityState::STATIC:
            if (variance > VARIANCE_THRESHOLD_HIGH) {
                highVarianceCount_++;
                if (highVarianceCount_ >= HIGH_VARIANCE_COUNT_THRESHOLD) {
                    ESP_LOGI(TAG, "State STATIC -> MOBILE (variance: %.2f)", variance);
                    state_ = MobilityState::MOBILE;
                    lastTransitionTime_ = now;
                    stableStartTime_ = 0;
                    highVarianceCount_ = 0;
                }
            } else {
                highVarianceCount_ = 0;
            }
            break;

        case MobilityState::MOBILE:
            if (variance < VARIANCE_THRESHOLD_LOW && trickleAtMax) {
                if (stableStartTime_ == 0) {
                    stableStartTime_ = now;
                } else if (now - stableStartTime_ >= STABLE_DURATION_MS) {
                    ESP_LOGI(TAG, "State MOBILE -> STATIC (variance: %.2f)", variance);
                    state_ = MobilityState::STATIC;
                    lastTransitionTime_ = now;
                    stableStartTime_ = 0;
                }
            } else {
                stableStartTime_ = 0;
            }
            break;

        case MobilityState::EMERGENCY:
            if (now - emergencyStartTime_ >= EMERGENCY_HOLD_MS) {
                ESP_LOGI(TAG, "State EMERGENCY -> MOBILE");
                state_ = MobilityState::MOBILE;
                lastTransitionTime_ = now;
            }
            break;
    }
    xSemaphoreGive(mutex_);
}

MobilityState MobilityDetector::getState() const {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return state_;
    MobilityState s = state_;
    xSemaphoreGive(mutex_);
    return s;
}

const char* MobilityDetector::getStateName() const {
    switch (state_) {
        case MobilityState::STATIC: return "STATIC";
        case MobilityState::MOBILE: return "MOBILE";
        case MobilityState::EMERGENCY: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}

void MobilityDetector::triggerEmergency() {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;
    if (state_ != MobilityState::EMERGENCY) {
        ESP_LOGI(TAG, "Emergency triggered");
        state_ = MobilityState::EMERGENCY;
        emergencyStartTime_ = millis();
        lastTransitionTime_ = millis();
    }
    xSemaphoreGive(mutex_);
}

void MobilityDetector::simulateState(MobilityState state) {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;
    state_ = state;
    lastTransitionTime_ = millis();
    xSemaphoreGive(mutex_);
}

float MobilityDetector::getAggregateVariance() const {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return 0.0f;
    float totalVariance = 0;
    uint8_t count = 0;

    for (uint8_t i = 0; i < neighborCount_; i++) {
        float v = calculateNeighborVariance(neighbors_[i]);
        if (neighbors_[i].windowFilled >= 5) {
            totalVariance += v;
            count++;
        }
    }

    float result = (count > 0) ? (totalVariance / count) : 0.0f;
    xSemaphoreGive(mutex_);
    return result;
}

float MobilityDetector::calculateNeighborVariance(const NeighborSNR& neighbor) const {
    if (neighbor.windowFilled < 5) return 0.0f;

    float sum = 0;
    for (uint8_t i = 0; i < neighbor.windowFilled; i++) {
        sum += neighbor.snrWindow[i];
    }
    float mean = sum / neighbor.windowFilled;

    float sqSum = 0;
    for (uint8_t i = 0; i < neighbor.windowFilled; i++) {
        float diff = neighbor.snrWindow[i] - mean;
        sqSum += diff * diff;
    }

    return sqSum / neighbor.windowFilled;
}

MobilityDetector::NeighborSNR* MobilityDetector::findOrCreateNeighbor(uint16_t addr) {
    for (uint8_t i = 0; i < neighborCount_; i++) {
        if (neighbors_[i].address == addr) {
            return &neighbors_[i];
        }
    }

    if (neighborCount_ < MAX_NEIGHBORS) {
        neighbors_[neighborCount_].address = addr;
        neighbors_[neighborCount_].windowIndex = 0;
        neighbors_[neighborCount_].windowFilled = 0;
        return &neighbors_[neighborCount_++];
    }

    uint32_t oldestTime = UINT32_MAX;
    uint8_t oldestIdx = 0;
    for (uint8_t i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbors_[i].lastUpdate < oldestTime) {
            oldestTime = neighbors_[i].lastUpdate;
            oldestIdx = i;
        }
    }

    neighbors_[oldestIdx].address = addr;
    neighbors_[oldestIdx].windowIndex = 0;
    neighbors_[oldestIdx].windowFilled = 0;
    neighbors_[oldestIdx].lastUpdate = millis();
    return &neighbors_[oldestIdx];
}

}
