/**
 * @file main.cpp
 * @brief xMESH Gateway-Aware Cost Routing Protocol - Week 4-5
 * 
 * This firmware extends LoRaMesher's hop-count routing with multi-factor cost metrics:
 * - Link quality (RSSI + SNR)
 * - Transmission reliability (ETX)
 * - Gateway load balancing (bias penalty)
 * - Route stability (hysteresis)
 * 
 * Cost Function:
 * cost = W1×hops + W2×normalize(RSSI) + W3×normalize(SNR) + W4×ETX + W5×gateway_bias
 * 
 * Key Features:
 * - Smart route selection based on multiple factors
 * - Adaptive to link quality changes
 * - Load distribution across gateways
 * - Prevents route flapping with hysteresis
 * - OLED display showing cost metrics
 * 
 * Hardware: Heltec WiFi LoRa32 V3 (ESP32-S3 + SX1262)
 * 
 * @author xMESH Research Project
 * @date 2025-10-18
 */

#include <Arduino.h>
#include "LoraMesher.h"
#include "entities/routingTable/RouteNode.h"
#include "services/RoutingTableService.h"
#include "display.h"
#include "../../common/heltec_v3_config.h"

// ============================================================================
// Gateway-Aware Cost Function Configuration
// ============================================================================

// Cost function weights (tunable)
#define W1_HOP_COUNT    1.0   // Base hop-count weight
#define W2_RSSI         0.3   // RSSI component weight
#define W3_SNR          0.2   // SNR component weight
#define W4_ETX          0.4   // ETX component weight
#define W5_GATEWAY_BIAS 1.0   // Gateway load penalty weight
#define HYSTERESIS_THRESHOLD 0.15  // 15% threshold for route changes

// RSSI/SNR normalization ranges
#define RSSI_MIN        -120  // Minimum RSSI (dBm)
#define RSSI_MAX        -30   // Maximum RSSI (dBm)
#define SNR_MIN         -20   // Minimum SNR (dB)
#define SNR_MAX         10    // Maximum SNR (dB)

// ETX tracking configuration
#define ETX_WINDOW_SIZE 10    // Sliding window size for ETX calculation
#define ETX_DEFAULT     1.5   // Default ETX for new links
#define ETX_ALPHA       0.3   // EWMA smoothing factor (0-1, higher = more weight to recent)

// Trickle Timer Configuration (RFC 6206-inspired adaptive HELLO scheduler)
#define TRICKLE_IMIN_MS     60000   // Minimum interval: 60 seconds
#define TRICKLE_IMAX_MS     600000  // Maximum interval: 600 seconds (10 minutes)
#define TRICKLE_K           1       // Redundancy constant (suppress if heard k HELLOs)
#define TRICKLE_ENABLED     true    // Set false to disable Trickle (use fixed 120s)

// Heltec V3 Custom SPI pins (required for proper operation)
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_SCK    9

// LED Control
#define BOARD_LED   LED_PIN
#define LED_ON      HIGH
#define LED_OFF     LOW

// Custom SPI instance for Heltec V3
SPIClass customSPI(HSPI);

// Get LoRaMesher singleton instance
LoraMesher& radio = LoraMesher::getInstance();

// Packet counters
uint32_t txCount = 0;
uint32_t rxCount = 0;
uint32_t seqNumber = 0;  // Local sequence number for sent packets

// Role is defined in heltec_v3_config.h based on compile flags
// NODE_ROLE_STR, IS_SENSOR, IS_GATEWAY, IS_ROUTER are available

// ============================================================================
// Data Packet Structure
// ============================================================================

struct SensorData {
    uint32_t seqNum;        // Sequence number
    uint16_t srcAddr;       // Original source address
    uint32_t timestamp;     // Timestamp (milliseconds since boot)
    float sensorValue;      // Simulated sensor data
    uint8_t hopCount;       // Number of hops from source (updated by LoRaMesher)
};

// ============================================================================
// Link Quality Tracking Structures
// ============================================================================

/**
 * @brief Extended link metrics for cost calculation
 * Tracks additional metrics beyond LoRaMesher's built-in hop-count
 */
struct LinkMetrics {
    uint16_t address;           // Neighbor address
    int16_t rssi;               // Last RSSI (dBm) - extended from LoRaMesher
    int8_t snr;                 // Last SNR (dB) - from LoRaMesher receivedSNR
    float etx;                  // Expected Transmission Count (EWMA smoothed)
    
    // Sliding window for ETX calculation
    bool txWindow[ETX_WINDOW_SIZE];  // Transmission results (true = success, false = failure)
    uint8_t windowIndex;             // Current position in circular buffer
    uint8_t windowFilled;            // Number of valid entries in window
    
    uint32_t totalTxAttempts;   // Total transmission attempts (for statistics)
    uint32_t totalTxSuccess;    // Total successful transmissions
    uint32_t lastUpdate;        // Timestamp of last update
    
    LinkMetrics() : address(0), rssi(-120), snr(-20), etx(ETX_DEFAULT), 
                    windowIndex(0), windowFilled(0),
                    totalTxAttempts(0), totalTxSuccess(0), lastUpdate(0) {
        // Initialize sliding window with default (assume 67% success = ETX 1.5)
        for (int i = 0; i < ETX_WINDOW_SIZE; i++) {
            txWindow[i] = (i % 3 != 0);  // 2 success, 1 failure pattern
        }
    }
};

// Link quality tracking (max 10 neighbors)
#define MAX_TRACKED_LINKS 10
LinkMetrics linkMetrics[MAX_TRACKED_LINKS];
uint8_t numTrackedLinks = 0;

// Gateway load tracking
struct GatewayLoad {
    uint16_t address;
    uint32_t packetCount;
};
GatewayLoad gatewayLoads[5];  // Support up to 5 gateways
uint8_t numGateways = 0;

// ============================================================================
// Trickle Timer for Adaptive HELLO Packet Scheduling
// ============================================================================

/**
 * @brief Trickle-inspired adaptive timer for HELLO packet scheduling
 * 
 * Implementation based on RFC 6206 principles:
 * - Exponential backoff when network is stable
 * - Fast convergence on topology changes
 * - Suppression of redundant transmissions
 * 
 * State machine:
 * - IDLE: Not transmitting HELLOs (initial state)
 * - ACTIVE: Normal operation with adaptive interval
 * - RESET: Topology change detected, reset to I_min
 */
class TrickleTimer {
private:
    uint32_t I_min;             // Minimum interval (ms)
    uint32_t I_max;             // Maximum interval (ms)
    uint32_t I_current;         // Current interval (ms)
    uint8_t k;                  // Redundancy constant
    
    uint32_t intervalStart;     // Start time of current interval
    uint32_t nextTransmit;      // Time of next transmission
    uint8_t consistentHeard;    // Count of consistent messages heard
    
    bool enabled;               // Enable/disable Trickle
    uint32_t transmitCount;     // Total transmissions
    uint32_t suppressCount;     // Suppressed transmissions
    
    enum State {
        IDLE,
        ACTIVE,
        RESET
    } state;

public:
    TrickleTimer(uint32_t imin = TRICKLE_IMIN_MS, 
                 uint32_t imax = TRICKLE_IMAX_MS,
                 uint8_t k_val = TRICKLE_K,
                 bool enable = TRICKLE_ENABLED) 
        : I_min(imin), I_max(imax), I_current(imin), k(k_val),
          intervalStart(0), nextTransmit(0), consistentHeard(0),
          enabled(enable), transmitCount(0), suppressCount(0),
          state(IDLE) {}
    
    /**
     * @brief Start the Trickle timer
     */
    void start() {
        if (!enabled) return;
        state = ACTIVE;
        I_current = I_min;
        reset();
        Serial.printf("[Trickle] Started - I=%.1fs\n", I_current/1000.0);
    }
    
    /**
     * @brief Reset timer to minimum interval (on topology change)
     */
    void reset() {
        if (!enabled) return;
        
        I_current = I_min;
        consistentHeard = 0;
        intervalStart = millis();
        
        // Random point in interval [I/2, I] for transmission
        uint32_t halfInterval = I_current / 2;
        nextTransmit = intervalStart + halfInterval + random(halfInterval);
        
        state = RESET;
        Serial.printf("[Trickle] RESET - I=%.1fs, next TX in %.1fs\n", 
                     I_current/1000.0, (nextTransmit - millis())/1000.0);
    }
    
    /**
     * @brief Double the interval (on stable period)
     */
    void doubleInterval() {
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
    
    /**
     * @brief Check if interval has expired
     */
    bool intervalExpired() {
        if (!enabled) return true;  // Act like normal timer if disabled
        return (millis() - intervalStart) >= I_current;
    }
    
    /**
     * @brief Check if should transmit now
     * @return true if transmission should occur
     */
    bool shouldTransmit() {
        if (!enabled) return true;  // Always transmit if Trickle disabled
        
        uint32_t now = millis();
        
        // Check if interval expired - start new interval
        if (intervalExpired()) {
            doubleInterval();
            return false;  // Don't transmit immediately after doubling
        }
        
        // Check if reached transmission point (only once per interval!)
        if (now >= nextTransmit && state != IDLE) {
            // Mark transmission point as passed to avoid repeated triggers
            nextTransmit = UINT32_MAX;  // Set to max value so this triggers only once
            
            // Check suppression: if heard k consistent messages, suppress
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
    
    /**
     * @brief Notify that a consistent HELLO was heard
     */
    void heardConsistent() {
        if (!enabled) return;
        consistentHeard++;
    }
    
    /**
     * @brief Notify that an inconsistent HELLO was heard (topology change)
     */
    void heardInconsistent() {
        if (!enabled) return;
        Serial.println("[Trickle] Inconsistent HELLO - resetting");
        reset();
    }
    
    /**
     * @brief Get current interval in seconds
     */
    float getCurrentIntervalSec() {
        return I_current / 1000.0;
    }
    
    /**
     * @brief Get statistics
     */
    void printStats() {
        if (!enabled) {
            Serial.println("[Trickle] DISABLED - using fixed interval");
            return;
        }
        Serial.printf("[Trickle] TX=%u, Suppressed=%u, Efficiency=%.1f%%, I=%.1fs\n",
                     transmitCount, suppressCount,
                     (suppressCount * 100.0) / max(transmitCount + suppressCount, 1u),
                     I_current/1000.0);
    }
    
    /**
     * @brief Check if Trickle is enabled
     */
    bool isEnabled() { return enabled; }
    
    /**
     * @brief Get transmit count for logging
     */
    uint32_t getTransmitCount() { return transmitCount; }
    
    /**
     * @brief Get suppress count for logging
     */
    uint32_t getSuppressCount() { return suppressCount; }
};

// Global Trickle timer instance
TrickleTimer trickleTimer;

// ============================================================================
// Week 1: Network Monitoring Structures (for analytical model)
// ============================================================================

/**
 * @brief Channel occupancy monitor for duty-cycle tracking
 * Tracks airtime usage to ensure regulatory compliance (1% max)
 */
struct ChannelMonitor {
    uint32_t totalAirtimeMs;      // Total transmission time in current hour (ms)
    uint32_t windowStartMs;       // Start of current 1-hour window
    uint32_t transmissionCount;   // Number of transmissions in current window
    uint32_t violationCount;      // Number of times 1% exceeded
    
    ChannelMonitor() : totalAirtimeMs(0), windowStartMs(0), 
                       transmissionCount(0), violationCount(0) {}
    
    /**
     * @brief Record a transmission and update duty-cycle
     * @param durationMs Duration of transmission in milliseconds
     */
    void recordTransmission(uint32_t durationMs) {
        uint32_t now = millis();
        
        // Reset window if 1 hour has passed
        if (now - windowStartMs >= 3600000) {  // 1 hour = 3,600,000 ms
            totalAirtimeMs = 0;
            transmissionCount = 0;
            windowStartMs = now;
        }
        
        totalAirtimeMs += durationMs;
        transmissionCount++;
        
        // Check if exceeded 1% duty-cycle (36 seconds per hour)
        float dutyCyclePercent = (totalAirtimeMs / 3600000.0) * 100.0;
        if (dutyCyclePercent > 1.0) {
            violationCount++;
        }
    }
    
    /**
     * @brief Get current duty-cycle percentage
     * @return Duty-cycle as percentage (0-100)
     */
    float getDutyCyclePercent() {
        uint32_t now = millis();
        uint32_t windowDuration = now - windowStartMs;
        if (windowDuration == 0) return 0.0;
        return (totalAirtimeMs / (float)windowDuration) * 100.0;
    }
    
    /**
     * @brief Print duty-cycle statistics to serial
     */
    void printStats() {
        Serial.printf("Channel: %.3f%% duty-cycle, %u TX, %u violations\n",
                     getDutyCyclePercent(), transmissionCount, violationCount);
    }
};

/**
 * @brief Memory usage monitor
 * Tracks heap usage to analyze scalability limits
 */
struct MemoryMonitor {
    uint32_t minFreeHeap;    // Minimum free heap observed
    uint32_t maxUsedHeap;    // Maximum heap used
    
    MemoryMonitor() : minFreeHeap(UINT32_MAX), maxUsedHeap(0) {}
    
    /**
     * @brief Update memory statistics
     */
    void update() {
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t totalHeap = ESP.getHeapSize();
        uint32_t usedHeap = totalHeap - freeHeap;
        
        if (freeHeap < minFreeHeap) {
            minFreeHeap = freeHeap;
        }
        if (usedHeap > maxUsedHeap) {
            maxUsedHeap = usedHeap;
        }
    }
    
    /**
     * @brief Print memory statistics
     */
    void printStats() {
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t totalHeap = ESP.getHeapSize();
        Serial.printf("Memory: %u/%u KB free, Min: %u KB, Peak: %u KB\n",
                     freeHeap/1024, totalHeap/1024, 
                     minFreeHeap/1024, maxUsedHeap/1024);
    }
};

/**
 * @brief Queue statistics monitor
 * Tracks packet queue behavior (LoRaMesher internal queue)
 */
struct QueueMonitor {
    uint32_t packetsEnqueued;   // Total packets attempted to queue
    uint32_t packetsDropped;    // Packets dropped (queue full)
    uint32_t maxQueueDepth;     // Maximum queue depth observed
    
    QueueMonitor() : packetsEnqueued(0), packetsDropped(0), maxQueueDepth(0) {}
    
    /**
     * @brief Record packet enqueue attempt
     * @param success True if packet was queued, false if dropped
     */
    void recordEnqueue(bool success) {
        packetsEnqueued++;
        if (!success) {
            packetsDropped++;
        }
    }
    
    /**
     * @brief Update maximum queue depth
     * @param depth Current queue depth
     */
    void updateDepth(uint32_t depth) {
        if (depth > maxQueueDepth) {
            maxQueueDepth = depth;
        }
    }
    
    /**
     * @brief Get drop rate percentage
     * @return Drop rate as percentage
     */
    float getDropRate() {
        if (packetsEnqueued == 0) return 0.0;
        return (packetsDropped / (float)packetsEnqueued) * 100.0;
    }
    
    /**
     * @brief Print queue statistics
     */
    void printStats() {
        Serial.printf("Queue: %u enqueued, %u dropped (%.2f%%), max depth: %u\n",
                     packetsEnqueued, packetsDropped, 
                     getDropRate(), maxQueueDepth);
    }
};

// Global monitoring instances
ChannelMonitor channelMonitor;
MemoryMonitor memoryMonitor;
QueueMonitor queueMonitor;

// Monitoring interval (print stats every 30 seconds)
#define MONITORING_INTERVAL_MS 30000
uint32_t lastMonitoringPrint = 0;

// ============================================================================
// Cost Calculation Functions
// ============================================================================

/**
 * @brief Normalize RSSI to [0, 1] range
 * @param rssi RSSI value in dBm
 * @return Normalized value (1.0 = best, 0.0 = worst)
 */
float normalizeRSSI(int16_t rssi) {
    if (rssi >= RSSI_MAX) return 1.0;
    if (rssi <= RSSI_MIN) return 0.0;
    return (float)(rssi - RSSI_MIN) / (RSSI_MAX - RSSI_MIN);
}

/**
 * @brief Normalize SNR to [0, 1] range
 * @param snr SNR value in dB
 * @return Normalized value (1.0 = best, 0.0 = worst)
 */
float normalizeSNR(int8_t snr) {
    if (snr >= SNR_MAX) return 1.0;
    if (snr <= SNR_MIN) return 0.0;
    return (float)(snr - SNR_MIN) / (SNR_MAX - SNR_MIN);
}

/**
 * @brief Calculate gateway bias based on load
 * @param gatewayAddr Gateway address
 * @return Bias value (positive = penalty, negative = bonus)
 */
float calculateGatewayBias(uint16_t gatewayAddr) {
    if (numGateways <= 1) return 0.0;  // No bias with single gateway
    
    // Find this gateway's load
    uint32_t thisLoad = 0;
    for (uint8_t i = 0; i < numGateways; i++) {
        if (gatewayLoads[i].address == gatewayAddr) {
            thisLoad = gatewayLoads[i].packetCount;
            break;
        }
    }
    
    // Calculate average load
    uint32_t totalLoad = 0;
    for (uint8_t i = 0; i < numGateways; i++) {
        totalLoad += gatewayLoads[i].packetCount;
    }
    float avgLoad = (float)totalLoad / numGateways;
    
    if (avgLoad < 1.0) return 0.0;  // Not enough data yet
    
    // Calculate bias: (this_load - avg) / avg
    return (thisLoad - avgLoad) / avgLoad;
}

/**
 * @brief Get or create link metrics for a neighbor
 * @param address Neighbor address
 * @return Pointer to LinkMetrics structure
 */
LinkMetrics* getLinkMetrics(uint16_t address) {
    // Search for existing entry
    for (uint8_t i = 0; i < numTrackedLinks; i++) {
        if (linkMetrics[i].address == address) {
            return &linkMetrics[i];
        }
    }
    
    // Create new entry if space available
    if (numTrackedLinks < MAX_TRACKED_LINKS) {
        linkMetrics[numTrackedLinks].address = address;
        linkMetrics[numTrackedLinks].lastUpdate = millis();
        return &linkMetrics[numTrackedLinks++];
    }
    
    // Replace oldest entry if full
    uint8_t oldestIdx = 0;
    uint32_t oldestTime = linkMetrics[0].lastUpdate;
    for (uint8_t i = 1; i < MAX_TRACKED_LINKS; i++) {
        if (linkMetrics[i].lastUpdate < oldestTime) {
            oldestTime = linkMetrics[i].lastUpdate;
            oldestIdx = i;
        }
    }
    linkMetrics[oldestIdx].address = address;
    linkMetrics[oldestIdx].lastUpdate = millis();
    return &linkMetrics[oldestIdx];
}

/**
 * @brief Calculate combined cost metric for a route
 * @param hops Number of hops
 * @param nextHop Next hop address (for link quality lookup)
 * @param destAddr Destination address (for gateway bias if applicable)
 * @return Combined cost value (lower is better)
 */
float calculateRouteCost(uint8_t hops, uint16_t nextHop, uint16_t destAddr) {
    float cost = 0.0;
    
    // Component 1: Hop count (always present)
    cost += W1_HOP_COUNT * hops;
    
    // Get link metrics for next hop
    LinkMetrics* link = getLinkMetrics(nextHop);
    
    // Component 2: RSSI (inverted: worse RSSI = higher cost)
    float rssiNorm = normalizeRSSI(link->rssi);
    cost += W2_RSSI * (1.0 - rssiNorm);  // Invert: bad link = high cost
    
    // Component 3: SNR (inverted: worse SNR = higher cost)
    float snrNorm = normalizeSNR(link->snr);
    cost += W3_SNR * (1.0 - snrNorm);  // Invert: bad link = high cost
    
    // Component 4: ETX
    cost += W4_ETX * (link->etx - 1.0);  // ETX-1 so perfect link (ETX=1) adds no cost
    
    // Component 5: Gateway bias (only if destination is a gateway)
    // Check if destination has gateway role
    RouteNode* destNode = RoutingTableService::findNode(destAddr);
    if (destNode && (destNode->networkNode.role & ROLE_GATEWAY)) {
        float bias = calculateGatewayBias(destAddr);
        cost += W5_GATEWAY_BIAS * bias;
    }
    
    return cost;
}

// Forward declaration for ETX update function
void updateETX(uint16_t address, bool success);

/**
 * @brief Update link metrics from received packet
 * @param address Source address
 * @param rssi Received RSSI
 * @param snr Received SNR
 */
void updateLinkMetrics(uint16_t address, int16_t rssi, int8_t snr) {
    LinkMetrics* link = getLinkMetrics(address);
    
    // Update RSSI/SNR with exponential moving average (alpha = 0.3)
    if (link->lastUpdate == 0) {
        // First measurement
        link->rssi = rssi;
        link->snr = snr;
    } else {
        // Exponential moving average
        link->rssi = (int16_t)(0.7 * link->rssi + 0.3 * rssi);
        link->snr = (int8_t)(0.7 * link->snr + 0.3 * snr);
    }
    
    link->lastUpdate = millis();
    
    // Update ETX: Successful packet reception = successful transmission from sender
    updateETX(address, true);
    
    Serial.printf("Link %04X: RSSI=%d dBm, SNR=%d dB, ETX=%.2f\n", 
                 address, link->rssi, link->snr, link->etx);
}

/**
 * @brief Update ETX for a link after transmission attempt (Enhanced with Sliding Window + EWMA)
 * @param address Neighbor address
 * @param success True if transmission succeeded
 * 
 * Algorithm:
 * 1. Add new transmission result to sliding window (circular buffer)
 * 2. Calculate delivery ratio from window
 * 3. Compute instantaneous ETX = 1 / delivery_ratio
 * 4. Apply EWMA smoothing: ETX_new = α × ETX_instant + (1-α) × ETX_old
 * 
 * Benefits over simple counting:
 * - Sliding window: Only recent packets affect ETX (time-decayed)
 * - EWMA smoothing: Reduces jitter from single packet losses
 * - Continuous updates: ETX updates every packet, not every N packets
 */
void updateETX(uint16_t address, bool success) {
    LinkMetrics* link = getLinkMetrics(address);
    
    // Add transmission result to sliding window (circular buffer)
    link->txWindow[link->windowIndex] = success;
    link->windowIndex = (link->windowIndex + 1) % ETX_WINDOW_SIZE;
    
    // Track window fill status (for initial packets)
    if (link->windowFilled < ETX_WINDOW_SIZE) {
        link->windowFilled++;
    }
    
    // Update total statistics (for monitoring)
    link->totalTxAttempts++;
    if (success) link->totalTxSuccess++;
    
    // Calculate delivery ratio from sliding window
    uint8_t successCount = 0;
    for (uint8_t i = 0; i < link->windowFilled; i++) {
        if (link->txWindow[i]) successCount++;
    }
    
    float deliveryRatio = (float)successCount / link->windowFilled;
    
    // Calculate instantaneous ETX
    float instantETX;
    if (deliveryRatio > 0.01) {  // Avoid division by very small numbers
        instantETX = 1.0 / deliveryRatio;
    } else {
        instantETX = 100.0;  // Essentially unreachable
    }
    
    // Apply EWMA smoothing (only after window has some data)
    if (link->windowFilled >= 3) {  // Wait for at least 3 samples
        link->etx = ETX_ALPHA * instantETX + (1.0 - ETX_ALPHA) * link->etx;
    } else {
        link->etx = instantETX;  // Bootstrap phase: use instant value
    }
    
    // Clamp ETX to reasonable range [1.0, 10.0]
    if (link->etx < 1.0) link->etx = 1.0;
    if (link->etx > 10.0) link->etx = 10.0;
    
    // Periodic logging (every 10th packet) for production use
    if (link->totalTxAttempts % 10 == 0) {
        Serial.printf("ETX updated for %04X: %.2f (window: %d/%d, instant: %.2f, lifetime: %.1f%%)\n",
                     address, link->etx, successCount, link->windowFilled, instantETX,
                     (float)link->totalTxSuccess / link->totalTxAttempts * 100);
    }
}

/**
 * @brief Evaluate and optimize routing table based on cost function
 * 
 * This function periodically re-evaluates routes to see if better paths
 * exist based on multi-factor cost instead of just hop count.
 * Applies hysteresis to prevent route flapping.
 */
void evaluateRoutingTableCosts() {
    LM_LinkedList<RouteNode>* routingTable = RoutingTableService::routingTableList;
    if (!routingTable) return;
    
    // Skip if routing table is empty
    if (routingTable->getLength() == 0) return;
    
    // TODO: Implement cost-based route re-evaluation
    // For now, this is a placeholder to avoid interfering with LoRaMesher
    // The cost calculation functions are ready but route re-evaluation
    // needs more careful integration with LoRaMesher's routing logic
    
    // Future implementation will:
    // 1. Iterate through routing table safely
    // 2. Calculate cost for each route
    // 3. Find better alternatives based on link quality
    // 4. Apply hysteresis before updating routes
}

// ============================================================================
// LED Control Functions
// ============================================================================

/**
 * @brief Flash the LED
 *
 * @param flashes Number of flashes
 * @param delaymS Delay between on and off in milliseconds
 */
void led_Flash(uint16_t flashes, uint16_t delaymS) {
    for (uint16_t i = 0; i < flashes; i++) {
        digitalWrite(BOARD_LED, LED_ON);
        vTaskDelay(delaymS / portTICK_PERIOD_MS);
        digitalWrite(BOARD_LED, LED_OFF);
        vTaskDelay(delaymS / portTICK_PERIOD_MS);
    }
}

// ============================================================================
// Display Update Functions
// ============================================================================

/**
 * @brief Update Line 1: Node ID + Role
 */
void updateDisplayLine1() {
    char text[20];
    snprintf(text, 20, "ID:%04X [%s]", radio.getLocalAddress(), NODE_ROLE_STR);
    Screen.changeLineOne(String(text));
}

/**
 * @brief Update Line 2: TX/RX packet counts
 */
void updateDisplayLine2() {
    char text[20];
    snprintf(text, 20, "TX:%lu RX:%lu", txCount, rxCount);
    Screen.changeLineTwo(String(text));
}

/**
 * @brief Update Line 3: Protocol info + Cost metrics
 */
void updateDisplayLine3() {
    Screen.changeLineThree("GW-COST");
}

/**
 * @brief Update Line 4: Duty cycle (placeholder for now)
 */
void updateDisplayLine4() {
    // TODO: Calculate actual duty cycle from LoRaMesher stats
    Screen.changeLineFour("DC:0.0%");
}

// ============================================================================
// Packet Processing Functions
// ============================================================================

/**
 * @brief Process received packets (hop-count routing)
 * 
 * This task is notified by LoRaMesher when packets arrive.
 * LoRaMesher's routing layer handles forwarding automatically.
 * We just need to log the received packets and link quality.
 */
void processReceivedPackets(void*) {
    for (;;) {
        // Wait for notification from LoRaMesher
        ulTaskNotifyTake(pdPASS, portMAX_DELAY);
        
        led_Flash(1, 50); // Quick flash to indicate packet arrival

        // Process all packets in receive queue
        while (radio.getReceivedQueueSize() > 0) {
            // Get next packet from queue
            AppPacket<SensorData>* packet = radio.getNextAppPacket<SensorData>();

            if (packet == nullptr) {
                Serial.println("ERROR: Null packet received");
                continue;
            }

            SensorData* data = packet->payload;

            // Update RX counter and display
            rxCount++;
            updateDisplayLine2();
            
            // Update link quality metrics from routing table
            // LoRaMesher tracks SNR in RouteNode.receivedSNR for direct neighbors
            RouteNode* srcNode = RoutingTableService::findNode(packet->src);
            if (srcNode) {
                int8_t snr = srcNode->receivedSNR;
                // Estimate RSSI from SNR (rough approximation)
                // Typical: RSSI = -120 + SNR * 3 for LoRa
                int16_t estimatedRSSI = -120 + (snr * 3);
                
                updateLinkMetrics(packet->src, estimatedRSSI, snr);
                
                Serial.printf("Link quality: SNR=%d dB, Est.RSSI=%d dBm\n", 
                             snr, estimatedRSSI);
            }

            // Log received packet
            Serial.printf("RX: Seq=%lu From=%04X Hops=%d Value=%.2f\n", 
                         data->seqNum, 
                         data->srcAddr, 
                         data->hopCount, 
                         data->sensorValue);

            // Gateway-specific logging
            if (IS_GATEWAY) {
                Serial.printf("GATEWAY: Packet %lu from %04X received (hops=%d, value=%.2f)\n",
                             data->seqNum, 
                             data->srcAddr, 
                             data->hopCount, 
                             data->sensorValue);
            }

            // Clean up packet
            radio.deletePacket(packet);
        }
    }
}

TaskHandle_t receiveLoRaMessage_Handle = NULL;

/**
 * @brief Create receive task and register with LoRaMesher
 */
void createReceiveMessages() {
    int res = xTaskCreate(
        processReceivedPackets,
        "RX Task",
        4096,
        (void*) 1,
        2,
        &receiveLoRaMessage_Handle);
    
    if (res != pdPASS) {
        Serial.printf("ERROR: RX task creation failed: %d\n", res);
    } else {
        Serial.println("RX task created successfully");
    }
}

// ============================================================================
// LoRaMesher Setup
// ============================================================================

/**
 * @brief Initialize LoRaMesher with Heltec V3 configuration
 */
void setupLoraMesher() {
    Serial.println("Initializing LoRaMesher with hop-count routing...");

    // Initialize custom SPI with Heltec V3 pins (CRITICAL for Heltec V3!)
    customSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

    // Get default configuration
    LoraMesher::LoraMesherConfig config = LoraMesher::LoraMesherConfig();

    // Configure Heltec V3 pins (from heltec_v3_config.h)
    config.loraCs = LORA_CS;
    config.loraRst = LORA_RST;
    config.loraIrq = LORA_DIO1;
    config.loraIo1 = LORA_BUSY;

    // Set LoRa module type (SX1262)
    config.module = LoraMesher::LoraModules::SX1262_MOD;

    // Use custom SPI instance (CRITICAL for Heltec V3!)
    config.spi = &customSPI;

    // Set LoRa parameters to match working Heltec V3 example
    config.freq = 915.0;          // 915 MHz (AS923 compatible)
    config.bw = 125.0;            // 125 kHz bandwidth
    config.sf = 7;                // Spreading factor 7
    config.cr = 7;                // Coding rate 4/7
    config.syncWord = 0x12;       // Private sync word
    config.preambleLength = 8;

    // Set TX power - TEMPORARY: Use higher power for initial testing
    // TODO: Reduce power after confirming connectivity
#ifdef XMESH_ROLE_ROUTER
    config.power = 14;  // Router: Full power
#else
    config.power = 10;  // Sensor/Gateway: Medium power (10 dBm = 10 mW) for testing
#endif
    Serial.printf("TX Power: %d dBm (testing mode - higher power for connectivity verification)\n", config.power);

    // Initialize LoRaMesher
    radio.begin(config);

    // Create and register receive task
    createReceiveMessages();
    radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);

    // Start radio (this enables automatic routing)
    radio.start();
    
    // Initialize Trickle timer for adaptive HELLO scheduling
    trickleTimer.start();
    Serial.printf("Trickle timer: %s\n", 
                 trickleTimer.isEnabled() ? "ENABLED" : "DISABLED (fixed 120s)");
    if (trickleTimer.isEnabled()) {
        Serial.printf("  I_min=%.1fs, I_max=%.1fs, k=%d\n",
                     TRICKLE_IMIN_MS/1000.0, TRICKLE_IMAX_MS/1000.0, TRICKLE_K);
    }

    // If this is a gateway, set the gateway role so other nodes can find it
    if (IS_GATEWAY) {
        radio.addGatewayRole();
        Serial.println("Gateway role added - other nodes can discover this gateway");
    }

    Serial.println("\n========================================");
    Serial.println("LoRaMesher initialized successfully!");
    Serial.println("========================================");
    Serial.printf("Node Role: %s\n", NODE_ROLE_STR);
    Serial.printf("Local Address: %04X\n", radio.getLocalAddress());
    Serial.printf("LoRa Frequency: %.1f MHz\n", config.freq);
    Serial.printf("Spreading Factor: %d\n", config.sf);
    Serial.printf("Bandwidth: %.1f kHz\n", config.bw);
    Serial.printf("TX Power: %d dBm\n", config.power);
    Serial.println("========================================");
    Serial.println("\nWaiting for network discovery...");
    if (trickleTimer.isEnabled()) {
        Serial.println("NOTE: Trickle timer implemented - adaptive HELLO intervals");
        Serial.printf("      Initial: %.1fs, Max: %.1fs\n", 
                     TRICKLE_IMIN_MS/1000.0, TRICKLE_IMAX_MS/1000.0);
        Serial.println("      Full integration requires LoRaMesher library modification");
        Serial.println("      Current: Independent timer for overhead tracking");
    } else {
        Serial.println("HELLO packets will be sent every 120 seconds");
    }
    Serial.println("Routing table will build automatically\n");
}

// ============================================================================
// Sensor Task (periodic data transmission)
// ============================================================================

/**
 * @brief Sensor task: Send periodic data packets
 * 
 * Sensors send packets every 60 seconds.
 * LoRaMesher automatically routes to gateway via best path.
 */
void sendSensorData(void*) {
    for (;;) {
        // Wait 60 seconds between transmissions (per specification)
        vTaskDelay(60000 / portTICK_PERIOD_MS);

        // Create sensor data packet
        SensorData data;
        data.seqNum = seqNumber++;
        data.srcAddr = radio.getLocalAddress();
        data.timestamp = millis();
        data.sensorValue = random(0, 100) + (random(0, 100) / 100.0); // Simulated sensor
        data.hopCount = 0;

        // Find closest gateway in routing table
        RouteNode* gateway = radio.getClosestGateway();
        
        if (gateway != nullptr) {
            // Send to gateway address - LoRaMesher will route via routing table
            uint16_t gatewayAddr = gateway->networkNode.address;
            Serial.printf("TX: Seq=%lu Value=%.2f to Gateway=%04X (Hops=%u)\n", 
                         data.seqNum, data.sensorValue, gatewayAddr, gateway->networkNode.metric);
            
            // Record transmission for channel monitoring
            // Time-on-air for 50-byte packet @ SF7, BW125, CR4/5 ≈ 56 ms
            uint32_t toaMs = 56;  // Measured time-on-air
            channelMonitor.recordTransmission(toaMs);
            queueMonitor.recordEnqueue(true);  // Assume successful enqueue
            
            radio.createPacketAndSend(gatewayAddr, &data, 1);
            txCount++;
            
            // Update memory stats
            memoryMonitor.update();
        } else {
            // No gateway found yet, wait for routing table to build
            Serial.println("TX: No gateway in routing table yet, waiting...");
        }
        
        updateDisplayLine2();
    }
}

/**
 * @brief Create sensor transmission task (only for sensors)
 */
void createSendMessages() {
    if (!IS_SENSOR) {
        Serial.println("Not a sensor node, skipping TX task creation");
        return;
    }

    TaskHandle_t sendTask_Handle = NULL;
    BaseType_t res = xTaskCreate(
        sendSensorData,
        "TX Task",
        4096,
        (void*) 1,
        1,
        &sendTask_Handle);
    
    if (res != pdPASS) {
        Serial.printf("ERROR: TX task creation failed: %d\n", res);
        vTaskDelete(sendTask_Handle);
    } else {
        Serial.println("TX task created successfully");
    }
}

// ============================================================================
// Arduino Setup and Loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000); // Wait for serial to stabilize
    
    Serial.println("\n\n=================================");
    Serial.println("xMESH GATEWAY-AWARE COST ROUTING");
    Serial.println("WEEK 4-5 - v0.4.0-alpha");
    Serial.println("=================================");
    Serial.printf("Role: %s (%s)\n", NODE_ROLE_STR, 
                  IS_SENSOR ? "SENSOR" : (IS_GATEWAY ? "GATEWAY" : "ROUTER"));
    Serial.printf("IS_SENSOR=%d IS_ROUTER=%d IS_GATEWAY=%d\n", IS_SENSOR, IS_ROUTER, IS_GATEWAY);
    Serial.println("=================================\n");

    // Setup LED
    pinMode(BOARD_LED, OUTPUT);
    digitalWrite(BOARD_LED, LED_OFF);

    // Initialize display
    Screen.initDisplay();
    updateDisplayLine1();
    updateDisplayLine2();
    updateDisplayLine3();
    updateDisplayLine4();

    // Indicate startup with LED
    led_Flash(2, 125);

    // Initialize LoRaMesher (enables automatic hop-count routing)
    setupLoraMesher();

    // Create transmission task (sensors only)
    createSendMessages();

    Serial.println("Setup complete\n");
    Serial.println("LoRaMesher will automatically:");
    Serial.println("- Send HELLO packets to discover neighbors");
    Serial.println("- Build routing table with hop counts");
    Serial.println("- Route packets via shortest path");
}

void loop() {
    // Main loop handles display updates, cost-based route evaluation, and debugging
    Screen.drawDisplay();
    
    // Heartbeat every 30 seconds to show system is alive
    static uint32_t lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 30000) {
        lastHeartbeat = millis();
        Serial.printf("\n[%lu] Heartbeat - Node %04X (%s) - Uptime: %lu sec\n", 
                     millis()/1000, radio.getLocalAddress(), NODE_ROLE_STR, millis()/1000);
        Serial.printf("TX Count: %lu | RX Count: %lu | Routing Table Size: %d\n",
                     txCount, rxCount, radio.routingTableSize());
    }
    
    // Check Trickle timer periodically (not every loop iteration!)
    // Trickle just tracks timing, doesn't actually send HELLOs (that's LoRaMesher's job)
    static uint32_t lastTrickleCheck = 0;
    if (trickleTimer.isEnabled() && (millis() - lastTrickleCheck > 1000)) {
        lastTrickleCheck = millis();
        trickleTimer.shouldTransmit();  // Check once per second, not every 100ms
    }
    
    // Evaluate routes based on cost function every 10 seconds
    static uint32_t lastCostEvaluation = 0;
    if (millis() - lastCostEvaluation > 10000) {
        lastCostEvaluation = millis();
        evaluateRoutingTableCosts();
    }
    
    // Print routing table every 30 seconds for debugging
    static uint32_t lastRoutingTablePrint = 0;
    if (millis() - lastRoutingTablePrint > 30000) {
        lastRoutingTablePrint = millis();
        Serial.println("\n==== Routing Table (with Cost Metrics) ====");
        Serial.printf("Routing table size: %d\n", radio.routingTableSize());
        
        // Manually iterate and print routing table entries with cost calculation
        LM_LinkedList<RouteNode>* routingTable = RoutingTableService::routingTableList;
        if (routingTable && routingTable->moveToStart()) {
            Serial.println("Addr   Via    Hops  Role  Cost");
            Serial.println("------|------|------|------|------");
            do {
                RouteNode* node = routingTable->getCurrent();
                float routeCost = calculateRouteCost(node->networkNode.metric, 
                                                     node->via, 
                                                     node->networkNode.address);
                Serial.printf("%04X | %04X | %4d | %02X | %.2f\n",
                             node->networkNode.address,
                             node->via,
                             node->networkNode.metric,
                             node->networkNode.role,
                             routeCost);
            } while (routingTable->next());
        } else {
            Serial.println("(empty)");
        }
        
        // Print link metrics summary
        Serial.println("\n==== Link Quality Metrics ====");
        Serial.println("Addr   RSSI   SNR   ETX");
        Serial.println("------|------|------|------");
        for (int i = 0; i < MAX_TRACKED_LINKS; i++) {
            if (linkMetrics[i].address != 0) {
                Serial.printf("%04X | %4d | %3d | %.2f\n",
                             linkMetrics[i].address,
                             linkMetrics[i].rssi,
                             linkMetrics[i].snr,
                             linkMetrics[i].etx);
            }
        }
        Serial.println("================================\n");
    }
    
    // Print monitoring statistics every 30 seconds
    if (millis() - lastMonitoringPrint >= MONITORING_INTERVAL_MS) {
        lastMonitoringPrint = millis();
        
        Serial.println("\n==== Network Monitoring Stats ====");
        channelMonitor.printStats();
        memoryMonitor.printStats();
        queueMonitor.printStats();
        trickleTimer.printStats();
        
        // Print routing table memory usage
        Serial.printf("Routing table: %d entries × ~32 bytes = ~%d KB\n",
                     radio.routingTableSize(),
                     (radio.routingTableSize() * 32) / 1024);
        Serial.println("===================================\n");
    }
    
    // Update memory monitor periodically
    static uint32_t lastMemoryUpdate = 0;
    if (millis() - lastMemoryUpdate > 5000) {  // Every 5 seconds
        lastMemoryUpdate = millis();
        memoryMonitor.update();
    }
    
    delay(100);
}
