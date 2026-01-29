#include "DutyCycleBudget.h"
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "DUTY";

void DutyCycleBudget::recordAirtime(uint32_t durationMs) {
    entries_[head_].timestamp = millis();
    entries_[head_].durationMs = static_cast<uint16_t>(durationMs);
    
    head_ = (head_ + 1) % MAX_ENTRIES;
    if (count_ < MAX_ENTRIES) {
        count_++;
    }
    
    lastCacheUpdate_ = 0;
    
    ESP_LOGD(TAG, "Recorded airtime: %lums at %lu", durationMs, entries_[(head_ + MAX_ENTRIES - 1) % MAX_ENTRIES].timestamp);
}

uint32_t DutyCycleBudget::calculateCurrentUsage() const {
    uint32_t currentTime = millis();
    uint32_t totalUsage = 0;
    
    for (uint8_t i = 0; i < count_; i++) {
        uint32_t timestamp = entries_[i].timestamp;
        uint32_t age;
        
        if (timestamp > currentTime) {
            age = (0xFFFFFFFF - timestamp) + currentTime + 1;
        } else {
            age = currentTime - timestamp;
        }
        
        if (age < WINDOW_MS) {
            totalUsage += entries_[i].durationMs;
        }
    }
    
    return totalUsage;
}

void DutyCycleBudget::tick() {
    cachedTotalMs_ = calculateCurrentUsage();
    lastCacheUpdate_ = millis();
    
    float usagePercent = getUsagePercent();
    
    if (usagePercent >= 100.0f) {
        ESP_LOGE(TAG, "Duty cycle EXHAUSTED: %.1f%% (%lums / %lums)", 
                 usagePercent, cachedTotalMs_, BUDGET_MS);
    } else if (usagePercent >= 80.0f) {
        ESP_LOGW(TAG, "Duty cycle WARNING: %.1f%% (%lums / %lums)", 
                 usagePercent, cachedTotalMs_, BUDGET_MS);
    }
}

uint32_t DutyCycleBudget::getRemainingBudgetMs() const {
    uint32_t currentUsage = cachedTotalMs_;
    
    if (millis() - lastCacheUpdate_ > 60000) {
        currentUsage = calculateCurrentUsage();
    }
    
    if (currentUsage >= BUDGET_MS) {
        return 0;
    }
    
    return BUDGET_MS - currentUsage;
}

float DutyCycleBudget::getUsagePercent() const {
    return (cachedTotalMs_ * 100.0f) / BUDGET_MS;
}

bool DutyCycleBudget::isExhausted() const {
    return cachedTotalMs_ >= BUDGET_MS;
}
