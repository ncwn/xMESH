#include "xmesh/TrickleScheduler.h"
#include <algorithm>
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "TRICKLE";

namespace xmesh {

TrickleScheduler::TrickleScheduler(uint32_t imin, uint32_t imax, uint8_t redundancy, bool enable)
    : I_min(imin), I_max(imax), I_current(imin), k(redundancy),
      intervalStart(0), nextTransmit(0), consistentHeard(0),
      enabled(enable), transmitCount(0), suppressCount(0),
      state(IDLE) {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ESP_LOGE(TAG, "Failed to create mutex");
    }
    if (imin > imax) {
        ESP_LOGE(TAG, "Invalid interval config: I_min (%lu) > I_max (%lu)", imin, imax);
    }
    if (imin == 0 || imax == 0) {
        ESP_LOGE(TAG, "Invalid interval config: I_min or I_max is zero");
    }
    if (k == 0) {
        ESP_LOGW(TAG, "Redundancy k=0 - suppression disabled");
    }
}

TrickleScheduler::~TrickleScheduler() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

void TrickleScheduler::start() {
    if (!enabled) {
        ESP_LOGW(TAG, "Trickle disabled - start() ignored");
        return;
    }
    state = ACTIVE;
    I_current = I_min;
    reset();
    ESP_LOGI(TAG, "Started - I=%.1fs", I_current/1000.0);
}

void TrickleScheduler::reset() {
    if (!enabled) return;
    
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) { return; }
    I_current = I_min;
    consistentHeard = 0;
    xSemaphoreGive(mutex_);

    intervalStart = millis();
    
    uint32_t halfInterval = I_current / 2;
    nextTransmit = intervalStart + halfInterval + random(halfInterval);
    
    state = RESET;
    ESP_LOGI(TAG, "RESET - I=%.1fs, next TX in %.1fs", 
                 I_current/1000.0, (nextTransmit - millis())/1000.0);
}

void TrickleScheduler::doubleInterval() {
    if (!enabled) return;
    
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) { return; }
    I_current = std::min(I_current * 2, I_max);
    consistentHeard = 0;
    xSemaphoreGive(mutex_);

    intervalStart = millis();
    
    uint32_t halfInterval = I_current / 2;
    nextTransmit = intervalStart + halfInterval + random(halfInterval);
    
    state = ACTIVE;
    ESP_LOGI(TAG, "DOUBLE - I=%.1fs, next TX in %.1fs", 
                 I_current/1000.0, (nextTransmit - millis())/1000.0);
}

bool TrickleScheduler::intervalExpired() const {
    if (!enabled) return true;
    return (millis() - intervalStart) >= I_current;
}

bool TrickleScheduler::shouldTransmit() {
    if (!enabled) return true;
    
    uint32_t now = millis();
    
    if (state == IDLE) {
        ESP_LOGW(TAG, "shouldTransmit() called in IDLE state");
        return false;
    }
    
    if (intervalExpired()) {
        doubleInterval();
        return false;
    }
    
    if (now >= nextTransmit && state != IDLE) {
        nextTransmit = UINT32_MAX;
        
        bool suppress = false;
        if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) { return false; }
        if (consistentHeard >= k) {
            suppress = true;
        }
        xSemaphoreGive(mutex_);

        if (suppress) {
            suppressCount++;
            ESP_LOGD(TAG, "SUPPRESS - heard %d consistent HELLOs", 
                         consistentHeard);
            return false;
        }
        
        transmitCount++;
        ESP_LOGI(TAG, "TRANSMIT - count=%u, interval=%.1fs", 
                     transmitCount, I_current/1000.0);
        return true;
    }
    
    return false;
}

void TrickleScheduler::onHelloReceived() {
    if (!enabled) return;
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) { return; }
    consistentHeard++;
    xSemaphoreGive(mutex_);
}

void TrickleScheduler::onInconsistentHello() {
    if (!enabled) return;
    ESP_LOGI(TAG, "Inconsistent HELLO - resetting");
    reset();
}

float TrickleScheduler::getCurrentIntervalSec() const {
    return I_current / 1000.0;
}

uint32_t TrickleScheduler::getTransmitCount() const {
    return transmitCount;
}

uint32_t TrickleScheduler::getSuppressCount() const {
    return suppressCount;
}

bool TrickleScheduler::isEnabled() const {
    return enabled;
}

void TrickleScheduler::setIMin(uint32_t ms) {
    if (ms >= I_max) {
        ESP_LOGE(TAG, "setIMin failed: %lu >= I_max (%lu)", ms, I_max);
        return;
    }
    ESP_LOGI(TAG, "I_min changed: %lu -> %lu ms", I_min, ms);
    I_min = ms;
    // Clamp current interval if needed
    if (I_current < I_min) {
        I_current = I_min;
    }
}

void TrickleScheduler::setIMax(uint32_t ms) {
    if (ms <= I_min) {
        ESP_LOGE(TAG, "setIMax failed: %lu <= I_min (%lu)", ms, I_min);
        return;
    }
    ESP_LOGI(TAG, "I_max changed: %lu -> %lu ms", I_max, ms);
    I_max = ms;
    // Clamp current interval if needed
    if (I_current > I_max) {
        I_current = I_max;
    }
}

bool TrickleScheduler::isAtMaxInterval() const {
    return I_current >= I_max;
}

} // namespace xmesh
