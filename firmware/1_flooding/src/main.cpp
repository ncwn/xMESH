/**
 * @file main.cpp
 * @brief xMESH Flooding Protocol - Baseline Implementation
 * 
 * Simple flooding protocol where all nodes rebroadcast received packets
 * with duplicate detection to prevent infinite loops.
 * 
 * Features:
 * - Role-based behavior (Sensor/Router/Gateway)
 * - Duplicate detection with 5-entry cache
 * - Packet sequence numbers
 * - OLED display with node stats
 * 
 * Hardware: Heltec WiFi LoRa32 V3 (ESP32-S3 + SX1262)
 * 
 * @author xMESH Research Project
 * @date 2025-10-10
 */

#include <Arduino.h>
#include "LoraMesher.h"
#include "display.h"
#include "../../common/heltec_v3_config.h"

// LED Control
#define BOARD_LED   LED_PIN
#define LED_ON      HIGH
#define LED_OFF     LOW

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
    uint32_t seqNum;        // Sequence number for duplicate detection
    uint16_t srcAddr;       // Original source address
    uint32_t timestamp;     // Timestamp (milliseconds since boot)
    float sensorValue;      // Simulated sensor data
    uint8_t hopCount;       // Number of hops from source
};

// ============================================================================
// Duplicate Detection Cache
// ============================================================================

#define CACHE_SIZE 5

struct PacketID {
    uint16_t srcAddr;
    uint32_t seqNum;
};

PacketID duplicateCache[CACHE_SIZE];
uint8_t cacheIndex = 0;

/**
 * @brief Check if packet is duplicate using circular cache
 * 
 * @param srcAddr Source address
 * @param seqNum Sequence number
 * @return true if packet is duplicate
 * @return false if packet is new
 */
bool isDuplicate(uint16_t srcAddr, uint32_t seqNum) {
    for (uint8_t i = 0; i < CACHE_SIZE; i++) {
        if (duplicateCache[i].srcAddr == srcAddr && duplicateCache[i].seqNum == seqNum) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Add packet to duplicate detection cache
 * 
 * @param srcAddr Source address
 * @param seqNum Sequence number
 */
void addToCache(uint16_t srcAddr, uint32_t seqNum) {
    duplicateCache[cacheIndex].srcAddr = srcAddr;
    duplicateCache[cacheIndex].seqNum = seqNum;
    cacheIndex = (cacheIndex + 1) % CACHE_SIZE;
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
 * @brief Update Line 3: Protocol info
 */
void updateDisplayLine3() {
    Screen.changeLineThree("FLOOD");
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
 * @brief Process received packets (flooding logic)
 * 
 * This task is notified by LoRaMesher when packets arrive.
 * It implements flooding with duplicate detection:
 * 1. Check if packet is duplicate (skip if yes)
 * 2. Add to cache
 * 3. Process packet based on role
 * 4. Rebroadcast packet (except for gateways)
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
            
            // Check for duplicate
            if (isDuplicate(data->srcAddr, data->seqNum)) {
                Serial.printf("DUPLICATE: Packet %lu from %04X (already seen)\n", 
                             data->seqNum, data->srcAddr);
                radio.deletePacket(packet);
                continue;
            }

            // Add to cache
            addToCache(data->srcAddr, data->seqNum);
            
            // Increment hop count
            data->hopCount++;

            // Update RX counter and display
            rxCount++;
            updateDisplayLine2();

            // Log received packet
            Serial.printf("RX: Seq=%lu From=%04X Hops=%d Value=%.2f\n", 
                         data->seqNum, data->srcAddr, data->hopCount, data->sensorValue);

            // Gateway behavior: Log packet (don't rebroadcast)
            if (IS_GATEWAY) {
                Serial.printf("GATEWAY: Packet %lu from %04X received (hops=%d, value=%.2f)\n",
                             data->seqNum, data->srcAddr, data->hopCount, data->sensorValue);
            }
            // Router/Sensor behavior: Rebroadcast packet (flooding)
            else {
                Serial.printf("FLOOD: Rebroadcasting packet %lu from %04X\n", 
                             data->seqNum, data->srcAddr);
                
                // Broadcast to all neighbors
                radio.createPacketAndSend(BROADCAST_ADDR, data, 1);
                txCount++;
                updateDisplayLine2();
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
    Serial.println("Initializing LoRaMesher...");

    // Get default configuration
    LoraMesher::LoraMesherConfig config = LoraMesher::LoraMesherConfig();

    // Configure Heltec V3 pins (from heltec_v3_config.h)
    config.loraCs = LORA_CS;
    config.loraRst = LORA_RST;
    config.loraIrq = LORA_DIO1;
    config.loraIo1 = LORA_BUSY;

    // Set LoRa module type (SX1262)
    config.module = LoraMesher::LoraModules::SX1262_MOD;

    // Initialize LoRaMesher
    radio.begin(config);

    // Create and register receive task
    createReceiveMessages();
    radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);

    // Start radio
    radio.start();

    Serial.println("LoRaMesher initialized");
    Serial.printf("Local address: %04X\n", radio.getLocalAddress());
}

// ============================================================================
// Sensor Task (periodic data transmission)
// ============================================================================

/**
 * @brief Sensor task: Send periodic data packets
 * 
 * Sensors send packets every 60 seconds as per xMESH specification
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

        // Broadcast packet
        Serial.printf("TX: Seq=%lu Value=%.2f\n", data.seqNum, data.sensorValue);
        radio.createPacketAndSend(BROADCAST_ADDR, &data, 1);
        
        txCount++;
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
    Serial.println("xMESH Flooding Protocol");
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

    // Initialize LoRaMesher
    setupLoraMesher();

    // Create transmission task (sensors only)
    createSendMessages();

    Serial.println("Setup complete\n");
}

void loop() {
    // Main loop only handles display updates
    Screen.drawDisplay();
}
