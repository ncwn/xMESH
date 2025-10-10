/**
 * @file main.cpp
 * @brief xMESH Hop-Count Routing Protocol - Baseline 2
 * 
 * This firmware uses LoRaMesher's built-in hop-count routing protocol.
 * Unlike flooding, LoRaMesher automatically:
 * - Maintains routing tables via HELLO packets
 * - Selects shortest path based on hop count
 * - Forwards packets only to next hop (not broadcast)
 * 
 * Key Features:
 * - Role-based behavior (Sensor/Router/Gateway)
 * - Automatic route discovery and maintenance
 * - RSSI/SNR logging for link quality analysis
 * - OLED display with node stats
 * 
 * Hardware: Heltec WiFi LoRa32 V3 (ESP32-S3 + SX1262)
 * 
 * @author xMESH Research Project
 * @date 2025-10-10
 */

#include <Arduino.h>
#include "LoraMesher.h"
#include "entities/routingTable/RouteNode.h"
#include "services/RoutingTableService.h"
#include "display.h"
#include "../../common/heltec_v3_config.h"

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
 * @brief Update Line 3: Protocol info
 */
void updateDisplayLine3() {
    Screen.changeLineThree("HOP-CNT");
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

            // Log received packet
            // TODO: Add RSSI/SNR logging once we find the correct API
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

    // Set TX power - reduce for sensor/gateway to force multi-hop routing
    // Router uses full power (14 dBm) to ensure it can reach both ends
#ifdef XMESH_ROLE_ROUTER
    config.power = 14;  // Router: Full power to bridge sensor and gateway
#else
    config.power = -3;  // Sensor/Gateway: Minimum power (-3 dBm) to force routing via router
#endif
    Serial.printf("TX Power: %d dBm\n", config.power);

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

    Serial.println("LoRaMesher initialized with hop-count routing");
    Serial.printf("Local address: %04X\n", radio.getLocalAddress());
    Serial.println("Routing table will be built automatically via HELLO packets");
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
            
            radio.createPacketAndSend(gatewayAddr, &data, 1);
            txCount++;
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
    Serial.println("xMESH Hop-Count Routing Protocol");
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
    // Main loop only handles display updates and routing table debugging
    Screen.drawDisplay();
    
    // Print routing table every 30 seconds for debugging
    static uint32_t lastRoutingTablePrint = 0;
    if (millis() - lastRoutingTablePrint > 30000) {
        lastRoutingTablePrint = millis();
        Serial.println("\n==== Routing Table ====");
        Serial.printf("Routing table size: %d\n", radio.routingTableSize());
        
        // Manually iterate and print routing table entries
        LM_LinkedList<RouteNode>* routingTable = RoutingTableService::routingTableList;
        if (routingTable && routingTable->moveToStart()) {
            Serial.println("Addr   Via    Hops  Role");
            Serial.println("------|------|------|----");
            do {
                RouteNode* node = routingTable->getCurrent();
                Serial.printf("%04X | %04X | %4d | %02X\n",
                             node->networkNode.address,
                             node->via,
                             node->networkNode.metric,
                             node->networkNode.role);
            } while (routingTable->next());
        } else {
            Serial.println("(empty)");
        }
        Serial.println("=======================\n");
    }
    
    delay(100);
}
