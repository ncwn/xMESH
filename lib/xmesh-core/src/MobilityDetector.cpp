#include "xmesh/MobilityDetector.h"
#include <Arduino.h>
#include <esp_log.h>
#include <cmath>

namespace xmesh {

static const char* TAG = "MOBILITY";

MobilityDetector::MobilityDetector() {}

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
    if (!enabled_) return;

    NeighborSNR* neighbor = findOrCreateNeighbor(addr);
    if (!neighbor) return;

    neighbor->snrWindow[neighbor->windowIndex] = snr;
    neighbor->windowIndex = (neighbor->windowIndex + 1) % SNR_WINDOW_SIZE;
    if (neighbor->windowFilled < SNR_WINDOW_SIZE) {
        neighbor->windowFilled++;
    }
    neighbor->lastUpdate = millis();
}

void MobilityDetector::tick(bool trickleAtMax) {
    if (!enabled_) return;

    uint32_t now = millis();
    if (now - lastTransitionTime_ < HYSTERESIS_MS) return;

    float variance = getAggregateVariance();

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
}

MobilityState MobilityDetector::getState() const {
    return state_;
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
    if (state_ != MobilityState::EMERGENCY) {
        ESP_LOGI(TAG, "Emergency triggered");
        state_ = MobilityState::EMERGENCY;
        emergencyStartTime_ = millis();
        lastTransitionTime_ = millis();
    }
}

void MobilityDetector::simulateState(MobilityState state) {
    state_ = state;
    lastTransitionTime_ = millis();
}

float MobilityDetector::getAggregateVariance() const {
    float totalVariance = 0;
    uint8_t count = 0;

    for (uint8_t i = 0; i < neighborCount_; i++) {
        float v = calculateNeighborVariance(neighbors_[i]);
        if (neighbors_[i].windowFilled >= 5) {
            totalVariance += v;
            count++;
        }
    }

    return (count > 0) ? (totalVariance / count) : 0.0f;
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
