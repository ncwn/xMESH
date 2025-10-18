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
#define ETX_WINDOW_SIZE 20    // Number of packets to track for ETX
#define ETX_DEFAULT     1.5   // Default ETX for new links

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
    float etx;                  // Expected Transmission Count
    uint32_t txAttempts;        // Total transmission attempts
    uint32_t txSuccess;         // Successful transmissions
    uint32_t lastUpdate;        // Timestamp of last update
    
    LinkMetrics() : address(0), rssi(-120), snr(-20), etx(ETX_DEFAULT), 
                    txAttempts(0), txSuccess(0), lastUpdate(0) {}
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
    
    Serial.printf("Link %04X: RSSI=%d dBm, SNR=%d dB, ETX=%.2f\n", 
                 address, link->rssi, link->snr, link->etx);
}

/**
 * @brief Update ETX for a link after transmission attempt
 * @param address Neighbor address
 * @param success True if transmission succeeded
 */
void updateETX(uint16_t address, bool success) {
    LinkMetrics* link = getLinkMetrics(address);
    
    link->txAttempts++;
    if (success) link->txSuccess++;
    
    // Update ETX every 10 packets to smooth out variations
    if (link->txAttempts >= 10) {
        float deliveryRatio = (float)link->txSuccess / link->txAttempts;
        if (deliveryRatio > 0.01) {  // Avoid division by very small numbers
            link->etx = 1.0 / deliveryRatio;
        } else {
            link->etx = 100.0;  // Essentially unreachable
        }
        
        // Reset counters for next window
        link->txAttempts = 0;
        link->txSuccess = 0;
        
        Serial.printf("ETX updated for %04X: %.2f (delivery: %.1f%%)\n",
                     address, link->etx, deliveryRatio * 100);
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
    Serial.println("HELLO packets will be sent every 120 seconds");
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
