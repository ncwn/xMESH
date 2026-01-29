#pragma once
#include <cstdint>

class DutyCycleBudget {
public:
    DutyCycleBudget() = default;
    
    // Record airtime after transmission
    void recordAirtime(uint32_t durationMs);
    
    // Get remaining budget in current hour window
    uint32_t getRemainingBudgetMs() const;
    
    // Get usage as percentage (0-100+)
    float getUsagePercent() const;
    
    // Check if budget is exhausted
    bool isExhausted() const;
    
    // Periodic update - expires old entries (call every 60s)
    void tick();

private:
    static constexpr uint8_t MAX_ENTRIES = 100;
    static constexpr uint32_t WINDOW_MS = 3600000;  // 1 hour
    static constexpr uint32_t BUDGET_MS = 36000;    // 36s = 1% of 1 hour
    
    struct AirtimeEntry {
        uint32_t timestamp;   // millis() when TX occurred
        uint16_t durationMs;  // Airtime in milliseconds
    };
    
    AirtimeEntry entries_[MAX_ENTRIES];
    uint8_t head_ = 0;
    uint8_t count_ = 0;
    
    // Cached total for efficiency
    uint32_t cachedTotalMs_ = 0;
    uint32_t lastCacheUpdate_ = 0;
    
    // Recalculate total from entries within time window
    uint32_t calculateCurrentUsage() const;
};
