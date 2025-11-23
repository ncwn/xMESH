/**
 * @file main.cpp
 * @brief Protocol 1 - Flooding Baseline Implementation
 *
 * This implements a simple flooding protocol where:
 * - Sensors broadcast packets every 60 seconds
 * - All nodes rebroadcast received packets (except duplicates)
 * - Gateway terminates the flood
 * - Duplicate detection using (src, sequence) cache
 */

#include <Arduino.h>
#include "LoraMesher.h"
#include "config.h"
#include "../common/display_utils.h"
#include "../common/logging.h"
#include "../common/duty_cycle.h"

// Get LoRaMesher singleton instance
LoraMesher& radio = LoraMesher::getInstance();

// Global objects
NodeStatus nodeStatus;
FloodingStats stats;
DuplicateEntry duplicateCache[DUPLICATE_CACHE_SIZE];
uint16_t sequenceNumber = 0;

// Timing variables
unsigned long lastDisplayUpdate = 0;
unsigned long lastHeapCheck = 0;

// Packet structure for LoRaMesher
struct SensorPacket {
    uint16_t src;           // Source address
    uint16_t sequence;      // Sequence number
    uint8_t ttl;            // Time to live
    uint32_t timestamp;     // Packet creation time
    float sensorValue;      // Simulated sensor data
};

// ============================================================================
// Duplicate Detection
// ============================================================================

bool isDuplicate(uint16_t src, uint16_t seq) {
    for (int i = 0; i < DUPLICATE_CACHE_SIZE; i++) {
        if (duplicateCache[i].valid &&
            duplicateCache[i].src == src &&
            duplicateCache[i].sequence == seq) {
            return true;
        }
    }
    return false;
}

void addToCache(uint16_t src, uint16_t seq) {
    static int cacheIndex = 0;
    duplicateCache[cacheIndex].src = src;
    duplicateCache[cacheIndex].sequence = seq;
    duplicateCache[cacheIndex].timestamp = millis();
    duplicateCache[cacheIndex].valid = true;
    cacheIndex = (cacheIndex + 1) % DUPLICATE_CACHE_SIZE;
}

// ============================================================================
// Packet Processing - Receive Task
// ============================================================================

/**
 * @brief Process received packets (flooding logic)
 */
void processReceivedPackets(void*) {
    for (;;) {
        // Wait for notification from LoRaMesher
        ulTaskNotifyTake(pdPASS, portMAX_DELAY);

        // Process all packets in receive queue
        while (radio.getReceivedQueueSize() > 0) {
            AppPacket<SensorPacket>* packet = radio.getNextAppPacket<SensorPacket>();

            if (packet == nullptr) {
                LOG_ERROR("Null packet received");
                continue;
            }

            SensorPacket* data = packet->payload;

            // Check for duplicate
            if (isDuplicate(data->src, data->sequence)) {
                if (DEBUG_FLOODING) {
                    LOG_DEBUG("DUPLICATE: Seq=%u From=%04X", data->sequence, data->src);
                }
                stats.duplicatesDetected++;
                radio.deletePacket(packet);
                continue;
            }

            // Add to cache
            addToCache(data->src, data->sequence);

            // Update statistics
            stats.packetsReceived++;

            // Log received packet
            LOG_INFO("RX: Seq=%u From=%04X TTL=%d Value=%.2f",
                     data->sequence, data->src, data->ttl, data->sensorValue);

            // Gateway behavior: Terminate flood (don't rebroadcast)
            if (NODE_ROLE == XMESH_ROLE_GATEWAY) {
                LOG_INFO("GATEWAY: Packet %u terminated", data->sequence);
            }
            // Relay behavior: Rebroadcast to extend network range
            else if (NODE_ROLE == XMESH_ROLE_RELAY && data->ttl > 0) {
                data->ttl--;

                // Log relay forwarding at INFO level for visibility
                LOG_INFO("RELAY: Seq=%u From=%04X TTL=%d->%d", data->sequence, data->src, data->ttl + 1, data->ttl);

                // Rebroadcast
                radio.createPacketAndSend(BROADCAST_ADDRESS, data, 1);
                stats.packetsForwarded++;

                // Update duty cycle
                updateDutyCycle(PACKET_HEADER_SIZE + sizeof(SensorPacket));
            }
            // Sensor behavior: Receive only (don't rebroadcast)
            // Per design spec: Sensors only transmit their own data, never forward others'
            else if (NODE_ROLE == XMESH_ROLE_SENSOR) {
                if (DEBUG_FLOODING) {
                    LOG_DEBUG("SENSOR: Received Seq=%u, not rebroadcasting (sensors only TX own data)", data->sequence);
                }
            }

            // Clean up
            radio.deletePacket(packet);
        }
    }
}

TaskHandle_t receiveLoRaMessage_Handle = NULL;

/**
 * @brief Create receive task and register with LoRaMesher
 */
void createReceiveTask() {
    int res = xTaskCreate(
        processReceivedPackets,
        "RX Task",
        4096,
        (void*) 1,
        2,
        &receiveLoRaMessage_Handle);

    if (res != pdPASS) {
        LOG_ERROR("RX task creation failed: %d", res);
    } else {
        LOG_INFO("RX task created successfully");
        radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);
    }
}

// ============================================================================
// Sensor Task - Periodic Transmission
// ============================================================================

/**
 * @brief Sensor task: Send periodic data packets
 */
void sendSensorData(void*) {
    for (;;) {
        // Wait for packet interval (60 seconds)
        uint32_t interval = PACKET_INTERVAL_MS + random(-PACKET_INTERVAL_VARIATION, PACKET_INTERVAL_VARIATION);
        vTaskDelay(interval / portTICK_PERIOD_MS);

        // Check duty cycle
        if (!checkDutyCycle(PACKET_HEADER_SIZE + sizeof(SensorPacket))) {
            LOG_WARN("Duty cycle limit reached, skipping transmission");
            continue;
        }

        // Create sensor data packet
        SensorPacket data;
        data.src = NODE_ADDRESS;
        data.sequence = sequenceNumber++;
        data.ttl = MAX_TTL;
        data.timestamp = millis();
        data.sensorValue = random(0, 100) + (random(0, 100) / 100.0);

        // Send packet
        LOG_INFO("TX: Seq=%u Value=%.2f TTL=%d", data.sequence, data.sensorValue, data.ttl);
        radio.createPacketAndSend(BROADCAST_ADDRESS, &data, 1);

        // Update statistics
        stats.packetsTransmitted++;
        updateDutyCycle(PACKET_HEADER_SIZE + sizeof(SensorPacket));
    }
}

/**
 * @brief Create sensor transmission task (only for sensors)
 */
void createSendTask() {
    if (NODE_ROLE != XMESH_ROLE_SENSOR) {
        LOG_INFO("Not a sensor node, skipping TX task creation");
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
        LOG_ERROR("TX task creation failed: %d", res);
        vTaskDelete(sendTask_Handle);
    } else {
        LOG_INFO("TX task created successfully");
    }
}

// ============================================================================
// LoRaMesher Setup
// ============================================================================

void setupLoraMesher() {
    LOG_INFO("Initializing LoRaMesher...");

    // Get default configuration
    LoraMesher::LoraMesherConfig config = LoraMesher::LoraMesherConfig();

    // Configure Heltec V3 pins
    config.loraCs = LORA_CS_PIN;
    config.loraRst = LORA_RST_PIN;
    config.loraIrq = LORA_DIO1_PIN;
    config.loraIo1 = LORA_BUSY_PIN;

    // Set LoRa module type (SX1262)
    config.module = LoraMesher::LoraModules::SX1262_MOD;

    // Set LoRa parameters
    config.freq = LORA_FREQUENCY;
    config.bw = LORA_BANDWIDTH;
    config.sf = LORA_SPREADING_FACTOR;
    config.cr = LORA_CODING_RATE;
    config.power = LORA_TX_POWER;

    // Initialize LoRaMesher
    radio.begin(config);

    // Create and register receive task
    createReceiveTask();

    // Start radio
    radio.start();

    LOG_INFO("LoRaMesher initialized");
    LOG_INFO("Local address: %04X", radio.getLocalAddress());
    LOG_INFO("Frequency: %.1f MHz, SF: %d, BW: %.1f kHz, Power: %d dBm",
             LORA_FREQUENCY, LORA_SPREADING_FACTOR, LORA_BANDWIDTH, LORA_TX_POWER);
}

// ============================================================================
// Node Setup
// ============================================================================

void setupNode() {
    // Initialize pins
    pinMode(LED_PIN, OUTPUT);
    pinMode(PRG_BUTTON, INPUT_PULLUP);
    pinMode(VEXT_CTRL_PIN, OUTPUT);

    // Enable external power for peripherals
    ENABLE_VEXT();
    delay(100);

    // Initialize duplicate cache
    for (int i = 0; i < DUPLICATE_CACHE_SIZE; i++) {
        duplicateCache[i].valid = false;
    }

    // Initialize node status
    nodeStatus.nodeId = NODE_ID;
    nodeStatus.nodeRole = NODE_ROLE;
    nodeStatus.gatewayAddr = (NODE_ROLE == XMESH_ROLE_GATEWAY) ? NODE_ADDRESS : 0x0005;
    nodeStatus.statusMessage = "Idle";

    // Initialize statistics
    memset(&stats, 0, sizeof(stats));
}

// ============================================================================
// Display Update
// ============================================================================

void updateNodeStatus() {
    // Packet statistics
    nodeStatus.txPackets = stats.packetsTransmitted;
    nodeStatus.rxPackets = stats.packetsReceived;
    nodeStatus.fwdPackets = stats.packetsForwarded;
    nodeStatus.dropPackets = stats.packetsDropped;

    // Duty cycle
    nodeStatus.dutyCyclePercent = dutyCycle.getCurrentPercentage();
    nodeStatus.airtimeMs = dutyCycle.getCurrentAirtime();

    // System metrics
    nodeStatus.uptimeMs = millis();
    nodeStatus.freeHeap = ESP.getFreeHeap();
    nodeStatus.cpuUsage = 0.0;  // Not calculated for Protocol 1

    // Link quality (not used in Protocol 1 flooding)
    nodeStatus.rssi = 0.0;
    nodeStatus.snr = 0.0;
    nodeStatus.etx = 0.0;

    // Routing info (not used in Protocol 1 flooding)
    nodeStatus.routeCount = 0;
    nodeStatus.nextHopAddr = 0;
    nodeStatus.routeCost = 0.0;
}

// ============================================================================
// System Health Check
// ============================================================================

void checkSystemHealth() {
    uint32_t freeHeap = ESP.getFreeHeap();

    if (freeHeap < MIN_FREE_HEAP) {
        LOG_WARN("Low memory: %lu bytes free", freeHeap);
    }

    if (DEBUG_MEMORY) {
        LOG_DEBUG("Free heap: %lu bytes", freeHeap);
    }
}

// ============================================================================
// Arduino Setup and Loop
// ============================================================================

void setup() {
    // Initialize serial logging
    logger.begin(SERIAL_BAUD, CSV_OUTPUT);
    logger.setLevel((LogLevel)LOG_LEVEL);

    LOG_INFO("===========================================");
    LOG_INFO("xMESH Protocol 1 - Flooding Baseline");
    LOG_INFO("Node ID: %d, Role: %d, Address: 0x%04X", NODE_ID, NODE_ROLE, NODE_ADDRESS);
    LOG_INFO("===========================================");

    // Setup node hardware and state
    setupNode();

    // Initialize display
    initDisplay();
    displayMessage("Initializing...");

    // Initialize duty cycle monitor
    initDutyCycle(LORA_SPREADING_FACTOR, LORA_BANDWIDTH, LORA_CODING_RATE);

    // Initialize LoRaMesher
    setupLoraMesher();

    // Create sensor transmission task (for sensors only)
    createSendTask();

    // Setup complete
    LOG_INFO("Setup complete, starting main loop");
    displayMessage("Ready");

    // Add random delay to prevent synchronized transmissions
    delay(random(0, 5000));
}

void loop() {
    // Update display
    if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
        updateNodeStatus();
        updateDisplay(nodeStatus);
        lastDisplayUpdate = millis();
    }

    // Check system health
    if (millis() - lastHeapCheck >= HEAP_CHECK_INTERVAL_MS) {
        checkSystemHealth();
        lastHeapCheck = millis();
    }

    // Handle button press for page switching
    if (digitalRead(PRG_BUTTON) == LOW) {
        displayManager.nextPage();
        delay(200); // Debounce
    }

    // Small delay to prevent watchdog timeout
    delay(10);
}
