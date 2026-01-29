#pragma once
#include <cstdint>

namespace xmesh {

enum class MobilityState : uint8_t { STATIC, MOBILE, EMERGENCY };

class MobilityDetector {
public:
    MobilityDetector();
    
    void enable();
    void disable();
    bool isEnabled() const;
    
    void feedSNR(uint16_t addr, int8_t snr);
    
    void tick(bool trickleAtMax);
    
    MobilityState getState() const;
    const char* getStateName() const;
    
    void triggerEmergency();
    void simulateState(MobilityState state);
    
    float getAggregateVariance() const;

private:
    static constexpr uint8_t MAX_NEIGHBORS = 10;
    static constexpr uint8_t SNR_WINDOW_SIZE = 10;
    static constexpr float VARIANCE_THRESHOLD_HIGH = 3.0f;
    static constexpr float VARIANCE_THRESHOLD_LOW = 1.5f;
    static constexpr uint32_t STABLE_DURATION_MS = 120000;
    static constexpr uint32_t EMERGENCY_HOLD_MS = 60000;
    static constexpr uint32_t HYSTERESIS_MS = 60000;
    static constexpr uint8_t HIGH_VARIANCE_COUNT_THRESHOLD = 3;
    
    struct NeighborSNR {
        uint16_t address = 0;
        int8_t snrWindow[SNR_WINDOW_SIZE];
        uint8_t windowIndex = 0;
        uint8_t windowFilled = 0;
        uint32_t lastUpdate = 0;
    };
    
    bool enabled_ = false;
    MobilityState state_ = MobilityState::STATIC;
    NeighborSNR neighbors_[MAX_NEIGHBORS];
    uint8_t neighborCount_ = 0;
    
    uint32_t lastTransitionTime_ = 0;
    uint32_t stableStartTime_ = 0;
    uint32_t emergencyStartTime_ = 0;
    uint8_t highVarianceCount_ = 0;
    
    float calculateNeighborVariance(const NeighborSNR& neighbor) const;
    NeighborSNR* findOrCreateNeighbor(uint16_t addr);
    void evictOldestNeighbor();
};

}
