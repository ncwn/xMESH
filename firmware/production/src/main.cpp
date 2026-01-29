#include <Arduino.h>
#include "LoraMesher.h"
#include "config.h"
#include <esp_task_wdt.h>
#include <esp_log.h>

#include <xmesh/TrickleScheduler.h>
#include <xmesh/CostRouter.h>
#include <xmesh/ETXTracker.h>
#include <xmesh/GatewayBalancer.h>

static const char* TAG = "MAIN";

constexpr uint32_t WATCHDOG_TIMEOUT_SEC = 30;

xmesh::TrickleScheduler trickle(TRICKLE_I_MIN, TRICKLE_I_MAX, TRICKLE_K, TRICKLE_ENABLED);
xmesh::CostRouter costRouter(W1_HOP_COUNT, W2_RSSI, W3_SNR, W4_ETX, W5_GATEWAY_BIAS);
xmesh::ETXTracker etxTracker;
xmesh::GatewayBalancer gatewayBalancer;

uint32_t lastMonitorCheck = 0;
constexpr uint32_t MONITOR_INTERVAL_MS = 60000;

struct TestPacket {
    uint32_t counter;
};

TestPacket* testPacket = new TestPacket;
uint32_t packetCounter = 0;

void processReceivedPackets(void*) {
    for (;;) {
        ulTaskNotifyTake(pdPASS, portMAX_DELAY);
        
        LoraMesher& radio = LoraMesher::getInstance();
        while (radio.getReceivedQueueSize() > 0) {
            Serial.printf("[xMESH] Processing received packet\n");
            
            AppPacket<TestPacket>* packet = radio.getNextAppPacket<TestPacket>();
            
            Serial.printf("[xMESH] Received counter=%lu from %04X\n", 
                         packet->payload->counter, packet->src);
            
            trickle.onHelloReceived();
            gatewayBalancer.updateNeighborHealth(packet->src);
            
            radio.deletePacket(packet);
        }
    }
}

TaskHandle_t receiveLoRaMessage_Handle = NULL;

void createReceiveMessagesTask() {
    int res = xTaskCreate(
        processReceivedPackets,
        "Receive App Task",
        4096,
        (void*) 1,
        2,
        &receiveLoRaMessage_Handle);
    
    if (res != pdPASS) {
        Serial.printf("[ERROR] Receive App Task creation failed: %d\n", res);
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);
    
    ESP_LOGI(TAG, "Initializing watchdog (timeout: %lu seconds)", WATCHDOG_TIMEOUT_SEC);
    esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);
    ESP_LOGI(TAG, "Watchdog initialized and added to main task");
    
    Serial.printf("\n\n");
    Serial.printf("====================================\n");
    Serial.printf("  xMESH Production Firmware\n");
    Serial.printf("  Modular LoRa Mesh Network\n");
    Serial.printf("====================================\n");
    Serial.printf("Trickle: I_min=%lus, I_max=%lus, k=%d\n", 
                  TRICKLE_I_MIN/1000, TRICKLE_I_MAX/1000, TRICKLE_K);
    Serial.printf("====================================\n\n");
    
    gatewayBalancer.setIsGateway(IS_GATEWAY_NODE);
    
    trickle.start();
    Serial.printf("[Trickle] Started with interval: %.1fs\n", 
                  trickle.getCurrentIntervalSec());
    
    LoraMesher& radio = LoraMesher::getInstance();
    
    // Configure LoRaMesher with Heltec WiFi LoRa 32 V3 pins (SX1262)
    LoraMesher::LoraMesherConfig config = LoraMesher::LoraMesherConfig();
    config.loraCs = 8;      // SS pin
    config.loraRst = 12;    // RST_LoRa  
    config.loraIrq = 14;    // DIO1 (interrupt)
    config.loraIo1 = 13;    // BUSY_LoRa (gpio for SX1262)
    config.module = LoraMesher::LoraModules::SX1262_MOD;
    
    radio.begin(config);
    
    Serial.printf("[LoRaMesher] Initialized with address: %04X\n", radio.getLocalAddress());
    
    createReceiveMessagesTask();
    radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);
    
    radio.start();
    Serial.printf("[LoRaMesher] Radio started\n");
    
    Serial.printf("\n[xMESH] Initialization complete. Entering main loop...\n\n");
    ESP_LOGI(TAG, "System initialized successfully");
}

void loop() {
    esp_task_wdt_reset();
    
    if (trickle.shouldTransmit()) {
        Serial.printf("[Trickle] Transmitting (interval: %.1fs, TX: %lu, Suppressed: %lu)\n",
                     trickle.getCurrentIntervalSec(),
                     trickle.getTransmitCount(),
                     trickle.getSuppressCount());
        
        LoraMesher& radio = LoraMesher::getInstance();
        
        testPacket->counter = packetCounter++;
        radio.createPacketAndSend(BROADCAST_ADDR, testPacket, 1);
    }
    
    uint32_t now = millis();
    if (now - lastMonitorCheck >= MONITOR_INTERVAL_MS) {
        lastMonitorCheck = now;
        
        uint8_t failedNeighbors = gatewayBalancer.monitorNeighborHealth();
        if (failedNeighbors > 0) {
            Serial.printf("[GatewayBalancer] Detected %d failed neighbors\n", failedNeighbors);
        }
        
        LoraMesher& radio = LoraMesher::getInstance();
        Serial.printf("[Status] Tracked Links: %d, Neighbors: %d, Routing Table Size: %d\n",
                     etxTracker.getNumTrackedLinks(),
                     gatewayBalancer.getNeighborCount(),
                     radio.routingTableSize());
    }
    
    delay(1000);
}
