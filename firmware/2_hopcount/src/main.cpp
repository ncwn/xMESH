/**
 * @file main.cpp
 * @brief Protocol 2 - Hop-Count Routing using LoRaMesher
 *
 * This implements hop-count routing where:
 * - HELLO packets build routing tables automatically
 * - Packets are forwarded via unicast to next hop (not broadcast)
 * - Routing metric is hop count (shortest path)
 * - LoRaMesher library handles route discovery and maintenance
 */

#include <Arduino.h>
#include "LoraMesher.h"
#include "entities/routingTable/RouteNode.h"
#include "services/RoutingTableService.h"
#include "config.h"
#include "../common/display_utils.h"
#include "../common/logging.h"
#include "../common/duty_cycle.h"

// Get LoRaMesher singleton instance
LoraMesher& radio = LoraMesher::getInstance();

// Global objects
NodeStatus nodeStatus;
HopCountStats stats;
uint16_t sequenceNumber = 0;

// Timing variables
unsigned long lastDisplayUpdate = 0;
unsigned long lastHeapCheck = 0;
unsigned long lastRoutingTablePrint = 0;

// Packet structure for LoRaMesher
struct SensorPacket {
    uint16_t src;           // Source address
    uint16_t sequence;      // Sequence number
    uint32_t timestamp;     // Packet creation time
    float sensorValue;      // Simulated sensor data
    uint8_t hopCount;       // Hop count (updated by LoRaMesher)
};

// ============================================================================
// Packet Processing - Receive Task
// ============================================================================

/**
 * @brief Process received packets (LoRaMesher handles routing)
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

            // Update statistics
            stats.dataPacketsReceived++;

            // Blink LED
            if (LED_BLINK_ON_RX) {
                digitalWrite(LED_PIN, HIGH);
                delay(50);
                digitalWrite(LED_PIN, LOW);
            }

            // Log received packet
            LOG_INFO("RX: Seq=%u From=%04X Hops=%d Value=%.2f",
                     data->sequence, data->src, data->hopCount, data->sensorValue);

            // Gateway-specific logging
            if (NODE_ROLE == XMESH_ROLE_GATEWAY) {
                LOG_INFO("GATEWAY: Packet %u from %04X received (hops=%d, value=%.2f)",
                         data->sequence, data->src, data->hopCount, data->sensorValue);

                // Log in CSV format
                logPacketReceive(data->src, NODE_ADDRESS, 0, 0, data->sequence);

                nodeStatus.statusMessage = "Packet RX";
            } else {
                nodeStatus.statusMessage = "Received";
            }

            // Clean up packet
            radio.deletePacket(packet);
        }
    }
}

TaskHandle_t receiveTask_Handle = NULL;

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
        &receiveTask_Handle);

    if (res != pdPASS) {
        LOG_ERROR("RX task creation failed: %d", res);
    } else {
        LOG_INFO("RX task created successfully");
        radio.setReceiveAppDataTaskHandle(receiveTask_Handle);
    }
}

// ============================================================================
// Sensor Task (periodic data transmission)
// ============================================================================

/**
 * @brief Sensor task: Send periodic data packets
 */
void sendSensorData(void*) {
    for (;;) {
        // Wait between transmissions with random variation
        uint32_t interval = PACKET_INTERVAL_MS + random(-PACKET_INTERVAL_VARIATION, PACKET_INTERVAL_VARIATION);
        vTaskDelay(interval / portTICK_PERIOD_MS);

        // Check duty cycle before sending
        size_t packetSize = sizeof(SensorPacket) + 12; // Add header estimate
        if (!checkDutyCycle(packetSize)) {
            LOG_WARN("Duty cycle limit reached, skipping transmission");
            stats.dataPacketsDropped++;
            continue;
        }

        // Create sensor data packet
        SensorPacket data;
        data.src = radio.getLocalAddress();
        data.sequence = sequenceNumber++;
        data.timestamp = millis();
        data.sensorValue = random(0, 100) + (random(0, 100) / 100.0); // Simulated sensor
        data.hopCount = 0;

        // Find gateway in routing table
        RouteNode* gateway = radio.getClosestGateway();

        if (gateway != nullptr) {
            uint16_t gatewayAddr = gateway->networkNode.address;
            uint8_t hopCount = gateway->networkNode.metric;

            LOG_INFO("TX: Seq=%u Value=%.2f to Gateway=%04X (Hops=%u)",
                     data.sequence, data.sensorValue, gatewayAddr, hopCount);

            // Send packet - LoRaMesher will route automatically
            radio.createPacketAndSend(gatewayAddr, &data, 1);

            stats.dataPacketsSent++;
            updateDutyCycle(packetSize);

            // Log transmission
            logPacketTransmit(gatewayAddr, sizeof(SensorPacket), data.sequence);

            // Blink LED
            if (LED_BLINK_ON_TX) {
                digitalWrite(LED_PIN, HIGH);
                delay(50);
                digitalWrite(LED_PIN, LOW);
            }

            nodeStatus.statusMessage = "TX Success";
        } else {
            LOG_WARN("No gateway in routing table yet, waiting...");
            nodeStatus.statusMessage = "No Gateway";
        }
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
    } else {
        LOG_INFO("TX task created successfully");
    }
}

// ============================================================================
// LoRaMesher Setup
// ============================================================================

void setupLoraMesher() {
    LOG_INFO("Initializing LoRaMesher with hop-count routing...");

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
    config.syncWord = LORA_SYNC_WORD;
    config.preambleLength = LORA_PREAMBLE_LENGTH;

    // Initialize LoRaMesher
    radio.begin(config);

    // Create and register receive task
    createReceiveTask();

    // Start radio (this enables automatic routing)
    radio.start();

    // If this is a gateway, set the gateway role
    if (NODE_ROLE == XMESH_ROLE_GATEWAY) {
        radio.addGatewayRole();
        LOG_INFO("Gateway role added - other nodes can discover this gateway");
    }

    LOG_INFO("LoRaMesher initialized with hop-count routing");
    LOG_INFO("Local address: %04X", radio.getLocalAddress());
    LOG_INFO("Frequency: %.1f MHz, SF: %d, BW: %.1f kHz, Power: %d dBm",
             LORA_FREQUENCY, LORA_SPREADING_FACTOR, LORA_BANDWIDTH, LORA_TX_POWER);
    LOG_INFO("Routing table will be built automatically via HELLO packets");
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

    // Initialize node status
    nodeStatus.nodeId = NODE_ID;
    nodeStatus.nodeRole = NODE_ROLE;
    nodeStatus.gatewayAddr = GATEWAY_ADDRESS;
    nodeStatus.statusMessage = "Ready";
}

// ============================================================================
// Display Update
// ============================================================================

void updateNodeStatus() {
    // Packet statistics
    nodeStatus.txPackets = stats.dataPacketsSent;
    nodeStatus.rxPackets = stats.dataPacketsReceived;
    nodeStatus.fwdPackets = stats.dataPacketsForwarded;
    nodeStatus.dropPackets = stats.dataPacketsDropped;

    // Duty cycle
    nodeStatus.dutyCyclePercent = dutyCycle.getCurrentPercentage();
    nodeStatus.airtimeMs = dutyCycle.getCurrentAirtime();

    // System metrics
    nodeStatus.uptimeMs = millis();
    nodeStatus.freeHeap = ESP.getFreeHeap();
    nodeStatus.cpuUsage = 0.0;

    // Link quality (not available from LoRaMesher in Protocol 2)
    nodeStatus.rssi = 0.0;  // LoRaMesher doesn't expose RSSI/SNR easily
    nodeStatus.snr = 0.0;
    nodeStatus.etx = 0.0;  // Not calculated in hop-count routing

    // Routing info
    nodeStatus.routeCount = radio.routingTableSize();

    // Find route to gateway
    if (NODE_ROLE == XMESH_ROLE_GATEWAY) {
        // Gateway is self
        nodeStatus.nextHopAddr = radio.getLocalAddress();
        nodeStatus.routeCost = 0.0;
    } else {
        RouteNode* gateway = radio.getClosestGateway();
        if (gateway != nullptr) {
            nodeStatus.nextHopAddr = gateway->via;
            nodeStatus.routeCost = gateway->networkNode.metric;
        } else {
            nodeStatus.nextHopAddr = 0;
            nodeStatus.routeCost = 0.0;
        }
    }

    // Update status message based on activity
    if (nodeStatus.statusMessage == "TX Success" ||
        nodeStatus.statusMessage == "Packet RX" ||
        nodeStatus.statusMessage == "Received" ||
        nodeStatus.statusMessage == "Ready") {
        // Keep the message for one update cycle, then change to Idle
        static unsigned long lastStatusChange = 0;
        if (millis() - lastStatusChange > 2000) {
            nodeStatus.statusMessage = "Idle";
        }
    }
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

    // Check duty cycle
    if (dutyCycle.getCurrentPercentage() > DUTY_CYCLE_WARNING_PCT * 100) {
        LOG_WARN("Duty cycle warning: %.2f%%", dutyCycle.getCurrentPercentage());
    }
}

// ============================================================================
// Routing Table Debug
// ============================================================================

void printRoutingTable() {
    if (!LOG_ROUTING_TABLE || CSV_OUTPUT) {
        return;
    }

    LOG_INFO("==== Routing Table ====");
    LOG_INFO("Size: %d entries", radio.routingTableSize());

    // Access routing table via RoutingTableService
    LM_LinkedList<RouteNode>* routingTable = RoutingTableService::routingTableList;
    if (routingTable && routingTable->moveToStart()) {
        LOG_INFO("Addr   Via    Hops  Role");
        LOG_INFO("------|------|------|----");
        do {
            RouteNode* node = routingTable->getCurrent();
            LOG_INFO("%04X | %04X | %4d | %02X",
                     node->networkNode.address,
                     node->via,
                     node->networkNode.metric,
                     node->networkNode.role);
        } while (routingTable->next());
    } else {
        LOG_INFO("(empty)");
    }
    LOG_INFO("=======================");
}

// ============================================================================
// Arduino Setup and Loop
// ============================================================================

void setup() {
    // Initialize serial logging
    logger.begin(SERIAL_BAUD, CSV_OUTPUT);
    logger.setLevel((LogLevel)LOG_LEVEL);

    LOG_INFO("===========================================");
    LOG_INFO("xMESH Protocol 2 - Hop-Count Routing");
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

    // Print routing table periodically
    if (millis() - lastRoutingTablePrint >= 30000) {
        printRoutingTable();
        lastRoutingTablePrint = millis();
    }

    // Handle button press for page switching
    if (digitalRead(PRG_BUTTON) == LOW) {
        displayManager.nextPage();
        delay(200); // Debounce
    }

    delay(10);
}
