/**
 * @file trickle_hello.h
 * @brief Trickle-controlled HELLO packet transmission for Protocol 3
 *
 * This replaces LoRaMesher's fixed-interval HELLO task with an adaptive
 * Trickle timer implementation (RFC 6206).
 *
 * Strategy: Suspend LoRaMesher's HELLO task and run our own.
 */

#ifndef TRICKLE_HELLO_H
#define TRICKLE_HELLO_H

#include <Arduino.h>
#include "LoraMesher.h"
#include "services/PacketService.h"
#include "services/RoutingTableService.h"
#include "services/RoleService.h"
#include "config.h"

// Forward declarations
extern TrickleTimer trickleTimer;
extern LoraMesher& radio;

// Forward declaration for link metric update (defined in main.cpp)
void updateLinkMetricsFromHello(uint16_t fromAddr);
uint8_t sampleLocalGatewayLoadForHello();

/**
 * @brief Trickle-controlled HELLO packet task
 *
 * Replaces LoRaMesher's fixed 120s HELLO with adaptive 60-600s intervals.
 *
 * Algorithm:
 * 1. Wait for Trickle timer to say "send now"
 * 2. Create RoutePacket with current routing table
 * 3. Send packet via LoRaMesher
 * 4. Repeat
 */
void trickleHelloTask(void* parameter) {
    Serial.println("[TrickleHELLO] Task started - replacing LoRaMesher fixed HELLO");

    // Wait for LoRaMesher to fully initialize
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    size_t maxNodesPerPacket = (PacketFactory::getMaxPacketSize() - sizeof(RoutePacket)) / sizeof(NetworkNode);
    Serial.printf("[TrickleHELLO] Max nodes per packet: %d\n", maxNodesPerPacket);

    uint32_t lastCheck = 0;
    uint32_t lastActualTransmit = 0;  // Track last HELLO transmission for safety mechanism
    const uint32_t SAFETY_HELLO_INTERVAL = 180000;  // 3 minutes max between HELLOs (OPTIMIZED for faster fault detection)

    for (;;) {
        uint32_t now = millis();

        // Check Trickle timer every second
        if (now - lastCheck >= 1000) {
            lastCheck = now;

            // Safety mechanism: Force HELLO if haven't sent in 3 minutes
            // Provides 420s buffer before library timeout (DEFAULT_TIMEOUT = 600s)
            // Enables detection in 180-360s (miss 2 safety HELLOs)
            bool safetySend = false;
            if (lastActualTransmit > 0 && (now - lastActualTransmit) > SAFETY_HELLO_INTERVAL) {
                safetySend = true;
                Serial.printf("[TrickleHELLO] SAFETY HELLO (forced) - %lu ms since last TX\n",
                             now - lastActualTransmit);
            }

            // Ask Trickle: should we transmit now? (OR safety override)
            if (trickleTimer.shouldTransmit() || safetySend) {
                if (!safetySend) {
                    Serial.printf("[TrickleHELLO] Sending HELLO - interval=%.1fs\n",
                                 trickleTimer.getCurrentIntervalSec());
                }

                lastActualTransmit = now;  // Update transmission timestamp
                uint8_t localGatewayLoad = sampleLocalGatewayLoadForHello();

                // Get current routing table
                NetworkNode* nodes = RoutingTableService::getAllNetworkNodes();
                size_t numOfNodes = RoutingTableService::routingTableSize();

                // Calculate number of packets needed
                size_t numPackets = (numOfNodes + maxNodesPerPacket - 1) / maxNodesPerPacket;
                numPackets = (numPackets == 0) ? 1 : numPackets;

                // Send HELLO packet(s)
                for (size_t i = 0; i < numPackets; ++i) {
                    size_t startIndex = i * maxNodesPerPacket;
                    size_t endIndex = startIndex + maxNodesPerPacket;
                    if (endIndex > numOfNodes) {
                        endIndex = numOfNodes;
                    }

                    size_t nodesInThisPacket = endIndex - startIndex;

                    // Create routing packet (same as LoRaMesher does)
                    RoutePacket* tx = PacketService::createRoutingPacket(
                        radio.getLocalAddress(),
                        &nodes[startIndex],
                        nodesInThisPacket,
                        RoleService::getRole()
                    );
                    tx->gatewayLoad = localGatewayLoad;

                    // Queue for transmission (uses setPackedForSend via LM_GOD_MODE)
                    // LM_GOD_MODE makes private methods accessible for custom implementations
                    radio.setPackedForSend(
                        reinterpret_cast<Packet<uint8_t>*>(tx),
                        DEFAULT_PRIORITY + 4
                    );

                    // Note: Don't delete tx - setPackedForSend takes ownership
                }

                // Clean up nodes array
                if (numOfNodes > 0)
                    delete[] nodes;
            }
        }

        // Don't hog CPU
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Initialize Trickle HELLO system
 *
 * Call this AFTER radio.start() in setup().
 * It will suspend LoRaMesher's HELLO task and start our Trickle-controlled one.
 */
void initTrickleHello() {
    Serial.println("[TrickleHELLO] Initializing Trickle-controlled HELLO system");

    // Give LoRaMesher time to create its tasks
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Suspend LoRaMesher's fixed-interval HELLO task
    // We need to access the task handle - it's part of LoraMesher singleton
    // Workaround: The task was created by LoRaMesher::initializeSchedulers()
    // We can find it by name
    TaskHandle_t lorameshHelloTask = xTaskGetHandle("Hello routine");

    if (lorameshHelloTask != NULL) {
        vTaskSuspend(lorameshHelloTask);
        Serial.println("[TrickleHELLO] ✅ Suspended LoRaMesher's fixed 120s HELLO task");
    } else {
        Serial.println("[TrickleHELLO] ⚠️  Could not find LoRaMesher HELLO task to suspend");
    }

    // Create our Trickle-controlled HELLO task
    TaskHandle_t trickleTask = NULL;
    BaseType_t result = xTaskCreate(
        trickleHelloTask,
        "Trickle HELLO",
        4096,
        NULL,
        4,  // Same priority as original HELLO task
        &trickleTask
    );

    if (result == pdPASS) {
        Serial.println("[TrickleHELLO] ✅ Started Trickle HELLO task (60-600s adaptive)");
    } else {
        Serial.println("[TrickleHELLO] ❌ Failed to create Trickle HELLO task!");
    }
}

/**
 * @brief Call this when receiving a HELLO packet
 *
 * Notifies Trickle for suppression AND updates link metrics for bidirectional support.
 *
 * @param fromAddr Address of node that sent the HELLO
 */
void onHelloReceived(uint16_t fromAddr) {
    // 1. Trickle suppression feedback
    trickleTimer.heardConsistent();

    // 2. Update link metrics for bidirectional support
    // Calls main.cpp function (has access to getLinkMetrics/updateETX)
    updateLinkMetricsFromHello(fromAddr);

    // 3. Update neighbor health tracking (FAST fault detection)
    // Resets missed HELLO counter, enables 180-360s detection
    updateNeighborHealth(fromAddr);
}

#endif // TRICKLE_HELLO_H
