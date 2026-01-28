#include <Arduino.h>
#include "LoraMesher.h"
#include "config.h"

#include <xmesh/TrickleScheduler.h>
#include <xmesh/CostRouter.h>
#include <xmesh/ETXTracker.h>
#include <xmesh/GatewayBalancer.h>

xmesh::TrickleScheduler trickle(TRICKLE_I_MIN, TRICKLE_I_MAX, TRICKLE_K, TRICKLE_ENABLED);
xmesh::CostRouter costRouter(W1_HOP_COUNT, W2_RSSI, W3_SNR, W4_ETX, W5_GATEWAY_BIAS);
xmesh::ETXTracker etxTracker;
xmesh::GatewayBalancer gatewayBalancer;

uint32_t lastMonitorCheck = 0;
constexpr uint32_t MONITOR_INTERVAL_MS = 60000;

float costCalculationCB(uint8_t hops, uint16_t nextHop, uint16_t destAddr) {
    int16_t rssi = -70;
    int8_t snr = 5;
    
    xmesh::LinkMetrics* metrics = etxTracker.getLinkMetrics(nextHop);
    if (metrics != nullptr) {
        rssi = metrics->rssi;
        snr = metrics->snr;
    }
    
    float etx = (metrics != nullptr) ? metrics->etx : ETX_DEFAULT;
    
    uint8_t encodedLoad = 255;
    float gatewayBias = gatewayBalancer.getGatewayBias(destAddr, encodedLoad);
    
    return costRouter.calculateCost(hops, nextHop, destAddr, rssi, snr, etx, gatewayBias);
}

void helloReceivedCB(uint16_t srcAddr) {
    trickle.onHelloReceived();
    
    gatewayBalancer.updateNeighborHealth(srcAddr);
    
    int8_t receivedSNR = LoraMesher.getReceivedSNR();
    int16_t estimatedRSSI = static_cast<int16_t>(receivedSNR * 4 - 110);
    
    static uint32_t helloSeqNum = 0;
    helloSeqNum++;
    
    etxTracker.updateLinkMetrics(srcAddr, estimatedRSSI, receivedSNR, helloSeqNum);
}

void processReceivedPackets(void*) {
    for (;;) {
        ulTaskNotifyTake(pdPASS, portMAX_DELAY);
        
        while (LoraMesher.getReceivedQueueSize() > 0) {
            Serial.printf("[xMESH] Processing received packet\n");
            
            AppPacket<uint8_t>* packet = LoraMesher.getNextAppPacket<uint8_t>();
            
            Serial.printf("[xMESH] Received %d bytes from %04X\n", 
                         packet->payloadSize, packet->src);
            
            LoraMesher.deletePacket(packet);
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
    
    Serial.printf("\n\n");
    Serial.printf("====================================\n");
    Serial.printf("  xMESH Production Firmware\n");
    Serial.printf("  Modular LoRa Mesh Network\n");
    Serial.printf("====================================\n");
    Serial.printf("Node Address: %04X\n", NODE_ADDRESS);
    Serial.printf("Gateway Mode: %s\n", IS_GATEWAY_NODE ? "YES" : "NO");
    Serial.printf("Trickle: I_min=%lus, I_max=%lus, k=%d\n", 
                  TRICKLE_I_MIN/1000, TRICKLE_I_MAX/1000, TRICKLE_K);
    Serial.printf("====================================\n\n");
    
    gatewayBalancer.setIsGateway(IS_GATEWAY_NODE);
    
    trickle.start();
    Serial.printf("[Trickle] Started with interval: %.1fs\n", 
                  trickle.getCurrentIntervalSec());
    
    LoraMesher& radio = LoraMesher::getInstance();
    radio.begin();
    
    RoutingTableService::setCostCalculationCallback(costCalculationCB);
    RoutingTableService::setHelloReceivedCallback(helloReceivedCB);
    Serial.printf("[LoRaMesher] Callbacks registered\n");
    
    createReceiveMessagesTask();
    radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);
    
    radio.start();
    Serial.printf("[LoRaMesher] Radio started\n");
    
    Serial.printf("\n[xMESH] Initialization complete. Entering main loop...\n\n");
}

void loop() {
    if (trickle.shouldTransmit()) {
        Serial.printf("[Trickle] Transmitting HELLO (interval: %.1fs, TX: %lu, Suppressed: %lu)\n",
                     trickle.getCurrentIntervalSec(),
                     trickle.getTransmitCount(),
                     trickle.getSuppressCount());
        
        LoraMesher.sendHello();
    }
    
    uint32_t now = millis();
    if (now - lastMonitorCheck >= MONITOR_INTERVAL_MS) {
        lastMonitorCheck = now;
        
        uint8_t failedNeighbors = gatewayBalancer.monitorNeighborHealth();
        if (failedNeighbors > 0) {
            Serial.printf("[GatewayBalancer] Detected %d failed neighbors\n", failedNeighbors);
        }
        
        Serial.printf("[Status] Tracked Links: %d, Neighbors: %d\n",
                     etxTracker.getNumTrackedLinks(),
                     gatewayBalancer.getNeighborCount());
        
        RoutingTableService::printRoutingTable();
    }
    
    delay(1000);
}
