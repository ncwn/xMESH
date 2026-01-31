#ifndef XMESH_GATEWAY_BALANCER_H
#define XMESH_GATEWAY_BALANCER_H

#include <cstdint>
#include <cfloat>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace xmesh {

/**
 * @brief Gateway load state for traffic tracking
 * 
 * Tracks packets processed by a gateway for load calculation.
 * Load is measured in packets per minute and encoded to 0-254 range
 * (255 = unknown/unavailable).
 */
struct GatewayLoadState {
    uint32_t packetsSinceLastSample;  ///< Packets processed since last sample
    uint32_t lastSampleTimestamp;     ///< Timestamp of last load sample (millis)
    uint8_t lastEncodedLoad;          ///< Last encoded load value (0-254 valid, 255 unknown)

    GatewayLoadState() 
        : packetsSinceLastSample(0), 
          lastSampleTimestamp(0), 
          lastEncodedLoad(255) {}
};

/**
 * @brief Neighbor health state for fast fault detection
 * 
 * Tracks health status of mesh neighbors via HELLO monitoring.
 * Detects failures faster than library timeout (180-360s vs 600s).
 */
struct NeighborHealth {
    uint16_t address;          ///< Neighbor address
    uint32_t lastHeard;        ///< Last HELLO received timestamp (millis)
    uint8_t missedHellos;      ///< Consecutive missed safety HELLOs
    bool failureFlagged;       ///< Proactive failure detected flag

    NeighborHealth()
        : address(0),
          lastHeard(0),
          missedHellos(0),
          failureFlagged(false) {}
};

/**
 * @brief Active gateway load balancing for multi-gateway deployments
 * 
 * Distributes traffic across multiple gateways by tracking load and biasing
 * route costs. Prevents single gateway bottlenecks in large-scale networks.
 * 
 * Key features:
 * - Packets-per-minute load metric
 * - Load-biased gateway selection
 * - Fast neighbor failure detection (360s vs 600s library timeout)
 * - Proactive route removal and recovery
 */
class GatewayBalancer {
public:
    /**
     * @brief Construct a new GatewayBalancer
     * @param maxNeighbors Maximum neighbors to track (default: 10)
     */
    GatewayBalancer(uint8_t maxNeighbors = 10);

    /**
     * @brief Destructor
     */
    ~GatewayBalancer();

    // ============================================================
    // Gateway Load Tracking
    // ============================================================

    /**
     * @brief Encode gateway load (packets/min) to 0-254 range
     * @param packetsPerMinute Measured packets per minute
     * @return Encoded load value (0-254 valid, 255 reserved for unknown)
     */
    static uint8_t encodeGatewayLoad(float packetsPerMinute);

    /**
     * @brief Decode gateway load indicator back to packets per minute
     * @param encodedLoad Encoded load value (0-254 valid, 255=unknown)
     * @return Packets per minute (0.0 if unknown)
     */
    static float decodeGatewayLoad(uint8_t encodedLoad);

    /**
     * @brief Record that local gateway processed one downstream packet
     * 
     * Call this when gateway forwards a packet to upstream (WiFi/Internet).
     * Used for local load calculation.
     */
    void recordGatewayLoadSample();

    /**
     * @brief Sample local gateway load for HELLO serialization
     * 
     * Calculates packets/min based on counter since last sample.
     * Resets counter after sampling.
     * 
     * @return Encoded load value for HELLO broadcast (255 if not gateway)
     */
    uint8_t sampleLocalGatewayLoadForHello();

    /**
     * @brief Peek last known local gateway load value without resetting counters
     * @return Last encoded load value (255 if never sampled)
     */
    uint8_t peekLocalGatewayLoad() const;

    /**
     * @brief Get gateway bias for cost calculation (W5 parameter)
     * 
     * Returns load-based bias penalty for a specific gateway.
     * Higher load = higher bias = higher cost.
     * 
     * @param gatewayAddr Gateway address to check
     * @param encodedLoad Gateway's reported load value
     * @return Bias penalty value (0.0 if no bias needed)
     */
    float getGatewayBias(uint16_t gatewayAddr, uint8_t encodedLoad) const;

    // ============================================================
    // Neighbor Health Monitoring
    // ============================================================

    /**
     * @brief Update neighbor health status on HELLO receipt
     * 
     * Records heartbeat from neighbor and clears failure flags.
     * Adds new neighbor if not already tracked.
     * 
     * @param addr Neighbor address that sent HELLO
     */
    void updateNeighborHealth(uint16_t addr);

    /**
     * @brief Monitor neighbor health for fast fault detection
     * 
     * Checks all tracked neighbors for missing safety HELLOs.
     * Detects failures in 360s (miss 2 HELLOs) vs library's 600s timeout.
     * 
     * Call this periodically (every 30-60s) from main loop.
     * 
     * @return Number of failed neighbors detected this check
     */
    uint8_t monitorNeighborHealth();

    /**
     * @brief Check if neighbor is flagged as failed
     * @param addr Neighbor address to check
     * @return true if neighbor has been flagged as failed
     */
    bool isNeighborFailed(uint16_t addr) const;

    /**
     * @brief Get current number of tracked neighbors
     * @return Number of neighbors being monitored
     */
    uint8_t getNeighborCount() const { return numNeighbors; }

    /**
     * @brief Get neighbor address by index
     * @param index Neighbor index (0 to getNeighborCount()-1)
     * @return Neighbor address, or 0 if index out of bounds
     */
    uint16_t getNeighborAddress(uint8_t index) const;

#if defined(UNIT_TEST) || defined(NATIVE_BUILD)
    /**
     * @brief Get neighbor health stats for debugging
     * @param addr Neighbor address to query
     * @param missedHellos Output: number of missed HELLOs
     * @param silenceDuration Output: milliseconds since last heard
     * @return true if neighbor found, false otherwise
     */
    bool getNeighborStats(uint16_t addr, uint8_t& missedHellos, uint32_t& silenceDuration) const;
#endif

    // ============================================================
    // Configuration
    // ============================================================

    /**
     * @brief Set whether this node is a gateway
     * @param isGateway true if this node is a gateway
     */
    void setIsGateway(bool isGateway) { isGatewayNode = isGateway; }

    // Runtime threshold adjustment for mobility
    void setWarningThreshold(uint32_t ms);
    void setDetectionThreshold(uint32_t ms);

private:
    // Local gateway load tracking
    GatewayLoadState localLoadState;
    bool isGatewayNode;

    // Neighbor health tracking
    NeighborHealth* neighbors;      ///< Dynamic array of neighbor states
    uint8_t maxNeighbors;           ///< Maximum neighbors to track
    uint8_t numNeighbors;           ///< Current number of tracked neighbors

    // Gateway balancing configuration (from research config.h)
    static constexpr uint32_t MIN_GATEWAY_LOAD_WINDOW_MS = 1000;   ///< Minimum sampling window

    // Neighbor health monitoring thresholds
    uint32_t detectionThresholdMs_ = 360000;
    uint32_t warningThresholdMs_ = 180000;
    static constexpr uint32_t STATUS_LOG_INTERVAL_MS = 300000;     ///< 5 min periodic status

    // Internal state for periodic logging
    uint32_t lastStatusLog;
    SemaphoreHandle_t mutex_ = nullptr;

    /**
     * @brief Find neighbor index by address
     * @param addr Address to search for
     * @return Index if found, -1 otherwise
     */
    int8_t findNeighborIndex(uint16_t addr) const;

    /**
     * @brief Add new neighbor to tracking list
     * @param addr Neighbor address
     * @return true if added, false if list is full
     */
    bool addNeighbor(uint16_t addr);
};

} // namespace xmesh

#endif // XMESH_GATEWAY_BALANCER_H
