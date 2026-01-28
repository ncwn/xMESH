#include "xmesh/TrickleScheduler.h"
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "TRICKLE";

namespace xmesh {

TrickleScheduler::TrickleScheduler(uint32_t imin, uint32_t imax, uint8_t redundancy, bool enable)
    : I_min(imin), I_max(imax), I_current(imin), k(redundancy),
      intervalStart(0), nextTransmit(0), consistentHeard(0),
      enabled(enable), transmitCount(0), suppressCount(0),
      state(IDLE) {
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

void TrickleScheduler::start() {
    if (!enabled) {
        ESP_LOGW(TAG, "Trickle disabled - start() ignored");
        return;
    }
    state = ACTIVE;
    I_current = I_min;
    reset();
    Serial.printf("[Trickle] Started - I=%.1fs\n", I_current/1000.0);
}

void TrickleScheduler::reset() {
    if (!enabled) return;
    
    I_current = I_min;
    consistentHeard = 0;
    intervalStart = millis();
    
    uint32_t halfInterval = I_current / 2;
    nextTransmit = intervalStart + halfInterval + random(halfInterval);
    
    state = RESET;
    Serial.printf("[Trickle] RESET - I=%.1fs, next TX in %.1fs\n", 
                 I_current/1000.0, (nextTransmit - millis())/1000.0);
}

void TrickleScheduler::doubleInterval() {
    if (!enabled) return;
    
    I_current = min(I_current * 2, I_max);
    consistentHeard = 0;
    intervalStart = millis();
    
    uint32_t halfInterval = I_current / 2;
    nextTransmit = intervalStart + halfInterval + random(halfInterval);
    
    state = ACTIVE;
    Serial.printf("[Trickle] DOUBLE - I=%.1fs, next TX in %.1fs\n", 
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
        
        if (consistentHeard >= k) {
            suppressCount++;
            Serial.printf("[Trickle] SUPPRESS - heard %d consistent HELLOs\n", 
                         consistentHeard);
            return false;
        }
        
        transmitCount++;
        Serial.printf("[Trickle] TRANSMIT - count=%u, interval=%.1fs\n", 
                     transmitCount, I_current/1000.0);
        return true;
    }
    
    return false;
}

void TrickleScheduler::onHelloReceived() {
    if (!enabled) return;
    consistentHeard++;
}

void TrickleScheduler::onInconsistentHello() {
    if (!enabled) return;
    Serial.println("[Trickle] Inconsistent HELLO - resetting");
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

} // namespace xmesh
